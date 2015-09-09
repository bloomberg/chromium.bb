// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router.cast;

import com.google.android.gms.cast.ApplicationMetadata;
import com.google.android.gms.cast.Cast;
import com.google.android.gms.cast.CastDevice;
import com.google.android.gms.common.api.GoogleApiClient;
import com.google.android.gms.common.api.ResultCallback;
import com.google.android.gms.common.api.Status;

import org.chromium.base.Log;
import org.chromium.chrome.browser.media.router.ChromeMediaRouter;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.IOException;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;

/**
 * A wrapper around the established Cast application session.
 */
public class SessionWrapper {
    private static final String TAG = "cr.MediaRouter";

    private static final String MEDIA_NAMESPACE = "urn:x-cast:com.google.cast.media";
    private static final String RECEIVER_NAMESPACE = "urn:x-cast:com.google.cast.receiver";
    private static final String MEDIA_MESSAGE_TYPES[] = {
            "PLAY",
            "LOAD",
            "PAUSE",
            "SEEK",
            "GET_STATUS",
            "STOP_MEDIA",
            "SET_VOLUME",
            "GET_STATUS",
            "EDIT_TRACKS_INFO",
            "QUEUE_LOAD",
            "QUEUE_INSERT",
            "QUEUE_UPDATE",
            "QUEUE_REMOVE",
            "QUEUE_REORDER"
    };

    private static class CastMessagingChannel implements Cast.MessageReceivedCallback {
        private final SessionWrapper mSession;

        public CastMessagingChannel(SessionWrapper session) {
            mSession = session;
        }

        @Override
        public void onMessageReceived(CastDevice castDevice, String namespace, String message) {
            mSession.onMessage("v2_message", message);
        }
    }

    private final String mMediaRouteId;
    private final CastMessagingChannel mMessageChannel;
    private final ChromeMediaRouter mMediaRouter;
    private final CastDevice mCastDevice;

    // Ids of the connected Cast clients.
    private Set<String> mClients = new HashSet<String>();
    private GoogleApiClient mApiClient;
    private String mSessionId;
    private String mApplicationStatus;
    private ApplicationMetadata mApplicationMetadata;
    private int mSequenceNumber;

    /**
     * Initializes a new {@link SessionWrapper} instance.
     * @param apiClient The Google Play Services client used to create the session.
     * @param sessionId The session identifier to use with the Cast SDK.
     * @param mediaRouteId The media route identifier associated with this session.
     * @param mediaRouter The {@link ChromeMediaRouter} instance managing this session.
     */
    public SessionWrapper(
            GoogleApiClient apiClient,
            Cast.ApplicationConnectionResult result,
            CastDevice castDevice,
            String mediaRouteId,
            ChromeMediaRouter mediaRouter) {
        mApiClient = apiClient;
        mSessionId = result.getSessionId();
        mMediaRouteId = mediaRouteId;
        mMediaRouter = mediaRouter;
        mApplicationMetadata = result.getApplicationMetadata();
        mApplicationStatus = result.getApplicationStatus();
        mCastDevice = castDevice;

        mMessageChannel = new CastMessagingChannel(this);
        try {
            Cast.CastApi.setMessageReceivedCallbacks(
                    mApiClient, MEDIA_NAMESPACE, mMessageChannel);
            Cast.CastApi.setMessageReceivedCallbacks(
                    mApiClient, RECEIVER_NAMESPACE, mMessageChannel);
        } catch (IOException e) {
            Log.e(TAG, "Exception while creating channel", e);
        }
    }

    /**
     * Stops the Cast application associated with this session.
     */
    public void stop() {
        assert mApiClient != null;

        if (mApiClient.isConnected() || mApiClient.isConnecting()) {
            Cast.CastApi.stopApplication(mApiClient, mSessionId);
        }

        mSessionId = null;
        mApiClient = null;
    }

    /**
     * Send a string message to the session and invokes the {@link ChromeMediaRouter} with the
     * passed callback id on success or failure.
     * @param message The message to send.
     * @param callbackId The id of the callback handling the result.
     */
    public void sendStringMessage(String message, final int callbackId) {
        if (handleInternalMessage(message, callbackId)) return;

        // TODO(avayvod): figure out what to do with custom namespace messages.
        mMediaRouter.onMessageSentResult(false, callbackId);
    }

    public void onMessage(String type, String message) {
        for (String client : mClients) {
            mMediaRouter.onMessage(mMediaRouteId,
                    buildInternalMessage(type, message, client, mSequenceNumber));
        }
        mSequenceNumber++;
    }

    private boolean handleInternalMessage(String message, int callbackId) {
        boolean success = true;
        try {
            JSONObject jsonMessage = new JSONObject(message);

            String messageType = jsonMessage.getString("type");
            if ("client_connect".equals(messageType)) {
                success = handleClientConnectMessage(jsonMessage);
            } else if ("v2_message".equals(messageType)) {
                success = handleCastV2Message(message, jsonMessage);
            } else {
                return false;
            }
        } catch (JSONException e) {
            return false;
        }

        mMediaRouter.onMessageSentResult(success, callbackId);
        return true;
    }

    private boolean handleClientConnectMessage(JSONObject jsonMessage)
            throws JSONException {
        String clientId = jsonMessage.getString("clientId");

        if (mClients.contains(clientId)) return false;

        mClients.add(clientId);

        mMediaRouter.onMessage(mMediaRouteId,
                buildInternalMessage("new_session", buildSessionMessage(), clientId, -1));
        return true;
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
    private boolean handleCastV2Message(String message, JSONObject jsonMessage)
            throws JSONException {
        assert "v2_message".equals(jsonMessage.getString("type"));

        String clientId = jsonMessage.getString("clientId");
        if (!mClients.contains(clientId)) return false;

        JSONObject jsonCastMessage = jsonMessage.getJSONObject("message");
        String messageType = jsonCastMessage.getString("type");
        if ("STOP".equals(messageType)) {
            return stopApplication();
        } else if (Arrays.asList(MEDIA_MESSAGE_TYPES).contains(messageType)) {
            return sendCastMessage(jsonMessage.getString("message"), MEDIA_NAMESPACE);
        }

        return true;
    }

    private boolean sendCastMessage(String message, String namespace) {
        if (!mApiClient.isConnected() && !mApiClient.isConnecting()) return false;

        if (!message.contains("\"requestId\"")) {
            try {
                JSONObject jsonMessage = new JSONObject(message);
                jsonMessage.put("requestId", 0);
                message = jsonMessage.toString();
            } catch (JSONException e) {
                Log.w(TAG, "Cast message is not a valid JSON");
            }
        }

        try {
            Cast.CastApi.sendMessage(mApiClient, namespace, message)
                    .setResultCallback(
                            new ResultCallback<Status>() {
                                @Override
                                public void onResult(Status result) {
                                    // TODO(avayvod): should actually report the result of sending
                                    // status here.
                                }
                            });
        } catch (Exception e) {
            Log.e(TAG, "Exception while sending message", e);
            return false;
        }
        return true;
    }

    private String buildInternalMessage(
            String type, String message, String clientId, int sequenceNumber) {
        return String.format(Locale.US,
                "{ \"type\": \"%s\"," + "\"message\": %s," + "\"sequenceNumber\":%d,"
                        + "\"timeoutMillis\":0," + "\"clientId\": \"%s\"}",
                type, message, sequenceNumber, clientId);
    }

    private boolean stopApplication() {
        if (!mApiClient.isConnected() && !mApiClient.isConnecting()) return false;

        try {
            Cast.CastApi.stopApplication(mApiClient, mSessionId);
        } catch (Exception e) {
            Log.e(TAG, "Stopping the application failed.", e);
            return false;
        }
        onMessage("remove_session", "\"" + mSessionId + "\"");
        return true;
    }

    public void updateSessionStatus() {
        if (!mApiClient.isConnected() && !mApiClient.isConnecting()) return;

        try {
            mApplicationStatus = Cast.CastApi.getApplicationStatus(mApiClient);
            mApplicationMetadata = Cast.CastApi.getApplicationMetadata(mApiClient);

            for (String clientId : mClients) {
                mMediaRouter.onMessage(mMediaRouteId, buildInternalMessage(
                        "update_session", buildSessionMessage(), clientId, -1));
            }
        } catch (IllegalStateException e) {
            Log.d(TAG, "Can't get application status", e);
        }
    }

    private String buildSessionMessage() {
        if (!mApiClient.isConnected() && !mApiClient.isConnecting()) return "{}";

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

            JSONObject jsonMessage = new JSONObject();
            jsonMessage.put("sessionId", mSessionId);
            jsonMessage.put("appId", mApplicationMetadata.getApplicationId());
            jsonMessage.put("displayName", mApplicationMetadata.getName());
            jsonMessage.put("statusText", mApplicationStatus);
            jsonMessage.put("receiver", jsonReceiver);
            jsonMessage.put("namespaces", extractNamespaces(mApplicationMetadata));
            jsonMessage.put("media", new JSONArray());
            jsonMessage.put("status", "connected");
            jsonMessage.put("transportId", "web-4");

            return jsonMessage.toString();
        } catch (JSONException e) {
            Log.w(TAG, "Building session message failed", e);
            return "{}";
        }
    }

    private JSONArray getCapabilities(CastDevice device) {
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

    private JSONArray extractNamespaces(ApplicationMetadata metadata) throws JSONException {
        JSONArray jsonNamespaces = new JSONArray();
        // TODO(avayvod): Need a way to retrieve all the supported namespaces (e.g. YouTube).
        // See crbug.com/529680.
        if (metadata.isNamespaceSupported(MEDIA_NAMESPACE)) {
            JSONObject jsonNamespace = new JSONObject();
            jsonNamespace.put("name", MEDIA_NAMESPACE);
            jsonNamespaces.put(jsonNamespace);
        }
        return jsonNamespaces;
    }
}
