// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.media.MediaCrypto;
import android.media.MediaDrm;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Handler;
import android.util.Log;

import org.apache.http.HttpResponse;
import org.apache.http.client.ClientProtocolException;
import org.apache.http.client.HttpClient;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.impl.client.DefaultHttpClient;
import org.apache.http.util.EntityUtils;
import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

import java.io.IOException;
import java.util.ArrayDeque;
import java.util.HashMap;
import java.util.UUID;

/**
 * A wrapper of the android MediaDrm class. Each MediaDrmBridge manages multiple
 * sessions for a single MediaSourcePlayer.
 */
@JNINamespace("media")
class MediaDrmBridge {
    // Implementation Notes:
    // - A media crypto session (mMediaCryptoSessionId) is opened after MediaDrm
    //   is created. This session will be added to mSessionIds.
    //   a) In multiple session mode,  this session will only be used to create
    //      the MediaCrypto object and it's associated mime type is always null.
    //   b) In single session mode, this session will be used to create the
    //      MediaCrypto object and will be used to call getKeyRequest() and
    //      manage all keys.
    // - Each generateKeyRequest() call creates a new session. All sessions are
    //   managed in mSessionIds.
    // - Whenever NotProvisionedException is thrown, we will clean up the
    //   current state and start the provisioning process.
    // - When provisioning is finished, we will try to resume suspended
    //   operations:
    //   a) Create the media crypto session if it's not created.
    //   b) Finish generateKeyRequest() if previous generateKeyRequest() was
    //      interrupted by a NotProvisionedException.
    // - Whenever an unexpected error occurred, we'll call release() to release
    //   all resources and clear all states. In that case all calls to this
    //   object will be no-op. All public APIs and callbacks should check
    //   mMediaBridge to make sure release() hasn't been called. Also, we call
    //   release() immediately after the error happens (e.g. after mMediaDrm)
    //   calls. Indirect calls should not call release() again to avoid
    //   duplication (even though it doesn't hurt to call release() twice).

    private static final String TAG = "MediaDrmBridge";
    private static final String SECURITY_LEVEL = "securityLevel";
    private static final String PRIVACY_MODE = "privacyMode";
    private static final String SESSION_SHARING = "sessionSharing";
    private static final String ENABLE = "enable";

    private MediaDrm mMediaDrm;
    private long mNativeMediaDrmBridge;
    private UUID mSchemeUUID;
    private Handler mHandler;

    // In this mode, we only open one session, i.e. mMediaCryptoSessionId, and
    // mSessionIds is always empty.
    private boolean mSingleSessionMode;

    // A session only for the purpose of creating a MediaCrypto object.
    // This session is opened when generateKeyRequest is called for the first
    // time.
    // - In multiple session mode, all following generateKeyRequest() calls
    // should create a new session and use it to call getKeyRequest(). No
    // getKeyRequest() should ever be called on this media crypto session.
    // - In single session mode, all generateKeyRequest() calls use the same
    // media crypto session. When generateKeyRequest() is called with a new
    // initData, previously added keys may not be available anymore.
    private String mMediaCryptoSessionId;
    private MediaCrypto mMediaCrypto;

    // The map of all opened sessions to their mime types.
    private HashMap<String, String> mSessionIds;

    private ArrayDeque<PendingGkrData> mPendingGkrDataQueue;

    private boolean mResetDeviceCredentialsPending;

    // MediaDrmBridge is waiting for provisioning response from the server.
    //
    // Notes about NotProvisionedException: This exception can be thrown in a
    // lot of cases. To streamline implementation, we do not catch it in private
    // non-native methods and only catch it in public APIs.
    private boolean mProvisioningPending;

    /*
     *  This class contains data needed to call generateKeyRequest().
     */
    private static class PendingGkrData {
        private final byte[] mInitData;
        private final String mMimeType;

        private PendingGkrData(byte[] initData, String mimeType) {
            mInitData = initData;
            mMimeType = mimeType;
        }

        private byte[] initData() { return mInitData; }
        private String mimeType() { return mMimeType; }
    }

    private static UUID getUUIDFromBytes(byte[] data) {
        if (data.length != 16) {
            return null;
        }
        long mostSigBits = 0;
        long leastSigBits = 0;
        for (int i = 0; i < 8; i++) {
            mostSigBits = (mostSigBits << 8) | (data[i] & 0xff);
        }
        for (int i = 8; i < 16; i++) {
            leastSigBits = (leastSigBits << 8) | (data[i] & 0xff);
        }
        return new UUID(mostSigBits, leastSigBits);
    }

    private MediaDrmBridge(UUID schemeUUID, String securityLevel, long nativeMediaDrmBridge,
            boolean singleSessionMode) throws android.media.UnsupportedSchemeException {
        mSchemeUUID = schemeUUID;
        mMediaDrm = new MediaDrm(schemeUUID);
        mNativeMediaDrmBridge = nativeMediaDrmBridge;
        mHandler = new Handler();
        mSingleSessionMode = singleSessionMode;
        mSessionIds = new HashMap<String, String>();
        mPendingGkrDataQueue = new ArrayDeque<PendingGkrData>();
        mResetDeviceCredentialsPending = false;
        mProvisioningPending = false;

        mMediaDrm.setOnEventListener(new MediaDrmListener());
        mMediaDrm.setPropertyString(PRIVACY_MODE, ENABLE);
        if (!mSingleSessionMode) {
            mMediaDrm.setPropertyString(SESSION_SHARING, ENABLE);
        }
        String currentSecurityLevel = mMediaDrm.getPropertyString(SECURITY_LEVEL);
        Log.e(TAG, "Security level: current " + currentSecurityLevel + ", new " + securityLevel);
        if (!securityLevel.equals(currentSecurityLevel)) {
            mMediaDrm.setPropertyString(SECURITY_LEVEL, securityLevel);
        }

        // We could open a MediaCrypto session here to support faster start of
        // clear lead (no need to wait for generateKeyRequest()). But on
        // Android, memory and battery resources are precious and we should
        // only create a session when we are sure we'll use it.
        // TODO(xhwang): Investigate other options to support fast start.
    }

    /**
     * Create a MediaCrypto object.
     *
     * @return whether a MediaCrypto object is successfully created.
     */
    private boolean createMediaCrypto() throws android.media.NotProvisionedException {
        if (mMediaDrm == null) {
            return false;
        }
        assert(!mProvisioningPending);
        assert(mMediaCryptoSessionId == null);
        assert(mMediaCrypto == null);

        // Open media crypto session.
        mMediaCryptoSessionId = openSession();
        if (mMediaCryptoSessionId == null) {
            Log.e(TAG, "Cannot create MediaCrypto Session.");
            return false;
        }
        Log.d(TAG, "MediaCrypto Session created: " + mMediaCryptoSessionId);

        // Create MediaCrypto object.
        try {
            if (MediaCrypto.isCryptoSchemeSupported(mSchemeUUID)) {
                final byte[] mediaCryptoSession = mMediaCryptoSessionId.getBytes("UTF-8");
                mMediaCrypto = new MediaCrypto(mSchemeUUID, mediaCryptoSession);
                assert(mMediaCrypto != null);
                Log.d(TAG, "MediaCrypto successfully created!");
                mSessionIds.put(mMediaCryptoSessionId, null);
                // Notify the native code that MediaCrypto is ready.
                nativeOnMediaCryptoReady(mNativeMediaDrmBridge);
                return true;
            } else {
                Log.e(TAG, "Cannot create MediaCrypto for unsupported scheme.");
            }
        } catch (java.io.UnsupportedEncodingException e) {
            Log.e(TAG, "Cannot create MediaCrypto", e);
        } catch (android.media.MediaCryptoException e) {
            Log.e(TAG, "Cannot create MediaCrypto", e);
        }

        release();
        return false;
    }

    /**
     * Open a new session and return the session ID string.
     *
     * @return null if unexpected error happened.
     */
    private String openSession() throws android.media.NotProvisionedException {
        assert(mMediaDrm != null);
        String sessionId = null;
        try {
            final byte[] session = mMediaDrm.openSession();
            sessionId = new String(session, "UTF-8");
        } catch (java.lang.RuntimeException e) {  // TODO(xhwang): Drop this?
            Log.e(TAG, "Cannot open a new session", e);
            release();
        } catch (java.io.UnsupportedEncodingException e) {
            Log.e(TAG, "Cannot open a new session", e);
            release();
        }
        return sessionId;
    }

    /**
     * Close a session.
     *
     * @param sesstionIdString ID of the session to be closed.
     */
    private void closeSession(String sesstionIdString) {
        assert(mMediaDrm != null);
        try {
            final byte[] session = sesstionIdString.getBytes("UTF-8");
            mMediaDrm.closeSession(session);
        } catch (java.io.UnsupportedEncodingException e) {
            Log.e(TAG, "Failed to close session", e);
        }
    }

    /**
     * Check whether the crypto scheme is supported for the given container.
     * If |containerMimeType| is an empty string, we just return whether
     * the crypto scheme is supported.
     * TODO(qinmin): Implement the checking for container.
     *
     * @return true if the container and the crypto scheme is supported, or
     * false otherwise.
     */
    @CalledByNative
    private static boolean isCryptoSchemeSupported(byte[] schemeUUID, String containerMimeType) {
        UUID cryptoScheme = getUUIDFromBytes(schemeUUID);
        return MediaDrm.isCryptoSchemeSupported(cryptoScheme);
    }

    /**
     * Create a new MediaDrmBridge from the crypto scheme UUID.
     *
     * @param schemeUUID Crypto scheme UUID.
     * @param securityLevel Security level to be used.
     * @param nativeMediaDrmBridge Native object of this class.
     */
    @CalledByNative
    private static MediaDrmBridge create(
            byte[] schemeUUID, String securityLevel, int nativeMediaDrmBridge) {
        UUID cryptoScheme = getUUIDFromBytes(schemeUUID);
        if (cryptoScheme == null || !MediaDrm.isCryptoSchemeSupported(cryptoScheme)) {
            return null;
        }

        boolean singleSessionMode = false;
        if (Build.VERSION.RELEASE.equals("4.4")) {
            singleSessionMode = true;
        }
        Log.d(TAG, "MediaDrmBridge uses " +
                (singleSessionMode ? "single" : "multiple") + "-session mode.");

        MediaDrmBridge media_drm_bridge = null;
        try {
            media_drm_bridge = new MediaDrmBridge(
                    cryptoScheme, securityLevel, nativeMediaDrmBridge, singleSessionMode);
        } catch (android.media.UnsupportedSchemeException e) {
            Log.e(TAG, "Unsupported DRM scheme", e);
        } catch (java.lang.IllegalArgumentException e) {
            Log.e(TAG, "Failed to create MediaDrmBridge", e);
        } catch (java.lang.IllegalStateException e) {
            Log.e(TAG, "Failed to create MediaDrmBridge", e);
        }

        return media_drm_bridge;
    }

    /**
     * Return the MediaCrypto object if available.
     */
    @CalledByNative
    private MediaCrypto getMediaCrypto() {
        return mMediaCrypto;
    }

    /**
     * Reset the device DRM credentials.
     */
    @CalledByNative
    private void resetDeviceCredentials() {
        mResetDeviceCredentialsPending = true;
        MediaDrm.ProvisionRequest request = mMediaDrm.getProvisionRequest();
        PostRequestTask postTask = new PostRequestTask(request.getData());
        postTask.execute(request.getDefaultUrl());
    }

    /**
     * Release the MediaDrmBridge object.
     */
    @CalledByNative
    private void release() {
        // Do not reset mHandler and mNativeMediaDrmBridge so that we can still
        // post KeyError back to native code.

        mPendingGkrDataQueue.clear();
        mPendingGkrDataQueue = null;

        for (String sessionId : mSessionIds.keySet()) {
            closeSession(sessionId);
        }
        mSessionIds.clear();
        mSessionIds = null;

        // This session was closed in the "for" loop above.
        mMediaCryptoSessionId = null;

        if (mMediaCrypto != null) {
            mMediaCrypto.release();
            mMediaCrypto = null;
        }

        if (mMediaDrm != null) {
            mMediaDrm.release();
            mMediaDrm = null;
        }
    }

    /**
     * Get a key request and post an asynchronous task to the native side
     * with the response message upon success, or with the key error if
     * unexpected error happened.
     *
     * @param sessionId ID of the session on which we need to get the key request.
     * @param data Data needed to get the key request.
     * @param mime Mime type to get the key request.
     *
     * @return whether a key request is successfully obtained.
     */
    private boolean getKeyRequest(String sessionId, byte[] data, String mime)
            throws android.media.NotProvisionedException {
        assert(mMediaDrm != null);
        assert(mMediaCrypto != null);
        assert(!mProvisioningPending);

        try {
            final byte[] session = sessionId.getBytes("UTF-8");
            HashMap<String, String> optionalParameters = new HashMap<String, String>();
            final MediaDrm.KeyRequest request = mMediaDrm.getKeyRequest(
                    session, data, mime, MediaDrm.KEY_TYPE_STREAMING, optionalParameters);
            Log.e(TAG, "Got key request successfully.");
            onKeyMessage(sessionId, request.getData(), request.getDefaultUrl());
            return true;
        } catch (java.io.UnsupportedEncodingException e) {
            Log.e(TAG, "Cannot get key request", e);
        }
        onKeyError(sessionId);
        release();
        return false;
    }

    /**
     * Save |initData| and |mime| to |mPendingGkrDataQueue| so that we can
     * resume the generateKeyRequest() call later.
     */
    private void savePendingGkrData(byte[] initData, String mime) {
        Log.d(TAG, "savePendingGkrData()");
        mPendingGkrDataQueue.offer(new PendingGkrData(initData, mime));
    }

    /**
     * Process all pending generateKeyRequest() calls synchronously.
     */
    private void processPendingGkrData() {
        Log.d(TAG, "processPendingGkrData()");
        assert(mMediaDrm != null);

        // Check mMediaDrm != null because error may happen in generateKeyRequest().
        // Check !mProvisioningPending because NotProvisionedException may be
        // thrown in generateKeyRequest().
        while (mMediaDrm != null && !mProvisioningPending && !mPendingGkrDataQueue.isEmpty()) {
            PendingGkrData pendingGkrData = mPendingGkrDataQueue.poll();
            byte[] initData = pendingGkrData.initData();
            String mime = pendingGkrData.mimeType();
            generateKeyRequest(initData, mime);
        }
    }

    /**
     * Process pending operations asynchrnously.
     */
    private void resumePendingOperations() {
        mHandler.post(new Runnable(){
            public void run() {
                processPendingGkrData();
            }
        });
    }

    /**
     * Generate a key request with |initData| and |mime|, and post an
     * asynchronous task to the native side with the key request or a key error.
     * In multiple session mode, a new session will be open. In single session
     * mode, the mMediaCryptoSessionId will be used.
     *
     * @param initData Data needed to generate the key request.
     * @param mime Mime type.
     */
    @CalledByNative
    private void generateKeyRequest(byte[] initData, String mime) {
        Log.d(TAG, "generateKeyRequest()");
        if (mMediaDrm == null) {
            Log.e(TAG, "generateKeyRequest() called when MediaDrm is null.");
            return;
        }

        if (mProvisioningPending) {
            assert(mMediaCrypto == null);
            savePendingGkrData(initData, mime);
            return;
        }

        boolean newSessionOpened = false;
        String sessionId = null;
        try {
            // Create MediaCrypto if necessary.
            if (mMediaCrypto == null && !createMediaCrypto()) {
              onKeyError(null);
              return;
            }
            assert(mMediaCrypto != null);
            assert(mSessionIds.containsKey(mMediaCryptoSessionId));

            if (mSingleSessionMode) {
                sessionId = mMediaCryptoSessionId;
                if (mSessionIds.get(sessionId) == null) {
                    // Set |mime| when we call generateKeyRequest() for the first time.
                    mSessionIds.put(sessionId, mime);
                } else if (!mSessionIds.get(sessionId).equals(mime)) {
                    Log.e(TAG, "Only one mime type is supported in single session mode.");
                    onKeyError(sessionId);
                    return;
                }
            } else {
                sessionId = openSession();
                if (sessionId == null) {
                    Log.e(TAG, "Cannot open session in generateKeyRequest().");
                    onKeyError(null);
                    return;
                }
                newSessionOpened = true;
                assert(!mSessionIds.containsKey(sessionId));
            }

            // KeyMessage or KeyError is fired in getKeyRequest().
            if (!getKeyRequest(sessionId, initData, mime)) {
                Log.e(TAG, "Cannot get key request in generateKeyRequest().");
                return;
            }

            if (newSessionOpened) {
                Log.e(TAG, "generateKeyRequest(): Session " + sessionId + " created.");
                mSessionIds.put(sessionId, mime);
                return;
            }
        } catch (android.media.NotProvisionedException e) {
            Log.e(TAG, "Device not provisioned", e);
            if (newSessionOpened) {
                closeSession(sessionId);
            }
            savePendingGkrData(initData, mime);
            startProvisioning();
        }
    }

    /**
     * Returns whether |sessionId| is a valid key session, excluding the media
     * crypto session in multi-session mode.
     *
     * @param sessionId Crypto session Id.
     */
    private boolean sessionExists(String sessionId) {
        if (mMediaCryptoSessionId == null) {
            assert(mSessionIds.isEmpty());
            Log.e(TAG, "Session doesn't exist because media crypto session is not created.");
            return false;
        }
        assert(mSessionIds.containsKey(mMediaCryptoSessionId));

        if (mSingleSessionMode) {
            return mMediaCryptoSessionId.equals(sessionId);
        }

        return !mMediaCryptoSessionId.equals(sessionId) && mSessionIds.containsKey(sessionId);
    }

    /**
     * Cancel a key request for a session Id.
     *
     * @param sessionId Crypto session Id.
     */
    @CalledByNative
    private void cancelKeyRequest(String sessionId) {
        Log.d(TAG, "cancelKeyRequest(): " + sessionId);
        if (mMediaDrm == null) {
            Log.e(TAG, "cancelKeyRequest() called when MediaDrm is null.");
            return;
        }

        if (!sessionExists(sessionId)) {
            Log.e(TAG, "Invalid session in cancelKeyRequest.");
            onKeyError(sessionId);
            return;
        }

        try {
            final byte[] session = sessionId.getBytes("UTF-8");
            mMediaDrm.removeKeys(session);
        } catch (java.io.UnsupportedEncodingException e) {
            Log.e(TAG, "Cannot cancel key request", e);
        }

        // We don't close the media crypto session in single session mode.
        if (!mSingleSessionMode) {
            closeSession(sessionId);
            mSessionIds.remove(sessionId);
            Log.d(TAG, "Session " + sessionId + "closed.");
        }
    }

    /**
     * Add a key for a session Id.
     *
     * @param sessionId Crypto session Id.
     * @param key Response data from the server.
     */
    @CalledByNative
    private void addKey(String sessionId, byte[] key) {
        Log.d(TAG, "addKey(): " + sessionId);
        if (mMediaDrm == null) {
            Log.e(TAG, "addKey() called when MediaDrm is null.");
            return;
        }

        // TODO(xhwang): We should be able to DCHECK this when WD EME is implemented.
        if (!sessionExists(sessionId)) {
            Log.e(TAG, "Invalid session in addKey.");
            onKeyError(sessionId);
            return;
        }

        try {
            final byte[] session = sessionId.getBytes("UTF-8");
            try {
                mMediaDrm.provideKeyResponse(session, key);
            } catch (java.lang.IllegalStateException e) {
                // This is not really an exception. Some error code are incorrectly
                // reported as an exception.
                // TODO(qinmin): remove this exception catch when b/10495563 is fixed.
                Log.e(TAG, "Exception intentionally caught when calling provideKeyResponse()", e);
            }
            onKeyAdded(sessionId);
            Log.d(TAG, "Key successfully added for session " + sessionId);
            return;
        } catch (android.media.NotProvisionedException e) {
            // TODO (xhwang): Should we handle this?
            Log.e(TAG, "failed to provide key response", e);
        } catch (android.media.DeniedByServerException e) {
            Log.e(TAG, "failed to provide key response", e);
        } catch (java.io.UnsupportedEncodingException e) {
            Log.e(TAG, "failed to provide key response", e);
        }
        onKeyError(sessionId);
        release();
    }

    /**
     * Return the security level of this DRM object.
     */
    @CalledByNative
    private String getSecurityLevel() {
        if (mMediaDrm == null) {
            Log.e(TAG, "getSecurityLevel() called when MediaDrm is null.");
            return null;
        }
        return mMediaDrm.getPropertyString("securityLevel");
    }

    private void startProvisioning() {
        Log.d(TAG, "startProvisioning");
        assert(mMediaDrm != null);
        assert(!mProvisioningPending);
        mProvisioningPending = true;
        MediaDrm.ProvisionRequest request = mMediaDrm.getProvisionRequest();
        PostRequestTask postTask = new PostRequestTask(request.getData());
        postTask.execute(request.getDefaultUrl());
    }

    /**
     * Called when the provision response is received.
     *
     * @param response Response data from the provision server.
     */
    private void onProvisionResponse(byte[] response) {
        Log.d(TAG, "onProvisionResponse()");
        assert(mProvisioningPending);
        mProvisioningPending = false;

        // If |mMediaDrm| is released, there is no need to callback native.
        if (mMediaDrm == null) {
            return;
        }

        boolean success = provideProvisionResponse(response);

        if (mResetDeviceCredentialsPending) {
            nativeOnResetDeviceCredentialsCompleted(mNativeMediaDrmBridge, success);
            mResetDeviceCredentialsPending = false;
        }

        if (success) {
            resumePendingOperations();
        }
    }

    /**
     * Provide the provisioning response to MediaDrm.
     * @returns false if the response is invalid or on error, true otherwise.
     */
    boolean provideProvisionResponse(byte[] response) {
        if (response == null || response.length == 0) {
            Log.e(TAG, "Invalid provision response.");
            return false;
        }

        try {
            mMediaDrm.provideProvisionResponse(response);
            return true;
        } catch (android.media.DeniedByServerException e) {
            Log.e(TAG, "failed to provide provision response", e);
        } catch (java.lang.IllegalStateException e) {
            Log.e(TAG, "failed to provide provision response", e);
        }
        return false;
    }

    private void onKeyMessage(
            final String sessionId, final byte[] message, final String destinationUrl) {
        mHandler.post(new Runnable(){
            public void run() {
                nativeOnKeyMessage(mNativeMediaDrmBridge, sessionId, message, destinationUrl);
            }
        });
    }

    private void onKeyAdded(final String sessionId) {
        mHandler.post(new Runnable() {
            public void run() {
                nativeOnKeyAdded(mNativeMediaDrmBridge, sessionId);
            }
        });
    }

    private void onKeyError(final String sessionId) {
        // TODO(qinmin): pass the error code to native.
        mHandler.post(new Runnable() {
            public void run() {
                nativeOnKeyError(mNativeMediaDrmBridge, sessionId);
            }
        });
    }

    private String GetSessionId(byte[] session) {
        String sessionId = null;
        try {
            sessionId = new String(session, "UTF-8");
        } catch (java.io.UnsupportedEncodingException e) {
            Log.e(TAG, "GetSessionId failed", e);
        } catch (java.lang.NullPointerException e) {
            Log.e(TAG, "GetSessionId failed", e);
        }
        return sessionId;
    }

    private class MediaDrmListener implements MediaDrm.OnEventListener {
        @Override
        public void onEvent(MediaDrm mediaDrm, byte[] session, int event, int extra, byte[] data) {
            String sessionId = null;
            switch(event) {
                case MediaDrm.EVENT_PROVISION_REQUIRED:
                    // This is handled by the handler of NotProvisionedException.
                    Log.d(TAG, "MediaDrm.EVENT_PROVISION_REQUIRED");
                    break;
                case MediaDrm.EVENT_KEY_REQUIRED:
                    Log.d(TAG, "MediaDrm.EVENT_KEY_REQUIRED");
                    sessionId = GetSessionId(session);
                    if (sessionId != null && !mProvisioningPending) {
                        assert(sessionExists(sessionId));
                        String mime = mSessionIds.get(sessionId);
                        try {
                            getKeyRequest(sessionId, data, mime);
                        } catch (android.media.NotProvisionedException e) {
                            Log.e(TAG, "Device not provisioned", e);
                            startProvisioning();
                        }
                    }
                    break;
                case MediaDrm.EVENT_KEY_EXPIRED:
                    Log.d(TAG, "MediaDrm.EVENT_KEY_EXPIRED");
                    sessionId = GetSessionId(session);
                    if (sessionId != null) {
                        onKeyError(sessionId);
                    }
                    break;
                case MediaDrm.EVENT_VENDOR_DEFINED:
                    Log.d(TAG, "MediaDrm.EVENT_VENDOR_DEFINED");
                    assert(false);  // Should never happen.
                    break;
                default:
                    Log.e(TAG, "Invalid DRM event " + (int)event);
                    return;
            }
        }
    }

    private class PostRequestTask extends AsyncTask<String, Void, Void> {
        private static final String TAG = "PostRequestTask";

        private byte[] mDrmRequest;
        private byte[] mResponseBody;

        public PostRequestTask(byte[] drmRequest) {
            mDrmRequest = drmRequest;
        }

        @Override
        protected Void doInBackground(String... urls) {
            mResponseBody = postRequest(urls[0], mDrmRequest);
            if (mResponseBody != null) {
                Log.d(TAG, "response length=" + mResponseBody.length);
            }
            return null;
        }

        private byte[] postRequest(String url, byte[] drmRequest) {
            HttpClient httpClient = new DefaultHttpClient();
            HttpPost httpPost = new HttpPost(url + "&signedRequest=" + new String(drmRequest));

            Log.d(TAG, "PostRequest:" + httpPost.getRequestLine());
            try {
                // Add data
                httpPost.setHeader("Accept", "*/*");
                httpPost.setHeader("User-Agent", "Widevine CDM v1.0");
                httpPost.setHeader("Content-Type", "application/json");

                // Execute HTTP Post Request
                HttpResponse response = httpClient.execute(httpPost);

                byte[] responseBody;
                int responseCode = response.getStatusLine().getStatusCode();
                if (responseCode == 200) {
                    responseBody = EntityUtils.toByteArray(response.getEntity());
                } else {
                    Log.d(TAG, "Server returned HTTP error code " + responseCode);
                    return null;
                }
                return responseBody;
            } catch (ClientProtocolException e) {
                e.printStackTrace();
            } catch (IOException e) {
                e.printStackTrace();
            }
            return null;
        }

        @Override
        protected void onPostExecute(Void v) {
            onProvisionResponse(mResponseBody);
        }
    }

    private native void nativeOnMediaCryptoReady(long nativeMediaDrmBridge);

    private native void nativeOnKeyMessage(long nativeMediaDrmBridge, String sessionId,
                                           byte[] message, String destinationUrl);

    private native void nativeOnKeyAdded(long nativeMediaDrmBridge, String sessionId);

    private native void nativeOnKeyError(long nativeMediaDrmBridge, String sessionId);

    private native void nativeOnResetDeviceCredentialsCompleted(
            long nativeMediaDrmBridge, boolean success);
}
