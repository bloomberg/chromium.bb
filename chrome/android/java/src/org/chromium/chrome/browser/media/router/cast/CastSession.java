// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router.cast;

import android.content.Context;
import android.os.Handler;
import android.support.v4.util.ArrayMap;
import android.util.SparseArray;

import com.google.android.gms.cast.ApplicationMetadata;
import com.google.android.gms.cast.Cast;
import com.google.android.gms.cast.CastDevice;
import com.google.android.gms.cast.MediaStatus;
import com.google.android.gms.cast.RemoteMediaPlayer;
import com.google.android.gms.common.api.GoogleApiClient;
import com.google.android.gms.common.api.ResultCallback;
import com.google.android.gms.common.api.Status;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.media.ui.MediaNotificationInfo;
import org.chromium.chrome.browser.media.ui.MediaNotificationListener;
import org.chromium.chrome.browser.media.ui.MediaNotificationManager;
import org.chromium.chrome.browser.tab.Tab;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.IOException;
import java.util.ArrayDeque;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Queue;
import java.util.Set;

/**
 * A wrapper around the established Cast application session.
 */
public class CastSession implements MediaNotificationListener {
    private static final String TAG = "MediaRouter";

    private static final String MEDIA_NAMESPACE = "urn:x-cast:com.google.cast.media";
    private static final String GAMES_NAMESPACE = "urn:x-cast:com.google.cast.games";

    private static final String MEDIA_MESSAGE_TYPES[] = {
            "PLAY",
            "LOAD",
            "PAUSE",
            "SEEK",
            "STOP_MEDIA",
            "MEDIA_SET_VOLUME",
            "MEDIA_GET_STATUS",
            "EDIT_TRACKS_INFO",
            "QUEUE_LOAD",
            "QUEUE_INSERT",
            "QUEUE_UPDATE",
            "QUEUE_REMOVE",
            "QUEUE_REORDER",
    };

    private static final String MEDIA_SUPPORTED_COMMANDS[] = {
            "pause",
            "seek",
            "stream_volume",
            "stream_mute",
    };

    // Sequence number used when no sequence number is required or was initially passed.
    private static final int INVALID_SEQUENCE_NUMBER = -1;

    // Map associating types that have a different names outside of the media namespace and inside.
    // In other words, some types are sent as MEDIA_FOO or FOO_MEDIA by the client by the Cast
    // expect them to be named FOO. The reason being that FOO might exist in multiple namespaces
    // but the client isn't aware of namespacing.
    private static Map<String, String> sMediaOverloadedMessageTypes;

    // Lock used to lazy initialize sMediaOverloadedMessageTypes.
    private static final Object INIT_LOCK = new Object();

    // The value is borrowed from the Android Cast SDK code to match their behavior.
    private static final double MIN_VOLUME_LEVEL_DELTA = 1e-7;

    private static class RequestRecord {
        public final String clientId;
        public final int sequenceNumber;

        public RequestRecord(String clientId, int sequenceNumber) {
            this.clientId = clientId;
            this.sequenceNumber = sequenceNumber;
        }
    }

    private static class CastMessagingChannel implements Cast.MessageReceivedCallback {
        private final CastSession mSession;

        public CastMessagingChannel(CastSession session) {
            mSession = session;
        }

        @Override
        public void onMessageReceived(CastDevice castDevice, String namespace, String message) {
            Log.d(TAG, "Received message from Cast device: namespace=\"" + namespace
                       + "\" message=\"" + message + "\"");

            RequestRecord request = null;
            try {
                JSONObject jsonMessage = new JSONObject(message);
                int requestId = jsonMessage.getInt("requestId");
                if (mSession.mRequests.indexOfKey(requestId) >= 0) {
                    request = mSession.mRequests.get(requestId);
                    mSession.mRequests.delete(requestId);
                }
            } catch (JSONException e) {
            }

            if (MEDIA_NAMESPACE.equals(namespace)) {
                mSession.onMediaMessage(message, request);
                return;
            }

            mSession.onAppMessage(message, namespace, request);
        }
    }

    /**
     * Remove 'null' fields from a JSONObject. This method calls itself recursively until all the
     * fields have been looked at.
     * TODO(mlamouri): move to some util class?
     */
    private static void removeNullFields(Object object) throws JSONException {
        if (object instanceof JSONArray) {
            JSONArray array = (JSONArray) object;
            for (int i = 0; i < array.length(); ++i) removeNullFields(array.get(i));
        } else if (object instanceof JSONObject) {
            JSONObject json = (JSONObject) object;
            JSONArray names = json.names();
            if (names == null) return;
            for (int i = 0; i < names.length(); ++i) {
                String key = names.getString(i);
                if (json.isNull(key)) {
                    json.remove(key);
                } else {
                    removeNullFields(json.get(key));
                }
            }
        }
    }

    private final CastMessagingChannel mMessageChannel;
    private final CastMediaRouteProvider mRouteProvider;
    private final CastDevice mCastDevice;
    private final MediaSource mSource;

    private Set<String> mNamespaces = new HashSet<String>();
    private GoogleApiClient mApiClient;
    private String mSessionId;
    private String mApplicationStatus;
    private ApplicationMetadata mApplicationMetadata;
    private boolean mStoppingApplication;
    private MediaNotificationInfo.Builder mNotificationBuilder;
    private RemoteMediaPlayer mMediaPlayer;

    private SparseArray<RequestRecord> mRequests;
    private Queue<RequestRecord> mVolumeRequests;
    private ArrayMap<String, Queue<Integer>> mStopRequests;

    private Handler mHandler;

    /**
     * Initializes a new {@link CastSession} instance.
     * @param apiClient The Google Play Services client used to create the session.
     * @param sessionId The session identifier to use with the Cast SDK.
     * @param origin The origin of the frame requesting the route.
     * @param tabId the id of the tab containing the frame requesting the route.
     * @param source The {@link MediaSource} corresponding to this session.
     * @param routeProvider The {@link CastMediaRouteProvider} instance managing this session.
     */
    public CastSession(
            GoogleApiClient apiClient,
            String sessionId,
            ApplicationMetadata metadata,
            String applicationStatus,
            CastDevice castDevice,
            String origin,
            int tabId,
            MediaSource source,
            CastMediaRouteProvider routeProvider) {
        mApiClient = apiClient;
        mSessionId = sessionId;
        mSource = source;
        mRouteProvider = routeProvider;
        mApplicationMetadata = metadata;
        mApplicationStatus = applicationStatus;
        mCastDevice = castDevice;
        mRequests = new SparseArray<RequestRecord>();
        mVolumeRequests = new ArrayDeque<RequestRecord>();
        mStopRequests = new ArrayMap<String, Queue<Integer>>();
        mHandler = new Handler();

        mMessageChannel = new CastMessagingChannel(this);
        updateNamespaces();

        final Context context = ApplicationStatus.getApplicationContext();

        if (mNamespaces.contains(MEDIA_NAMESPACE)) {
            mMediaPlayer = new RemoteMediaPlayer();
            mMediaPlayer.setOnStatusUpdatedListener(
                    new RemoteMediaPlayer.OnStatusUpdatedListener() {
                        @Override
                        public void onStatusUpdated() {
                            MediaStatus mediaStatus = mMediaPlayer.getMediaStatus();
                            if (mediaStatus == null) return;

                            int playerState = mediaStatus.getPlayerState();
                            if (playerState == MediaStatus.PLAYER_STATE_PAUSED
                                    || playerState == MediaStatus.PLAYER_STATE_PLAYING) {
                                mNotificationBuilder.setPaused(
                                        playerState != MediaStatus.PLAYER_STATE_PLAYING);
                                mNotificationBuilder.setActions(MediaNotificationInfo.ACTION_STOP
                                        | MediaNotificationInfo.ACTION_PLAY_PAUSE);
                            } else {
                                mNotificationBuilder.setActions(MediaNotificationInfo.ACTION_STOP);
                            }
                            MediaNotificationManager.show(context, mNotificationBuilder.build());
                        }
                    });
        }

        mNotificationBuilder = new MediaNotificationInfo.Builder()
                .setTitle(mCastDevice.getFriendlyName())
                .setPaused(false)
                .setOrigin(origin)
                // TODO(avayvod): the same session might have more than one tab id. Should we track
                // the last foreground alive tab and update the notification with it?
                .setTabId(tabId)
                // TODO(avayvod): pass true here if initiated from the incognito mode.
                // MediaRouter is disabled for Incognito mode for now, see https://crbug.com/525215
                .setPrivate(false)
                .setIcon(R.drawable.ic_notification_media_route)
                .setActions(MediaNotificationInfo.ACTION_STOP)
                .setContentIntent(Tab.createBringTabToFrontIntent(tabId))
                .setId(R.id.presentation_notification)
                .setListener(this);
        MediaNotificationManager.show(context, mNotificationBuilder.build());

        synchronized (INIT_LOCK) {
            if (sMediaOverloadedMessageTypes == null) {
                sMediaOverloadedMessageTypes = new HashMap<String, String>();
                sMediaOverloadedMessageTypes.put("STOP_MEDIA", "STOP");
                sMediaOverloadedMessageTypes.put("MEDIA_SET_VOLUME", "SET_VOLUME");
                sMediaOverloadedMessageTypes.put("MEDIA_GET_STATUS", "GET_STATUS");
            }
        }
    }

    /**
     * @return the id of the Cast session controlled by the route.
     */
    public String getSessionId() {
        return mSessionId;
    }

    public void stopApplication() {
        if (mStoppingApplication) return;

        if (isApiClientInvalid()) return;

        mStoppingApplication = true;
        Cast.CastApi.stopApplication(mApiClient, mSessionId)
                .setResultCallback(new ResultCallback<Status>() {
                    @Override
                    public void onResult(Status status) {
                        for (String clientId : mRouteProvider.getClients()) {
                            Queue<Integer> sequenceNumbersForClient = mStopRequests.get(clientId);
                            if (sequenceNumbersForClient == null) {
                                sendClientMessageTo(clientId, "remove_session", mSessionId,
                                        INVALID_SEQUENCE_NUMBER);
                                continue;
                            }

                            for (int sequenceNumber : sequenceNumbersForClient) {
                                sendClientMessageTo(
                                        clientId, "remove_session", mSessionId, sequenceNumber);
                            }
                            mStopRequests.remove(clientId);
                        }

                        // TODO(avayvod): handle a failure to stop the application.
                        // https://crbug.com/535577

                        for (String namespace : mNamespaces) unregisterNamespace(namespace);
                        mNamespaces.clear();

                        mSessionId = null;
                        mApiClient = null;

                        mRouteProvider.onSessionClosed();
                        mStoppingApplication = false;

                        MediaNotificationManager.clear(R.id.presentation_notification);
                    }
                });
    }

    public String getSourceId() {
        return mSource.getUrn();
    }

    public String getSinkId() {
        return mCastDevice.getDeviceId();
    }

    public void onClientConnected(String clientId) {
        sendClientMessageTo(
                clientId, "new_session", buildSessionMessage(), INVALID_SEQUENCE_NUMBER);

        if (mMediaPlayer != null && !isApiClientInvalid()) mMediaPlayer.requestStatus(mApiClient);
    }

    /////////////////////////////////////////////////////////////////////////////////////////////
    // MediaNotificationListener implementation.

    @Override
    public void onPlay(int actionSource) {
        if (mMediaPlayer == null || isApiClientInvalid()) return;

        mMediaPlayer.play(mApiClient);
    }

    @Override
    public void onPause(int actionSource) {
        if (mMediaPlayer == null || isApiClientInvalid()) return;

        mMediaPlayer.pause(mApiClient);
    }

    @Override
    public void onStop(int actionSource) {
        stopApplication();
        mRouteProvider.onSessionStopAction();
    }


    /**
     * Forwards the media message to the page via the media router.
     * The MEDIA_STATUS message needs to be sent to all the clients.
     * @param message The media that's being send by the receiver.
     * @param request The information about the client and the sequence number to respond with.
     */
    private void onMediaMessage(String message, RequestRecord request) {
        if (mMediaPlayer != null) {
            mMediaPlayer.onMessageReceived(mCastDevice, MEDIA_NAMESPACE, message);
        }

        if (isMediaStatusMessage(message)) {
            // MEDIA_STATUS needs to be sent to all the clients.
            for (String clientId : mRouteProvider.getClients()) {
                if (request != null && clientId.equals(request.clientId)) continue;

                sendClientMessageTo(
                        clientId, "v2_message", message, INVALID_SEQUENCE_NUMBER);
            }
        }
        if (request != null) {
            sendClientMessageTo(request.clientId, "v2_message", message, request.sequenceNumber);
        }
    }

    /**
     * Forwards the application specific message to the page via the media router.
     * @param message The message within the namespace that's being send by the receiver.
     * @param namespace The application specific namespace this message belongs to.
     * @param request The information about the client and the sequence number to respond with.
     */
    private void onAppMessage(String message, String namespace, RequestRecord request) {
        try {
            JSONObject jsonMessage = new JSONObject();
            jsonMessage.put("sessionId", mSessionId);
            jsonMessage.put("namespaceName", namespace);
            jsonMessage.put("message", message);
            if (request != null) {
                sendClientMessageTo(request.clientId, "app_message",
                        jsonMessage.toString(), request.sequenceNumber);
            } else {
                broadcastClientMessage("app_message", jsonMessage.toString());
            }
        } catch (JSONException e) {
            Log.e(TAG, "Failed to create the message wrapper", e);
        }
    }

    private boolean isMediaStatusMessage(String message) {
        try {
            JSONObject jsonMessage = new JSONObject(message);
            return "MEDIA_STATUS".equals(jsonMessage.getString("type"));
        } catch (JSONException e) {
            return false;
        }
    }

    private void addNamespace(String namespace) {
        assert !mNamespaces.contains(namespace);

        if (isApiClientInvalid()) return;

        // If application metadata is null, register the callback anyway.
        if (mApplicationMetadata != null && !mApplicationMetadata.isNamespaceSupported(namespace)) {
            return;
        }

        try {
            Cast.CastApi.setMessageReceivedCallbacks(mApiClient, namespace, mMessageChannel);
            mNamespaces.add(namespace);
        } catch (IOException e) {
            Log.e(TAG, "Failed to register namespace listener for %s", namespace, e);
        }
    }

    private void unregisterNamespace(String namespace) {
        assert mNamespaces.contains(namespace);

        if (isApiClientInvalid()) return;

        try {
            Cast.CastApi.removeMessageReceivedCallbacks(mApiClient, namespace);
        } catch (IOException e) {
            Log.e(TAG, "Failed to remove the namespace listener for %s", namespace, e);
        }
    }

    public boolean handleSessionMessage(
            JSONObject message, String messageType) throws JSONException {
        if ("v2_message".equals(messageType)) {
            return handleCastV2Message(message);
        } else if ("app_message".equals(messageType)) {
            return handleAppMessage(message);
        } else {
            Log.e(TAG, "Unsupported message: %s", message);
            return false;
        }
    }

    // An example of the Cast V2 message:
    //    {
    //        "type": "v2_message",
    //        "message": {
    //          "type": "...",
    //          ...
    //        },
    //        "sequenceNumber": 0,
    //        "timeoutMillis": 0,
    //        "clientId": "144042901280235697"
    //    }
    private boolean handleCastV2Message(JSONObject jsonMessage)
            throws JSONException {
        assert "v2_message".equals(jsonMessage.getString("type"));

        String clientId = jsonMessage.getString("clientId");
        if (clientId == null || !mRouteProvider.getClients().contains(clientId)) return false;

        JSONObject jsonCastMessage = jsonMessage.getJSONObject("message");
        String messageType = jsonCastMessage.getString("type");
        int sequenceNumber = jsonMessage.optInt("sequenceNumber", INVALID_SEQUENCE_NUMBER);

        if ("STOP".equals(messageType)) {
            handleStopMessage(clientId, sequenceNumber);
            return true;
        }

        if ("SET_VOLUME".equals(messageType)) {
            return handleVolumeMessage(
                    jsonCastMessage.getJSONObject("volume"), clientId, sequenceNumber);
        }

        if (Arrays.asList(MEDIA_MESSAGE_TYPES).contains(messageType)) {
            if (sMediaOverloadedMessageTypes.containsKey(messageType)) {
                messageType = sMediaOverloadedMessageTypes.get(messageType);
                jsonCastMessage.put("type", messageType);
            }
            return sendJsonCastMessage(jsonCastMessage, MEDIA_NAMESPACE, clientId, sequenceNumber);
        }

        return true;
    }

    private void handleStopMessage(String clientId, int sequenceNumber) {
        Queue<Integer> sequenceNumbersForClient = mStopRequests.get(clientId);
        if (sequenceNumbersForClient == null) {
            sequenceNumbersForClient = new ArrayDeque<Integer>();
            mStopRequests.put(clientId, sequenceNumbersForClient);
        }
        sequenceNumbersForClient.add(sequenceNumber);

        stopApplication();
    }

    // SET_VOLUME messages have a |level| and |muted| properties. One of them is
    // |null| and the other one isn't. |muted| is expected to be a boolean while
    // |level| is a float from 0.0 to 1.0.
    // Example:
    // {
    //   "volume" {
    //     "level": 0.9,
    //     "muted": null
    //   }
    // }
    private boolean handleVolumeMessage(
            JSONObject volume, final String clientId, final int sequenceNumber)
            throws JSONException {
        if (volume == null) return false;

        if (isApiClientInvalid()) return false;

        boolean waitForVolumeChange = false;
        try {
            if (!volume.isNull("muted")) {
                boolean newMuted = volume.getBoolean("muted");
                if (Cast.CastApi.isMute(mApiClient) != newMuted) {
                    Cast.CastApi.setMute(mApiClient, newMuted);
                    waitForVolumeChange = true;
                }
            }
            if (!volume.isNull("level")) {
                double newLevel = volume.getDouble("level");
                double currentLevel = Cast.CastApi.getVolume(mApiClient);
                if (!Double.isNaN(currentLevel)
                        && Math.abs(currentLevel - newLevel) > MIN_VOLUME_LEVEL_DELTA) {
                    Cast.CastApi.setVolume(mApiClient, newLevel);
                    waitForVolumeChange = true;
                }
            }
        } catch (IOException e) {
            Log.e(TAG, "Failed to send volume command: " + e);
            return false;
        }

        // For each successful volume message we need to respond with an empty "v2_message" so the
        // Cast Web SDK can call the success callback of the page.
        // If we expect the volume to change as the result of the command, we're relying on
        // {@link Cast.CastListener#onVolumeChanged} to get called by the Android Cast SDK when the
        // receiver status is updated. We keep the sequence number until then.
        // If the volume doesn't change as the result of the command, we won't get notified by the
        // Android SDK when the status update is received so we respond to the volume message
        // immediately.
        if (waitForVolumeChange) {
            mVolumeRequests.add(new RequestRecord(clientId, sequenceNumber));
        } else {
            // It's usually bad to have request and response on the same call stack so post the
            // response to the Android message loop.
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    sendClientMessageTo(clientId, "v2_message", null, sequenceNumber);
                }
            });
        }

        return true;
    }

    // An example of the Cast application message:
    // {
    //   "type":"app_message",
    //   "message": {
    //     "sessionId":"...",
    //     "namespaceName":"...",
    //     "message": ...
    //   },
    //   "sequenceNumber":0,
    //   "timeoutMillis":3000,
    //   "clientId":"14417311915272175"
    // }
    private boolean handleAppMessage(JSONObject jsonMessage) throws JSONException {
        assert "app_message".equals(jsonMessage.getString("type"));

        String clientId = jsonMessage.getString("clientId");
        if (clientId == null || !mRouteProvider.getClients().contains(clientId)) return false;

        JSONObject jsonAppMessageWrapper = jsonMessage.getJSONObject("message");

        if (!mSessionId.equals(jsonAppMessageWrapper.getString("sessionId"))) return false;

        String namespaceName = jsonAppMessageWrapper.getString("namespaceName");
        if (namespaceName == null || namespaceName.isEmpty()) return false;

        if (!mNamespaces.contains(namespaceName)) return false;

        int sequenceNumber = jsonMessage.optInt("sequenceNumber", INVALID_SEQUENCE_NUMBER);

        Object actualMessageObject = jsonAppMessageWrapper.get("message");
        if (actualMessageObject == null) return false;

        if (actualMessageObject instanceof String) {
            String actualMessage = jsonAppMessageWrapper.getString("message");
            return sendStringCastMessage(actualMessage, namespaceName, clientId, sequenceNumber);
        }

        JSONObject actualMessage = jsonAppMessageWrapper.getJSONObject("message");
        return sendJsonCastMessage(actualMessage, namespaceName, clientId, sequenceNumber);
    }

    private boolean sendJsonCastMessage(
            JSONObject message,
            final String namespace,
            final String clientId,
            final int sequenceNumber) throws JSONException {
        if (isApiClientInvalid()) return false;

        removeNullFields(message);

        // Map the request id to a valid sequence number only.
        if (sequenceNumber != INVALID_SEQUENCE_NUMBER) {
            // If for some reason, there is already a requestId other than 0, it
            // is kept. Otherwise, one is generated. In all cases it's associated with the
            // sequenceNumber passed by the client.
            int requestId = message.optInt("requestId", 0);
            if (requestId == 0) {
                requestId = CastRequestIdGenerator.getNextRequestId();
                message.put("requestId", requestId);
            }
            mRequests.append(requestId, new RequestRecord(clientId, sequenceNumber));
        }

        return sendStringCastMessage(message.toString(), namespace, clientId, sequenceNumber);
    }

    private boolean sendStringCastMessage(
            final String message,
            final String namespace,
            final String clientId,
            final int sequenceNumber) {
        Log.d(TAG, "Sending message to Cast device in namespace %s: %s", namespace, message);

        try {
            Cast.CastApi.sendMessage(mApiClient, namespace, message)
                    .setResultCallback(
                            new ResultCallback<Status>() {
                                @Override
                                public void onResult(Status result) {
                                    if (!result.isSuccess()) {
                                        // TODO(avayvod): should actually report back to the page.
                                        // See https://crbug.com/550445.
                                        Log.e(TAG, "Failed to send the message: " + result);
                                        return;
                                    }

                                    // Media commands wait for the media status update as a result.
                                    if (MEDIA_NAMESPACE.equals(namespace)) return;

                                    // App messages wait for the empty message with the sequence
                                    // number.
                                    sendClientMessageTo(
                                            clientId, "app_message", null, sequenceNumber);
                                }
                            });
        } catch (Exception e) {
            Log.e(TAG, "Exception while sending message", e);
            return false;
        }
        return true;
    }

    /**
     * Modifies the received MediaStatus message to match the format expected by the client.
     */
    private void sanitizeMediaStatusMessage(JSONObject object) throws JSONException {
        object.put("sessionId", mSessionId);

        JSONArray mediaStatus = object.getJSONArray("status");
        for (int i = 0; i < mediaStatus.length(); ++i) {
            JSONObject status = mediaStatus.getJSONObject(i);
            status.put("sessionId", mSessionId);
            if (!status.has("supportedMediaCommands")) continue;

            JSONArray commands = new JSONArray();
            int bitfieldCommands = status.getInt("supportedMediaCommands");
            for (int j = 0; j < 4; ++j) {
                if ((bitfieldCommands & (1 << j)) != 0) {
                    commands.put(MEDIA_SUPPORTED_COMMANDS[j]);
                }
            }

            status.put("supportedMediaCommands", commands);  // Removes current entry.
        }
    }

    private String buildInternalMessage(
            String type, String message, String clientId, int sequenceNumber) {
        JSONObject json = new JSONObject();
        try {
            json.put("type", type);
            json.put("sequenceNumber", sequenceNumber);
            json.put("timeoutMillis", 0);
            json.put("clientId", clientId);

            // TODO(mlamouri): we should have a more reliable way to handle string, null and Object
            // messages.
            if (message == null
                    || "remove_session".equals(type)
                    || "disconnect_session".equals(type)) {
                json.put("message", message);
            } else {
                JSONObject jsonMessage = new JSONObject(message);
                if ("v2_message".equals(type)
                        && "MEDIA_STATUS".equals(jsonMessage.getString("type"))) {
                    sanitizeMediaStatusMessage(jsonMessage);
                }
                json.put("message", jsonMessage);
            }
        } catch (JSONException e) {
            Log.e(TAG, "Failed to build the reply: " + e);
        }

        return json.toString();
    }

    public void updateSessionStatus() {
        if (isApiClientInvalid()) return;

        try {
            mApplicationStatus = Cast.CastApi.getApplicationStatus(mApiClient);
            mApplicationMetadata = Cast.CastApi.getApplicationMetadata(mApiClient);

            updateNamespaces();

            broadcastClientMessage("update_session", buildSessionMessage());
        } catch (IllegalStateException e) {
            Log.e(TAG, "Can't get application status", e);
        }
    }

    public void onVolumeChanged() {
        updateSessionStatus();

        if (mVolumeRequests.isEmpty()) return;

        for (RequestRecord r : mVolumeRequests) {
            sendClientMessageTo(r.clientId, "v2_message", null, r.sequenceNumber);
        }
        mVolumeRequests.clear();
    }

    private String buildSessionMessage() {
        if (isApiClientInvalid()) return "{}";

        try {
            // "volume" is a part of "receiver" initialized below.
            JSONObject jsonVolume = new JSONObject();
            jsonVolume.put("level", Cast.CastApi.getVolume(mApiClient));
            jsonVolume.put("muted", Cast.CastApi.isMute(mApiClient));

            // "receiver" is a part of "message" initialized below.
            JSONObject jsonReceiver = new JSONObject();
            jsonReceiver.put("label", mCastDevice.getDeviceId());
            jsonReceiver.put("friendlyName", mCastDevice.getFriendlyName());
            jsonReceiver.put("capabilities", getCapabilities(mCastDevice));
            jsonReceiver.put("volume", jsonVolume);
            jsonReceiver.put("isActiveInput", Cast.CastApi.getActiveInputState(mApiClient));
            jsonReceiver.put("displayStatus", null);
            jsonReceiver.put("receiverType", "cast");

            JSONObject jsonMessage = new JSONObject();
            jsonMessage.put("sessionId", mSessionId);
            jsonMessage.put("statusText", mApplicationStatus);
            jsonMessage.put("receiver", jsonReceiver);
            jsonMessage.put("namespaces", extractNamespaces());
            jsonMessage.put("media", new JSONArray());
            jsonMessage.put("status", "connected");
            jsonMessage.put("transportId", "web-4");

            if (mApplicationMetadata != null) {
                jsonMessage.put("appId", mApplicationMetadata.getApplicationId());
                jsonMessage.put("displayName", mApplicationMetadata.getName());
            } else {
                jsonMessage.put("appId", mSource.getApplicationId());
                jsonMessage.put("displayName", mCastDevice.getFriendlyName());
            }

            return jsonMessage.toString();
        } catch (JSONException e) {
            Log.w(TAG, "Building session message failed", e);
            return "{}";
        }
    }

    private void broadcastClientMessage(String type, String message) {
        for (String clientId : mRouteProvider.getClients()) {
            sendClientMessageTo(clientId, type, message, INVALID_SEQUENCE_NUMBER);
        }
    }

    private void sendClientMessageTo(
            String clientId, String type, String message, int sequenceNumber) {
        mRouteProvider.onMessage(clientId,
                buildInternalMessage(type, message, clientId, sequenceNumber));
    }

    static JSONArray getCapabilities(CastDevice device) {
        JSONArray jsonCapabilities = new JSONArray();
        if (device.hasCapability(CastDevice.CAPABILITY_AUDIO_IN)) {
            jsonCapabilities.put("audio_in");
        }
        if (device.hasCapability(CastDevice.CAPABILITY_AUDIO_OUT)) {
            jsonCapabilities.put("audio_out");
        }
        if (device.hasCapability(CastDevice.CAPABILITY_VIDEO_IN)) {
            jsonCapabilities.put("video_in");
        }
        if (device.hasCapability(CastDevice.CAPABILITY_VIDEO_OUT)) {
            jsonCapabilities.put("video_out");
        }
        return jsonCapabilities;
    }

    private JSONArray extractNamespaces() throws JSONException {
        JSONArray jsonNamespaces = new JSONArray();
        for (String namespace : mNamespaces) {
            JSONObject jsonNamespace = new JSONObject();
            jsonNamespace.put("name", namespace);
            jsonNamespaces.put(jsonNamespace);
        }
        return jsonNamespaces;
    }

    private void updateNamespaces() {
        if (mApplicationMetadata == null) return;

        List<String> newNamespaces = mApplicationMetadata.getSupportedNamespaces();

        Set<String> toRemove = new HashSet<String>(mNamespaces);
        toRemove.removeAll(newNamespaces);
        for (String namespaceToRemove : toRemove) unregisterNamespace(namespaceToRemove);

        for (String newNamespace : newNamespaces) {
            if (!mNamespaces.contains(newNamespace)) addNamespace(newNamespace);
        }
    }

    private boolean isApiClientInvalid() {
        return mApiClient == null || (!mApiClient.isConnected() && !mApiClient.isConnecting());
    }
}
