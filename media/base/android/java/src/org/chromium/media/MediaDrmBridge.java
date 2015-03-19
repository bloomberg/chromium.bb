// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.annotation.TargetApi;
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
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayDeque;
import java.util.Arrays;
import java.util.HashMap;
import java.util.UUID;

/**
 * A wrapper of the android MediaDrm class. Each MediaDrmBridge manages multiple
 * sessions for a single MediaSourcePlayer.
 */
@JNINamespace("media")
@TargetApi(Build.VERSION_CODES.KITKAT)
public class MediaDrmBridge {
    // Implementation Notes:
    // - A media crypto session (mMediaCryptoSession) is opened after MediaDrm
    //   is created. This session will be added to mSessionIds and will only be
    //   used to create the MediaCrypto object. Its associated mime type is
    //   always null and its session ID is always INVALID_SESSION_ID.
    // - Each createSession() call creates a new session. All sessions are
    //   managed in mSessionIds.
    // - Whenever NotProvisionedException is thrown, we will clean up the
    //   current state and start the provisioning process.
    // - When provisioning is finished, we will try to resume suspended
    //   operations:
    //   a) Create the media crypto session if it's not created.
    //   b) Finish createSession() if previous createSession() was interrupted
    //      by a NotProvisionedException.
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
    private static final int INVALID_SESSION_ID = 0;
    private static final char[] HEX_CHAR_LOOKUP = "0123456789ABCDEF".toCharArray();
    private static final long INVALID_NATIVE_MEDIA_DRM_BRIDGE = 0;

    // KeyStatus for CDM key information. MUST keep this value in sync with
    // CdmKeyInformation::KeyStatus.
    private static final int KEY_STATUS_USABLE = 0;
    private static final int KEY_STATUS_INTERNAL_ERROR = 1;
    private static final int KEY_STATUS_EXPIRED = 2;
    private static final int KEY_STATUS_OUTPUT_NOT_ALLOWED = 3;

    private MediaDrm mMediaDrm;
    private long mNativeMediaDrmBridge;
    private UUID mSchemeUUID;
    private Handler mHandler;

    // A session only for the purpose of creating a MediaCrypto object.
    // This session is opened when createSession() is called for the first
    // time. All following createSession() calls will create a new session and
    // use it to call getKeyRequest(). No getKeyRequest() should ever be called
    // on |mMediaCryptoSession|.
    private byte[] mMediaCryptoSession;
    private MediaCrypto mMediaCrypto;

    // The map of all opened sessions (excluding mMediaCryptoSession) to their mime types.
    private HashMap<ByteBuffer, String> mSessionIds;

    // The queue of all pending createSession() data.
    private ArrayDeque<PendingCreateSessionData> mPendingCreateSessionDataQueue;

    private boolean mResetDeviceCredentialsPending;

    // MediaDrmBridge is waiting for provisioning response from the server.
    //
    // Notes about NotProvisionedException: This exception can be thrown in a
    // lot of cases. To streamline implementation, we do not catch it in private
    // non-native methods and only catch it in public APIs.
    private boolean mProvisioningPending;

    /**
     *  This class contains data needed to call createSession().
     */
    private static class PendingCreateSessionData {
        private final byte[] mInitData;
        private final String mMimeType;
        private final long mPromiseId;

        private PendingCreateSessionData(byte[] initData, String mimeType, long promiseId) {
            mInitData = initData;
            mMimeType = mimeType;
            mPromiseId = promiseId;
        }

        private byte[] initData() {
            return mInitData;
        }

        private String mimeType() {
            return mMimeType;
        }

        private long promiseId() {
            return mPromiseId;
        }
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

    /**
     *  Convert byte array to hex string for logging.
     *  This is modified from BytesToHexString() in url/url_canon_unittest.cc.
     */
    private static String bytesToHexString(byte[] bytes) {
        StringBuffer hexString = new StringBuffer();
        for (int i = 0; i < bytes.length; ++i) {
            hexString.append(HEX_CHAR_LOOKUP[bytes[i] >>> 4]);
            hexString.append(HEX_CHAR_LOOKUP[bytes[i] & 0xf]);
        }
        return hexString.toString();
    }

    private boolean isNativeMediaDrmBridgeValid() {
        return mNativeMediaDrmBridge != INVALID_NATIVE_MEDIA_DRM_BRIDGE;
    }

    private MediaDrmBridge(UUID schemeUUID, long nativeMediaDrmBridge)
            throws android.media.UnsupportedSchemeException {
        mSchemeUUID = schemeUUID;
        mMediaDrm = new MediaDrm(schemeUUID);

        mNativeMediaDrmBridge = nativeMediaDrmBridge;
        assert isNativeMediaDrmBridgeValid();

        mHandler = new Handler();
        mSessionIds = new HashMap<ByteBuffer, String>();
        mPendingCreateSessionDataQueue = new ArrayDeque<PendingCreateSessionData>();
        mResetDeviceCredentialsPending = false;
        mProvisioningPending = false;

        mMediaDrm.setOnEventListener(new MediaDrmListener());
        mMediaDrm.setPropertyString(PRIVACY_MODE, ENABLE);
        mMediaDrm.setPropertyString(SESSION_SHARING, ENABLE);

        // We could open a MediaCrypto session here to support faster start of
        // clear lead (no need to wait for createSession()). But on
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
        assert !mProvisioningPending;
        assert mMediaCryptoSession == null;
        assert mMediaCrypto == null;

        // Open media crypto session.
        mMediaCryptoSession = openSession();
        if (mMediaCryptoSession == null) {
            Log.e(TAG, "Cannot create MediaCrypto Session.");
            return false;
        }
        Log.d(TAG, "MediaCrypto Session created: " + bytesToHexString(mMediaCryptoSession));

        // Create MediaCrypto object.
        try {
            // TODO: This requires KitKat. Is this class used on pre-KK devices?
            if (MediaCrypto.isCryptoSchemeSupported(mSchemeUUID)) {
                mMediaCrypto = new MediaCrypto(mSchemeUUID, mMediaCryptoSession);
                Log.d(TAG, "MediaCrypto successfully created!");
                // Notify the native code that MediaCrypto is ready.
                onMediaCryptoReady();
                return true;
            } else {
                Log.e(TAG, "Cannot create MediaCrypto for unsupported scheme.");
            }
        } catch (android.media.MediaCryptoException e) {
            Log.e(TAG, "Cannot create MediaCrypto", e);
        }

        release();
        return false;
    }

    /**
     * Open a new session.
     *
     * @return ID of the session opened. Returns null if unexpected error happened.
     */
    private byte[] openSession() throws android.media.NotProvisionedException {
        assert mMediaDrm != null;
        try {
            byte[] sessionId = mMediaDrm.openSession();
            // Make a clone here in case the underlying byte[] is modified.
            return sessionId.clone();
        } catch (java.lang.RuntimeException e) {  // TODO(xhwang): Drop this?
            Log.e(TAG, "Cannot open a new session", e);
            release();
            return null;
        } catch (android.media.NotProvisionedException e) {
            // Throw NotProvisionedException so that we can startProvisioning().
            throw e;
        } catch (android.media.MediaDrmException e) {
            // Other MediaDrmExceptions (e.g. ResourceBusyException) are not
            // recoverable.
            Log.e(TAG, "Cannot open a new session", e);
            release();
            return null;
        }
    }

    /**
     * Check whether the crypto scheme is supported for the given container.
     * If |containerMimeType| is an empty string, we just return whether
     * the crypto scheme is supported.
     *
     * @return true if the container and the crypto scheme is supported, or
     * false otherwise.
     */
    @CalledByNative
    private static boolean isCryptoSchemeSupported(byte[] schemeUUID, String containerMimeType) {
        UUID cryptoScheme = getUUIDFromBytes(schemeUUID);

        if (containerMimeType.isEmpty()) {
            return MediaDrm.isCryptoSchemeSupported(cryptoScheme);
        }

        return MediaDrm.isCryptoSchemeSupported(cryptoScheme, containerMimeType);
    }

    /**
     * Create a new MediaDrmBridge from the crypto scheme UUID.
     *
     * @param schemeUUID Crypto scheme UUID.
     * @param nativeMediaDrmBridge Native object of this class.
     */
    @CalledByNative
    private static MediaDrmBridge create(byte[] schemeUUID, long nativeMediaDrmBridge) {
        UUID cryptoScheme = getUUIDFromBytes(schemeUUID);
        if (cryptoScheme == null || !MediaDrm.isCryptoSchemeSupported(cryptoScheme)) {
            return null;
        }

        MediaDrmBridge mediaDrmBridge = null;
        try {
            mediaDrmBridge = new MediaDrmBridge(cryptoScheme, nativeMediaDrmBridge);
            Log.d(TAG, "MediaDrmBridge successfully created.");
        } catch (android.media.UnsupportedSchemeException e) {
            Log.e(TAG, "Unsupported DRM scheme", e);
        } catch (java.lang.IllegalArgumentException e) {
            Log.e(TAG, "Failed to create MediaDrmBridge", e);
        } catch (java.lang.IllegalStateException e) {
            Log.e(TAG, "Failed to create MediaDrmBridge", e);
        }

        return mediaDrmBridge;
    }

    /**
     * Set the security level that the MediaDrm object uses.
     * This function should be called right after we construct MediaDrmBridge
     * and before we make any other calls.
     *
     * @param securityLevel Security level to be set.
     * @return whether the security level was successfully set.
     */
    @CalledByNative
    private boolean setSecurityLevel(String securityLevel) {
        if (mMediaDrm == null || mMediaCrypto != null) {
            return false;
        }

        String currentSecurityLevel = mMediaDrm.getPropertyString(SECURITY_LEVEL);
        Log.e(TAG, "Security level: current " + currentSecurityLevel + ", new " + securityLevel);
        if (securityLevel.equals(currentSecurityLevel)) {
            // No need to set the same security level again. This is not just
            // a shortcut! Setting the same security level actually causes an
            // exception in MediaDrm!
            return true;
        }

        try {
            mMediaDrm.setPropertyString(SECURITY_LEVEL, securityLevel);
            return true;
        } catch (java.lang.IllegalArgumentException e) {
            Log.e(TAG, "Failed to set security level " + securityLevel, e);
        } catch (java.lang.IllegalStateException e) {
            Log.e(TAG, "Failed to set security level " + securityLevel, e);
        }

        Log.e(TAG, "Security level " + securityLevel + " not supported!");
        return false;
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
     * Destroy the MediaDrmBridge object.
     */
    @CalledByNative
    private void destroy() {
        mNativeMediaDrmBridge = INVALID_NATIVE_MEDIA_DRM_BRIDGE;
        release();
    }

    /**
     * Release all allocated resources and finish all pending operations.
     */
    private void release() {
        // Do not reset mHandler so that we can still post tasks back to native code.
        // Note that mNativeMediaDrmBridge may have already been reset (see destroy()).

        for (PendingCreateSessionData data : mPendingCreateSessionDataQueue) {
            onPromiseRejected(data.promiseId(), "Create session aborted.");
        }
        mPendingCreateSessionDataQueue.clear();
        mPendingCreateSessionDataQueue = null;

        for (ByteBuffer sessionId : mSessionIds.keySet()) {
            mMediaDrm.removeKeys(sessionId.array());
            mMediaDrm.closeSession(sessionId.array());
            onSessionClosed(sessionId.array());
        }
        mSessionIds.clear();
        mSessionIds = null;

        // This session was closed in the "for" loop above.
        mMediaCryptoSession = null;

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
     * Get a key request.
     *
     * @param sessionId ID of session on which we need to get the key request.
     * @param data Data needed to get the key request.
     * @param mime Mime type to get the key request.
     *
     * @return the key request.
     */
    private MediaDrm.KeyRequest getKeyRequest(byte[] sessionId, byte[] data, String mime)
            throws android.media.NotProvisionedException {
        assert mMediaDrm != null;
        assert mMediaCrypto != null;
        assert !mProvisioningPending;

        HashMap<String, String> optionalParameters = new HashMap<String, String>();
        MediaDrm.KeyRequest request = mMediaDrm.getKeyRequest(
                sessionId, data, mime, MediaDrm.KEY_TYPE_STREAMING, optionalParameters);
        String result = (request != null) ? "successed" : "failed";
        Log.d(TAG, "getKeyRequest " + result + "!");
        return request;
    }

    /**
     * Save data to |mPendingCreateSessionDataQueue| so that we can resume the
     * createSession() call later.
     *
     * @param initData Data needed to generate the key request.
     * @param mime Mime type.
     * @param promiseId Promise ID for the createSession() call.
     */
    private void savePendingCreateSessionData(byte[] initData, String mime, long promiseId) {
        Log.d(TAG, "savePendingCreateSessionData()");
        mPendingCreateSessionDataQueue.offer(
                new PendingCreateSessionData(initData, mime, promiseId));
    }

    /**
     * Process all pending createSession() calls synchronously.
     */
    private void processPendingCreateSessionData() {
        Log.d(TAG, "processPendingCreateSessionData()");
        assert mMediaDrm != null;

        // Check mMediaDrm != null because error may happen in createSession().
        // Check !mProvisioningPending because NotProvisionedException may be
        // thrown in createSession().
        while (mMediaDrm != null && !mProvisioningPending
                && !mPendingCreateSessionDataQueue.isEmpty()) {
            PendingCreateSessionData pendingData = mPendingCreateSessionDataQueue.poll();
            byte[] initData = pendingData.initData();
            String mime = pendingData.mimeType();
            long promiseId = pendingData.promiseId();
            createSession(initData, mime, promiseId);
        }
    }

    /**
     * Process pending operations asynchrnously.
     */
    private void resumePendingOperations() {
        mHandler.post(new Runnable(){
            @Override
            public void run() {
                processPendingCreateSessionData();
            }
        });
    }

    /**
     * Create a session, and generate a request with |initData| and |mime|.
     *
     * @param initData Data needed to generate the key request.
     * @param mime Mime type.
     * @param promiseId Promise ID for this call.
     */
    @CalledByNative
    private void createSession(byte[] initData, String mime, long promiseId) {
        Log.d(TAG, "createSession()");
        if (mMediaDrm == null) {
            Log.e(TAG, "createSession() called when MediaDrm is null.");
            return;
        }

        if (mProvisioningPending) {
            assert mMediaCrypto == null;
            savePendingCreateSessionData(initData, mime, promiseId);
            return;
        }

        boolean newSessionOpened = false;
        byte[] sessionId = null;
        try {
            // Create MediaCrypto if necessary.
            if (mMediaCrypto == null && !createMediaCrypto()) {
                onPromiseRejected(promiseId, "MediaCrypto creation failed.");
                return;
            }
            assert mMediaCryptoSession != null;
            assert mMediaCrypto != null;

            sessionId = openSession();
            if (sessionId == null) {
                onPromiseRejected(promiseId, "Open session failed.");
                return;
            }
            newSessionOpened = true;
            assert !sessionExists(sessionId);

            MediaDrm.KeyRequest request = null;
            request = getKeyRequest(sessionId, initData, mime);
            if (request == null) {
                mMediaDrm.closeSession(sessionId);
                onPromiseRejected(promiseId, "Generate request failed.");
                return;
            }

            // Success!
            Log.d(TAG, "createSession(): Session (" + bytesToHexString(sessionId) + ") created.");
            onPromiseResolvedWithSession(promiseId, sessionId);
            onSessionMessage(sessionId, request);
            mSessionIds.put(ByteBuffer.wrap(sessionId), mime);
        } catch (android.media.NotProvisionedException e) {
            Log.e(TAG, "Device not provisioned", e);
            if (newSessionOpened) {
                mMediaDrm.closeSession(sessionId);
            }
            savePendingCreateSessionData(initData, mime, promiseId);
            startProvisioning();
        }
    }

    /**
     * Check whether |sessionId| is an existing session ID, excluding the media
     * crypto session.
     *
     * @param sessionId Crypto session Id.
     * @return true if |sessionId| exists, false otherwise.
     */
    private boolean sessionExists(byte[] sessionId) {
        if (mMediaCryptoSession == null) {
            assert mSessionIds.isEmpty();
            Log.e(TAG, "Session doesn't exist because media crypto session is not created.");
            return false;
        }
        return !Arrays.equals(sessionId, mMediaCryptoSession)
                && mSessionIds.containsKey(ByteBuffer.wrap(sessionId));
    }

    /**
     * Close a session that was previously created by createSession().
     *
     * @param sessionId ID of session to be closed.
     * @param promiseId Promise ID of this call.
     */
    @CalledByNative
    private void closeSession(byte[] sessionId, long promiseId) {
        Log.d(TAG, "closeSession()");
        if (mMediaDrm == null) {
            onPromiseRejected(promiseId, "closeSession() called when MediaDrm is null.");
            return;
        }

        if (!sessionExists(sessionId)) {
            onPromiseRejected(promiseId,
                    "Invalid sessionId in closeSession(): " + bytesToHexString(sessionId));
            return;
        }

        mMediaDrm.removeKeys(sessionId);
        mMediaDrm.closeSession(sessionId);
        mSessionIds.remove(ByteBuffer.wrap(sessionId));
        onPromiseResolved(promiseId);
        onSessionClosed(sessionId);
        Log.d(TAG, "Session " + bytesToHexString(sessionId) + " closed.");
    }

    /**
     * Update a session with response.
     *
     * @param sessionId Reference ID of session to be updated.
     * @param response Response data from the server.
     * @param promiseId Promise ID of this call.
     */
    @CalledByNative
    private void updateSession(byte[] sessionId, byte[] response, long promiseId) {
        Log.d(TAG, "updateSession()");
        if (mMediaDrm == null) {
            onPromiseRejected(promiseId, "updateSession() called when MediaDrm is null.");
            return;
        }

        // TODO(xhwang): DCHECK this when prefixed EME is deprecated.
        if (!sessionExists(sessionId)) {
            onPromiseRejected(
                    promiseId, "Invalid session in updateSession: " + bytesToHexString(sessionId));
            return;
        }

        try {
            try {
                mMediaDrm.provideKeyResponse(sessionId, response);
            } catch (java.lang.IllegalStateException e) {
                // This is not really an exception. Some error code are incorrectly
                // reported as an exception.
                // TODO(qinmin): remove this exception catch when b/10495563 is fixed.
                Log.e(TAG, "Exception intentionally caught when calling provideKeyResponse()", e);
            }
            Log.d(TAG, "Key successfully added for session " + bytesToHexString(sessionId));
            onPromiseResolved(promiseId);
            onSessionKeysChange(sessionId, true, KEY_STATUS_USABLE);
            return;
        } catch (android.media.NotProvisionedException e) {
            // TODO(xhwang): Should we handle this?
            Log.e(TAG, "failed to provide key response", e);
        } catch (android.media.DeniedByServerException e) {
            Log.e(TAG, "failed to provide key response", e);
        }
        onPromiseRejected(promiseId, "Update session failed.");
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
        assert mMediaDrm != null;
        assert !mProvisioningPending;
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
        assert mProvisioningPending;
        mProvisioningPending = false;

        // If |mMediaDrm| is released, there is no need to callback native.
        if (mMediaDrm == null) {
            return;
        }

        boolean success = provideProvisionResponse(response);

        if (mResetDeviceCredentialsPending) {
            onResetDeviceCredentialsCompleted(success);
            mResetDeviceCredentialsPending = false;
        }

        if (success) {
            resumePendingOperations();
        }
    }

    /**
     * Provide the provisioning response to MediaDrm.
     *
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

    // Helper functions to post native calls to prevent reentrancy.

    private void onMediaCryptoReady() {
        mHandler.post(new Runnable(){
            @Override
            public void run() {
                if (isNativeMediaDrmBridgeValid()) {
                    nativeOnMediaCryptoReady(mNativeMediaDrmBridge);
                }
            }
        });
    }

    private void onPromiseResolved(final long promiseId) {
        mHandler.post(new Runnable(){
            @Override
            public void run() {
                if (isNativeMediaDrmBridgeValid()) {
                    nativeOnPromiseResolved(mNativeMediaDrmBridge, promiseId);
                }
            }
        });
    }

    private void onPromiseResolvedWithSession(final long promiseId, final byte[] sessionId) {
        mHandler.post(new Runnable(){
            @Override
            public void run() {
                if (isNativeMediaDrmBridgeValid()) {
                    nativeOnPromiseResolvedWithSession(mNativeMediaDrmBridge, promiseId, sessionId);
                }
            }
        });
    }

    private void onPromiseRejected(final long promiseId, final String errorMessage) {
        Log.e(TAG, "onPromiseRejected: " + errorMessage);
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                if (isNativeMediaDrmBridgeValid()) {
                    nativeOnPromiseRejected(mNativeMediaDrmBridge, promiseId, errorMessage);
                }
            }
        });
    }

    private void onSessionMessage(final byte[] sessionId, final MediaDrm.KeyRequest request) {
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                if (isNativeMediaDrmBridgeValid()) {
                    nativeOnSessionMessage(mNativeMediaDrmBridge, sessionId, request.getData(),
                            request.getDefaultUrl());
                }
            }
        });
    }

    private void onSessionClosed(final byte[] sessionId) {
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                if (isNativeMediaDrmBridgeValid()) {
                    nativeOnSessionClosed(mNativeMediaDrmBridge, sessionId);
                }
            }
        });
    }

    private void onSessionKeysChange(
            final byte[] sessionId, final boolean hasAdditionalUsableKey, final int keyStatus) {
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                if (isNativeMediaDrmBridgeValid()) {
                    nativeOnSessionKeysChange(
                            mNativeMediaDrmBridge, sessionId, hasAdditionalUsableKey, keyStatus);
                }
            }
        });
    }

    private void onLegacySessionError(final byte[] sessionId, final String errorMessage) {
        Log.e(TAG, "onLegacySessionError: " + errorMessage);
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                if (isNativeMediaDrmBridgeValid()) {
                    nativeOnLegacySessionError(mNativeMediaDrmBridge, sessionId, errorMessage);
                }
            }
        });
    }

    private void onResetDeviceCredentialsCompleted(final boolean success) {
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                if (isNativeMediaDrmBridgeValid()) {
                    nativeOnResetDeviceCredentialsCompleted(mNativeMediaDrmBridge, success);
                }
            }
        });
    }

    private class MediaDrmListener implements MediaDrm.OnEventListener {
        @Override
        public void onEvent(
                MediaDrm mediaDrm, byte[] sessionId, int event, int extra, byte[] data) {
            if (sessionId == null) {
                Log.e(TAG, "MediaDrmListener: Null session.");
                return;
            }
            if (!sessionExists(sessionId)) {
                Log.e(TAG, "MediaDrmListener: Invalid session " + bytesToHexString(sessionId));
                return;
            }
            switch(event) {
                case MediaDrm.EVENT_PROVISION_REQUIRED:
                    Log.d(TAG, "MediaDrm.EVENT_PROVISION_REQUIRED");
                    break;
                case MediaDrm.EVENT_KEY_REQUIRED:
                    Log.d(TAG, "MediaDrm.EVENT_KEY_REQUIRED");
                    if (mProvisioningPending) {
                        return;
                    }
                    String mime = mSessionIds.get(ByteBuffer.wrap(sessionId));
                    MediaDrm.KeyRequest request = null;
                    try {
                        request = getKeyRequest(sessionId, data, mime);
                    } catch (android.media.NotProvisionedException e) {
                        Log.e(TAG, "Device not provisioned", e);
                        startProvisioning();
                        return;
                    }
                    if (request != null) {
                        onSessionMessage(sessionId, request);
                    } else {
                        onLegacySessionError(sessionId,
                                "MediaDrm EVENT_KEY_REQUIRED: Failed to generate request.");
                        onSessionKeysChange(sessionId, false, KEY_STATUS_INTERNAL_ERROR);
                    }
                    break;
                case MediaDrm.EVENT_KEY_EXPIRED:
                    Log.d(TAG, "MediaDrm.EVENT_KEY_EXPIRED");
                    onLegacySessionError(sessionId, "MediaDrm EVENT_KEY_EXPIRED.");
                    onSessionKeysChange(sessionId, false, KEY_STATUS_EXPIRED);
                    break;
                case MediaDrm.EVENT_VENDOR_DEFINED:
                    Log.d(TAG, "MediaDrm.EVENT_VENDOR_DEFINED");
                    assert false;  // Should never happen.
                    break;
                default:
                    Log.e(TAG, "Invalid DRM event " + event);
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

    public static void addKeySystemUuidMapping(String keySystem, UUID uuid) {
        ByteBuffer uuidBuffer = ByteBuffer.allocateDirect(16);
        // MSB (byte) should be positioned at the first element.
        uuidBuffer.order(ByteOrder.BIG_ENDIAN);
        uuidBuffer.putLong(uuid.getMostSignificantBits());
        uuidBuffer.putLong(uuid.getLeastSignificantBits());
        nativeAddKeySystemUuidMapping(keySystem, uuidBuffer);
    }

    private native void nativeOnMediaCryptoReady(long nativeMediaDrmBridge);

    private native void nativeOnPromiseResolved(long nativeMediaDrmBridge, long promiseId);
    private native void nativeOnPromiseResolvedWithSession(
            long nativeMediaDrmBridge, long promiseId, byte[] sessionId);
    private native void nativeOnPromiseRejected(
            long nativeMediaDrmBridge, long promiseId, String errorMessage);

    private native void nativeOnSessionMessage(
            long nativeMediaDrmBridge, byte[] sessionId, byte[] message, String destinationUrl);
    private native void nativeOnSessionClosed(long nativeMediaDrmBridge, byte[] sessionId);
    private native void nativeOnSessionKeysChange(long nativeMediaDrmBridge, byte[] sessionId,
            boolean hasAdditionalUsableKey, int keyStatus);
    private native void nativeOnLegacySessionError(
            long nativeMediaDrmBridge, byte[] sessionId, String errorMessage);

    private native void nativeOnResetDeviceCredentialsCompleted(
            long nativeMediaDrmBridge, boolean success);

    private static native void nativeAddKeySystemUuidMapping(String keySystem, ByteBuffer uuid);
}
