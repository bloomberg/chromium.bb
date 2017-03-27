// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.media.MediaCrypto;
import android.media.MediaDrm;
import android.os.Build;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;
import org.chromium.media.MediaDrmSessionManager.SessionId;
import org.chromium.media.MediaDrmSessionManager.SessionInfo;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.UUID;

/**
 * A wrapper of the android MediaDrm class. Each MediaDrmBridge manages multiple
 * sessions for AndroidVideoDecodeAccelerators and MediaCodecAudioDecoders.
 */
@JNINamespace("media")
@MainDex
@SuppressLint("WrongConstant")
@TargetApi(Build.VERSION_CODES.KITKAT)
public class MediaDrmBridge {
    // Implementation Notes:
    // - A media crypto session (mMediaCryptoSession) is opened after MediaDrm
    //   is created. This session will NOT be added to mSessionManager and will only
    //   be used to create the MediaCrypto object.
    // - Each createSession() call creates a new session. All created sessions
    //   are managed in mSessionManager.
    // - Whenever NotProvisionedException is thrown, we will clean up the
    //   current state and start the provisioning process.
    // - When provisioning is finished, we will try to resume suspended
    //   operations:
    //   a) Create the media crypto session if it's not created.
    //   b) Finish createSession() if previous createSession() was interrupted
    //      by a NotProvisionedException.
    // - Whenever an unexpected error occurred, we'll call release() to release
    //   all resources immediately, clear all states and fail all pending
    //   operations. After that all calls to this object will fail (e.g. return
    //   null or reject the promise). All public APIs and callbacks should check
    //   mMediaBridge to make sure release() hasn't been called.

    private static final String TAG = "cr_media";
    private static final String SECURITY_LEVEL = "securityLevel";
    private static final String SERVER_CERTIFICATE = "serviceCertificate";
    private static final String ORIGIN = "origin";
    private static final String PRIVACY_MODE = "privacyMode";
    private static final String SESSION_SHARING = "sessionSharing";
    private static final String ENABLE = "enable";
    private static final long INVALID_NATIVE_MEDIA_DRM_BRIDGE = 0;

    // Scheme UUID for Widevine. See http://dashif.org/identifiers/protection/
    private static final UUID WIDEVINE_UUID =
            UUID.fromString("edef8ba9-79d6-4ace-a3c8-27dcd51d21ed");

    // On Android L and before, MediaDrm doesn't support KeyStatus. Use a dummy
    // key ID to report key status info.
    // See details: https://github.com/w3c/encrypted-media/issues/32
    private static final byte[] DUMMY_KEY_ID = new byte[] {0};

    private MediaDrm mMediaDrm;
    private long mNativeMediaDrmBridge;
    private UUID mSchemeUUID;

    // A session only for the purpose of creating a MediaCrypto object. Created
    // after construction, or after the provisioning process is successfully
    // completed. No getKeyRequest() should be called on |mMediaCryptoSession|.
    private SessionId mMediaCryptoSession;

    // The map of all opened sessions (excluding mMediaCryptoSession) to their
    // associated meta data, e.g. mime types, key types.
    private MediaDrmSessionManager mSessionManager;

    // The queue of all pending createSession() data.
    private ArrayDeque<PendingCreateSessionData> mPendingCreateSessionDataQueue;

    private boolean mResetDeviceCredentialsPending;

    // MediaDrmBridge is waiting for provisioning response from the server.
    private boolean mProvisioningPending;

    /**
     *  An equivalent of MediaDrm.KeyStatus, which is only available on M+.
     */
    @MainDex
    private static class KeyStatus {
        private final byte[] mKeyId;
        private final int mStatusCode;

        private KeyStatus(byte[] keyId, int statusCode) {
            mKeyId = keyId;
            mStatusCode = statusCode;
        }

        @CalledByNative("KeyStatus")
        private byte[] getKeyId() {
            return mKeyId;
        }

        @CalledByNative("KeyStatus")
        private int getStatusCode() {
            return mStatusCode;
        }
    }

    /**
     *  Creates a dummy single element list of KeyStatus with a dummy key ID and
     *  the specified keyStatus.
     */
    private static List<KeyStatus> getDummyKeysInfo(int statusCode) {
        List<KeyStatus> keysInfo = new ArrayList<KeyStatus>();
        keysInfo.add(new KeyStatus(DUMMY_KEY_ID, statusCode));
        return keysInfo;
    }

    /**
     *  This class contains data needed to call createSession().
     */
    @MainDex
    private static class PendingCreateSessionData {
        private final byte[] mInitData;
        private final String mMimeType;

        // Key type to get the key request. Only KEY_TYPE_STREAMING and
        // KEY_TYPE_OFFLINE are supported.
        private final int mKeyType;
        private final HashMap<String, String> mOptionalParameters;
        private final long mPromiseId;

        private PendingCreateSessionData(byte[] initData, String mimeType, int keyType,
                HashMap<String, String> optionalParameters, long promiseId) {
            mInitData = initData;
            mMimeType = mimeType;

            assert keyType == MediaDrm.KEY_TYPE_STREAMING || keyType == MediaDrm.KEY_TYPE_OFFLINE;
            mKeyType = keyType;

            mOptionalParameters = optionalParameters;
            mPromiseId = promiseId;
        }

        private byte[] initData() {
            return mInitData;
        }

        private String mimeType() {
            return mMimeType;
        }

        private int keyType() {
            return mKeyType;
        }

        private HashMap<String, String> optionalParameters() {
            return mOptionalParameters;
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

    private boolean isNativeMediaDrmBridgeValid() {
        return mNativeMediaDrmBridge != INVALID_NATIVE_MEDIA_DRM_BRIDGE;
    }

    private boolean isWidevine() {
        return mSchemeUUID.equals(WIDEVINE_UUID);
    }

    @TargetApi(Build.VERSION_CODES.M)
    private MediaDrmBridge(UUID schemeUUID, long nativeMediaDrmBridge)
            throws android.media.UnsupportedSchemeException {
        mSchemeUUID = schemeUUID;
        mMediaDrm = new MediaDrm(schemeUUID);

        mNativeMediaDrmBridge = nativeMediaDrmBridge;
        assert isNativeMediaDrmBridgeValid();

        mSessionManager = new MediaDrmSessionManager();
        mPendingCreateSessionDataQueue = new ArrayDeque<PendingCreateSessionData>();
        mResetDeviceCredentialsPending = false;
        mProvisioningPending = false;

        mMediaDrm.setOnEventListener(new EventListener());
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            mMediaDrm.setOnExpirationUpdateListener(new ExpirationUpdateListener(), null);
            mMediaDrm.setOnKeyStatusChangeListener(new KeyStatusChangeListener(), null);
        }

        if (isWidevine()) {
            mMediaDrm.setPropertyString(PRIVACY_MODE, ENABLE);
            mMediaDrm.setPropertyString(SESSION_SHARING, ENABLE);
        }
    }

    /**
     * Create a MediaCrypto object.
     *
     * @return false upon fatal error in creating MediaCrypto. Returns true
     * otherwise, including the following two cases:
     *   1. MediaCrypto is successfully created and notified.
     *   2. Device is not provisioned and MediaCrypto creation will be tried
     *      again after the provisioning process is completed.
     *
     *  When false is returned, the caller should call release(), which will
     *  notify the native code with a null MediaCrypto, if needed.
     */
    private boolean createMediaCrypto() {
        assert mMediaDrm != null;
        assert !mProvisioningPending;
        assert mMediaCryptoSession == null;

        // Open media crypto session.
        byte[] mediaCryptoSessionDrmId = null;
        try {
            mediaCryptoSessionDrmId = openSession();
        } catch (android.media.NotProvisionedException e) {
            Log.d(TAG, "Device not provisioned", e);
            startProvisioning();
            return true;
        }

        if (mediaCryptoSessionDrmId == null) {
            Log.e(TAG, "Cannot create MediaCrypto Session.");
            return false;
        }

        mMediaCryptoSession = SessionId.createTemporarySessionId(mediaCryptoSessionDrmId);

        Log.d(TAG, "MediaCrypto Session created: %s", mMediaCryptoSession.toHexString());

        // Create MediaCrypto object.
        try {
            if (MediaCrypto.isCryptoSchemeSupported(mSchemeUUID)) {
                MediaCrypto mediaCrypto = new MediaCrypto(mSchemeUUID, mMediaCryptoSession.drmId());
                Log.d(TAG, "MediaCrypto successfully created!");
                onMediaCryptoReady(mediaCrypto);
                return true;
            } else {
                Log.e(TAG, "Cannot create MediaCrypto for unsupported scheme.");
            }
        } catch (android.media.MediaCryptoException e) {
            Log.e(TAG, "Cannot create MediaCrypto", e);
        }

        closeSessionNoException(mMediaCryptoSession);
        mMediaCryptoSession = null;

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
     * @param securityOrigin Security origin. Empty value means no need for origin isolated storage.
     * @param securityLevel Security level. If empty, the default one should be used.
     * @param nativeMediaDrmBridge Native object of this class.
     */
    @CalledByNative
    private static MediaDrmBridge create(byte[] schemeUUID, String securityOrigin,
            String securityLevel, long nativeMediaDrmBridge) {
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
            return null;
        } catch (java.lang.IllegalArgumentException e) {
            Log.e(TAG, "Failed to create MediaDrmBridge", e);
            return null;
        } catch (java.lang.IllegalStateException e) {
            Log.e(TAG, "Failed to create MediaDrmBridge", e);
            return null;
        }

        if (!securityLevel.isEmpty() && !mediaDrmBridge.setSecurityLevel(securityLevel)) {
            return null;
        }

        if (!securityOrigin.isEmpty() && !mediaDrmBridge.setOrigin(securityOrigin)) {
            return null;
        }

        if (!mediaDrmBridge.createMediaCrypto()) {
            return null;
        }

        return mediaDrmBridge;
    }

    /**
     * Set the security origin for the MediaDrm. All information should be isolated for different
     * origins, e.g. certificates, licenses.
     */
    private boolean setOrigin(String origin) {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.M;

        if (!isWidevine()) {
            Log.d(TAG, "Property " + ORIGIN + " isn't supported");
            return true;
        }

        assert mMediaDrm != null;
        assert !origin.isEmpty();

        try {
            mMediaDrm.setPropertyString(ORIGIN, origin);
            return true;
        } catch (java.lang.IllegalArgumentException e) {
            Log.e(TAG, "Failed to set security origin %s", origin, e);
        } catch (java.lang.IllegalStateException e) {
            Log.e(TAG, "Failed to set security origin %s", origin, e);
        }

        Log.e(TAG, "Security origin %s not supported!", origin);
        return false;
    }

    /**
     * Set the security level that the MediaDrm object uses.
     * This function should be called right after we construct MediaDrmBridge
     * and before we make any other calls.
     *
     * @param securityLevel Security level to be set.
     * @return whether the security level was successfully set.
     */
    private boolean setSecurityLevel(String securityLevel) {
        if (!isWidevine()) {
            Log.d(TAG, "Security level is not supported.");
            return true;
        }

        assert mMediaDrm != null;
        assert !securityLevel.isEmpty();

        String currentSecurityLevel = mMediaDrm.getPropertyString(SECURITY_LEVEL);
        Log.e(TAG, "Security level: current %s, new %s", currentSecurityLevel, securityLevel);
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
            Log.e(TAG, "Failed to set security level %s", securityLevel, e);
        } catch (java.lang.IllegalStateException e) {
            Log.e(TAG, "Failed to set security level %s", securityLevel, e);
        }

        Log.e(TAG, "Security level %s not supported!", securityLevel);
        return false;
    }

    /**
     * Set the server certificate.
     *
     * @param certificate Server certificate to be set.
     * @return whether the server certificate was successfully set.
     */
    @CalledByNative
    private boolean setServerCertificate(byte[] certificate) {
        if (!isWidevine()) {
            Log.d(TAG, "Setting server certificate is not supported.");
            return true;
        }

        try {
            mMediaDrm.setPropertyByteArray(SERVER_CERTIFICATE, certificate);
            return true;
        } catch (java.lang.IllegalArgumentException e) {
            Log.e(TAG, "Failed to set server certificate", e);
        } catch (java.lang.IllegalStateException e) {
            Log.e(TAG, "Failed to set server certificate", e);
        }

        return false;
    }

    /**
     * Reset the device DRM credentials.
     */
    @CalledByNative
    private void resetDeviceCredentials() {
        if (mMediaDrm == null) {
            onResetDeviceCredentialsCompleted(false);
            return;
        }

        mResetDeviceCredentialsPending = true;
        startProvisioning();
    }

    /**
     * Destroy the MediaDrmBridge object.
     */
    @CalledByNative
    private void destroy() {
        mNativeMediaDrmBridge = INVALID_NATIVE_MEDIA_DRM_BRIDGE;
        if (mMediaDrm != null) {
            release();
        }
    }

    /**
     * Release all allocated resources and finish all pending operations.
     */
    private void release() {
        // Note that mNativeMediaDrmBridge may have already been reset (see destroy()).

        assert mMediaDrm != null;

        // Reject all pending session creation.
        for (PendingCreateSessionData data : mPendingCreateSessionDataQueue) {
            onPromiseRejected(data.promiseId(), "Create session aborted.");
        }
        mPendingCreateSessionDataQueue.clear();
        mPendingCreateSessionDataQueue = null;

        // Close all open sessions.
        for (SessionId sessionId : mSessionManager.getAllSessionIds()) {
            try {
                // Some implementations don't have removeKeys, crbug/475632
                mMediaDrm.removeKeys(sessionId.drmId());
            } catch (Exception e) {
                Log.e(TAG, "removeKeys failed: ", e);
            }

            closeSessionNoException(sessionId);
            onSessionClosed(sessionId);
        }
        mSessionManager = new MediaDrmSessionManager();

        // Close mMediaCryptoSession if it's open or notify MediaCrypto
        // creation failure if it's never successfully opened.
        if (mMediaCryptoSession == null) {
            // MediaCrypto never notified. Notify a null one now.
            onMediaCryptoReady(null);
        } else {
            closeSessionNoException(mMediaCryptoSession);
            mMediaCryptoSession = null;
        }

        // Fail device credentials resetting.
        if (mResetDeviceCredentialsPending) {
            mResetDeviceCredentialsPending = false;
            onResetDeviceCredentialsCompleted(false);
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
     * @param keyType Key type for the requested key.
     * @param optionalParameters Optional parameters to pass to the DRM plugin.
     *
     * @return the key request.
     */
    private MediaDrm.KeyRequest getKeyRequest(SessionId sessionId, byte[] data, String mime,
            int keyType, HashMap<String, String> optionalParameters)
            throws android.media.NotProvisionedException {
        assert mMediaDrm != null;
        assert mMediaCryptoSession != null;
        assert !mProvisioningPending;

        if (optionalParameters == null) {
            optionalParameters = new HashMap<String, String>();
        }

        MediaDrm.KeyRequest request = null;

        try {
            request = mMediaDrm.getKeyRequest(
                    sessionId.drmId(), data, mime, keyType, optionalParameters);
        } catch (IllegalStateException e) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP && e
                    instanceof android.media.MediaDrm.MediaDrmStateException) {
                // See b/21307186 for details.
                Log.e(TAG, "MediaDrmStateException fired during getKeyRequest().", e);
            }
        }

        String result = (request != null) ? "successed" : "failed";
        Log.d(TAG, "getKeyRequest %s!", result);

        return request;
    }

    /**
     * Save data to |mPendingCreateSessionDataQueue| so that we can resume the
     * createSession() call later.
     *
     * @param initData Data needed to generate the key request.
     * @param mime Mime type.
     * @param keyType Key type.
     * @param optionalParameters Optional parameters to pass to the DRM plugin.
     * @param promiseId Promise ID for the createSession() call.
     */
    private void savePendingCreateSessionData(byte[] initData, String mime, int keyType,
            HashMap<String, String> optionalParameters, long promiseId) {
        Log.d(TAG, "savePendingCreateSessionData()");

        mPendingCreateSessionDataQueue.offer(new PendingCreateSessionData(
                initData, mime, keyType, optionalParameters, promiseId));
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
            int keyType = pendingData.keyType();
            HashMap<String, String> optionalParameters = pendingData.optionalParameters();
            long promiseId = pendingData.promiseId();
            createSession(initData, mime, keyType, optionalParameters, promiseId);
        }
    }

    /**
     * createSession interface to be called from native using primitive types.
     * @see createSession(byte[], String, HashMap<String, String>, long)
     */
    @CalledByNative
    private void createSessionFromNative(byte[] initData, String mime, int keyType,
            String[] optionalParamsArray, long promiseId) {
        HashMap<String, String> optionalParameters = new HashMap<String, String>();
        if (optionalParamsArray != null) {
            if (optionalParamsArray.length % 2 != 0) {
                throw new IllegalArgumentException(
                        "Additional data array doesn't have equal keys/values");
            }
            for (int i = 0; i < optionalParamsArray.length; i += 2) {
                optionalParameters.put(optionalParamsArray[i], optionalParamsArray[i + 1]);
            }
        }
        createSession(initData, mime, keyType, optionalParameters, promiseId);
    }

    /**
     * Create a session, and generate a request with |initData| and |mime|.
     *
     * @param initData Data needed to generate the key request.
     * @param mime Mime type.
     * @param keyType Key type.
     * @param optionalParameters Additional data to pass to getKeyRequest.
     * @param promiseId Promise ID for this call.
     */
    private void createSession(byte[] initData, String mime, int keyType,
            HashMap<String, String> optionalParameters, long promiseId) {
        Log.d(TAG, "createSession()");

        if (mMediaDrm == null) {
            Log.e(TAG, "createSession() called when MediaDrm is null.");
            onPromiseRejected(promiseId, "MediaDrm released previously.");
            return;
        }

        if (mProvisioningPending) {
            savePendingCreateSessionData(initData, mime, keyType, optionalParameters, promiseId);
            return;
        }

        assert mMediaCryptoSession != null;

        boolean newSessionOpened = false;
        SessionId sessionId = null;
        try {
            byte[] drmId = openSession();
            if (drmId == null) {
                onPromiseRejected(promiseId, "Open session failed.");
                return;
            }
            newSessionOpened = true;
            assert keyType == MediaDrm.KEY_TYPE_STREAMING || keyType == MediaDrm.KEY_TYPE_OFFLINE;
            sessionId = (keyType == MediaDrm.KEY_TYPE_OFFLINE)
                    ? SessionId.createPersistentSessionId(drmId)
                    : SessionId.createTemporarySessionId(drmId);

            MediaDrm.KeyRequest request =
                    getKeyRequest(sessionId, initData, mime, keyType, optionalParameters);
            if (request == null) {
                closeSessionNoException(sessionId);
                onPromiseRejected(promiseId, "Generate request failed.");
                return;
            }

            // Success!
            Log.d(TAG, "createSession(): Session (%s) created.", sessionId.toHexString());
            onPromiseResolvedWithSession(promiseId, sessionId);
            onSessionMessage(sessionId, request);
            mSessionManager.put(sessionId, mime, keyType);
        } catch (android.media.NotProvisionedException e) {
            Log.e(TAG, "Device not provisioned", e);
            if (newSessionOpened) {
                closeSessionNoException(sessionId);
            }
            savePendingCreateSessionData(initData, mime, keyType, optionalParameters, promiseId);
            startProvisioning();
        }
    }

    /**
     * Search and return the SessionId for raw EME/DRM session id.
     *
     * @param emeId Raw EME session Id.
     * @return SessionId of |emeId| if exists and isn't a MediaCryptoSession, null otherwise.
     */
    private SessionId getSessionIdByEmeId(byte[] emeId) {
        if (mMediaCryptoSession == null) {
            Log.e(TAG, "Session doesn't exist because media crypto session is not created.");
            return null;
        }

        SessionId sessionId = mSessionManager.getSessionIdByEmeId(emeId);
        if (sessionId == null) {
            return null;
        }

        assert !mMediaCryptoSession.isEqual(sessionId);

        return sessionId;
    }

    /**
     * Similar with getSessionIdByEmeId, just search for raw DRM session id.
     */
    private SessionId getSessionIdByDrmId(byte[] drmId) {
        if (mMediaCryptoSession == null) {
            Log.e(TAG, "Session doesn't exist because media crypto session is not created.");
            return null;
        }

        SessionId sessionId = mSessionManager.getSessionIdByDrmId(drmId);
        if (sessionId == null) {
            return null;
        }

        assert !mMediaCryptoSession.isEqual(sessionId);

        return sessionId;
    }

    /**
     * Close a session that was previously created by createSession().
     *
     * @param emeSessionId ID of session to be closed.
     * @param promiseId Promise ID of this call.
     */
    @CalledByNative
    private void closeSession(byte[] emeSessionId, long promiseId) {
        Log.d(TAG, "closeSession()");
        if (mMediaDrm == null) {
            onPromiseRejected(promiseId, "closeSession() called when MediaDrm is null.");
            return;
        }

        SessionId sessionId = getSessionIdByEmeId(emeSessionId);
        if (sessionId == null) {
            onPromiseRejected(promiseId,
                    "Invalid sessionId in closeSession(): " + SessionId.toHexString(emeSessionId));
            return;
        }

        try {
            // Some implementations don't have removeKeys, crbug/475632
            mMediaDrm.removeKeys(sessionId.drmId());
        } catch (Exception e) {
            Log.e(TAG, "removeKeys failed: ", e);
        }

        closeSessionNoException(sessionId);
        mSessionManager.remove(sessionId);
        onPromiseResolved(promiseId);
        onSessionClosed(sessionId);
        Log.d(TAG, "Session %s closed", sessionId.toHexString());
    }

    /**
     * Close the session without worry about the exception, because some
     * implementations let this method throw exception, crbug/611865.
     */
    private void closeSessionNoException(SessionId sessionId) {
        try {
            mMediaDrm.closeSession(sessionId.drmId());
        } catch (Exception e) {
            Log.e(TAG, "closeSession failed: ", e);
        }
    }

    /**
     * Update a session with response.
     *
     * @param emeSessionId Reference ID of session to be updated.
     * @param response Response data from the server.
     * @param promiseId Promise ID of this call.
     */
    @CalledByNative
    private void updateSession(byte[] emeSessionId, byte[] response, final long promiseId) {
        Log.d(TAG, "updateSession()");
        if (mMediaDrm == null) {
            onPromiseRejected(promiseId, "updateSession() called when MediaDrm is null.");
            return;
        }

        final SessionId sessionId = getSessionIdByEmeId(emeSessionId);
        if (sessionId == null) {
            assert false; // Should never happen.
            onPromiseRejected(promiseId,
                    "Invalid session in updateSession: " + SessionId.toHexString(emeSessionId));
            return;
        }

        try {
            SessionInfo sessionInfo = mSessionManager.get(sessionId);
            byte[] keySetId = mMediaDrm.provideKeyResponse(sessionId.drmId(), response);

            if (keySetId != null && keySetId.length > 0) {
                assert sessionInfo.keyType() == MediaDrm.KEY_TYPE_OFFLINE;
                mSessionManager.setKeySetId(sessionId, keySetId, new Callback<Boolean>() {
                    @Override
                    public void onResult(Boolean success) {
                        onKeyUpdated(sessionId, promiseId, success);
                    }
                });
            } else {
                // This can be either temporary license update or server certificate update.
                onKeyUpdated(sessionId, promiseId, true);
            }

            return;
        } catch (android.media.NotProvisionedException e) {
            // TODO(xhwang): Should we handle this?
            Log.e(TAG, "failed to provide key response", e);
        } catch (android.media.DeniedByServerException e) {
            Log.e(TAG, "failed to provide key response", e);
        } catch (java.lang.IllegalStateException e) {
            Log.e(TAG, "failed to provide key response", e);
        }
        onPromiseRejected(promiseId, "Update session failed.");
        release();
    }

    private void onKeyUpdated(SessionId sessionId, long promiseId, boolean success) {
        if (!success) {
            onPromiseRejected(promiseId, "failed to update key after response accepted");
            return;
        }

        Log.d(TAG, "Key successfully added for session %s", sessionId.toHexString());
        onPromiseResolved(promiseId);

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            onSessionKeysChange(
                    sessionId, getDummyKeysInfo(MediaDrm.KeyStatus.STATUS_USABLE).toArray(), true);
        }
    }

    /**
     * Return the security level of this DRM object.
     */
    @CalledByNative
    private String getSecurityLevel() {
        if (mMediaDrm == null || !isWidevine()) {
            Log.e(TAG, "getSecurityLevel(): MediaDrm is null or security level is not supported.");
            return null;
        }
        return mMediaDrm.getPropertyString("securityLevel");
    }

    private void startProvisioning() {
        if (mProvisioningPending) {
            Log.d(TAG, "startProvisioning: another provisioning is in progress, returning");
            return;
        }

        Log.d(TAG, "startProvisioning");
        mProvisioningPending = true;
        assert mMediaDrm != null;
        MediaDrm.ProvisionRequest request = mMediaDrm.getProvisionRequest();

        if (isNativeMediaDrmBridgeValid()) {
            nativeOnStartProvisioning(
                    mNativeMediaDrmBridge, request.getDefaultUrl(), request.getData());
        }
    }

    /**
     * Called when the provision response is received.
     *
     * @param isResponseReceived Flag set to true if commincation with provision server was
     * successful.
     * @param response Response data from the provision server.
     */
    @CalledByNative
    private void processProvisionResponse(boolean isResponseReceived, byte[] response) {
        Log.d(TAG, "processProvisionResponse()");

        // If |mMediaDrm| is released, there is no need to callback native.
        if (mMediaDrm == null) {
            return;
        }

        assert mProvisioningPending;
        mProvisioningPending = false;

        boolean success = isResponseReceived ? provideProvisionResponse(response) : false;

        if (mResetDeviceCredentialsPending) {
            onResetDeviceCredentialsCompleted(success);
            mResetDeviceCredentialsPending = false;
        }

        if (!success || (mMediaCryptoSession == null && !createMediaCrypto())) {
            release();
            return;
        }

        processPendingCreateSessionData();
    }

    /**
     * Provides the provision response to MediaDrm.
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

    // Helper functions to make native calls.

    private void onMediaCryptoReady(MediaCrypto mediaCrypto) {
        if (isNativeMediaDrmBridgeValid()) {
            nativeOnMediaCryptoReady(mNativeMediaDrmBridge, mediaCrypto);
        }
    }

    private void onPromiseResolved(final long promiseId) {
        if (isNativeMediaDrmBridgeValid()) {
            nativeOnPromiseResolved(mNativeMediaDrmBridge, promiseId);
        }
    }

    private void onPromiseResolvedWithSession(final long promiseId, final SessionId sessionId) {
        if (isNativeMediaDrmBridgeValid()) {
            nativeOnPromiseResolvedWithSession(mNativeMediaDrmBridge, promiseId, sessionId.emeId());
        }
    }

    private void onPromiseRejected(final long promiseId, final String errorMessage) {
        Log.e(TAG, "onPromiseRejected: %s", errorMessage);
        if (isNativeMediaDrmBridgeValid()) {
            nativeOnPromiseRejected(mNativeMediaDrmBridge, promiseId, errorMessage);
        }
    }

    @TargetApi(Build.VERSION_CODES.M)
    private void onSessionMessage(final SessionId sessionId, final MediaDrm.KeyRequest request) {
        if (!isNativeMediaDrmBridgeValid()) return;

        int requestType = MediaDrm.KeyRequest.REQUEST_TYPE_INITIAL;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            requestType = request.getRequestType();
        } else {
            // Prior to M, getRequestType() is not supported. Do our best guess here: Assume
            // requests with a URL are renewals and all others are initial requests.
            requestType = request.getDefaultUrl().isEmpty()
                    ? MediaDrm.KeyRequest.REQUEST_TYPE_INITIAL
                    : MediaDrm.KeyRequest.REQUEST_TYPE_RENEWAL;
        }

        nativeOnSessionMessage(
                mNativeMediaDrmBridge, sessionId.emeId(), requestType, request.getData());
    }

    private void onSessionClosed(final SessionId sessionId) {
        if (isNativeMediaDrmBridgeValid()) {
            nativeOnSessionClosed(mNativeMediaDrmBridge, sessionId.emeId());
        }
    }

    private void onSessionKeysChange(final SessionId sessionId, final Object[] keysInfo,
            final boolean hasAdditionalUsableKey) {
        if (isNativeMediaDrmBridgeValid()) {
            nativeOnSessionKeysChange(
                    mNativeMediaDrmBridge, sessionId.emeId(), keysInfo, hasAdditionalUsableKey);
        }
    }

    private void onSessionExpirationUpdate(final SessionId sessionId, final long expirationTime) {
        if (isNativeMediaDrmBridgeValid()) {
            nativeOnSessionExpirationUpdate(
                    mNativeMediaDrmBridge, sessionId.emeId(), expirationTime);
        }
    }

    private void onResetDeviceCredentialsCompleted(final boolean success) {
        if (isNativeMediaDrmBridgeValid()) {
            nativeOnResetDeviceCredentialsCompleted(mNativeMediaDrmBridge, success);
        }
    }

    @MainDex
    private class EventListener implements MediaDrm.OnEventListener {
        @Override
        public void onEvent(
                MediaDrm mediaDrm, byte[] drmSessionId, int event, int extra, byte[] data) {
            if (drmSessionId == null) {
                Log.e(TAG, "EventListener: Null session.");
                return;
            }
            SessionId sessionId = getSessionIdByDrmId(drmSessionId);

            if (sessionId == null) {
                Log.e(TAG, "EventListener: Invalid session %s",
                        SessionId.toHexString(drmSessionId));
                return;
            }

            switch(event) {
                case MediaDrm.EVENT_KEY_REQUIRED:
                    Log.d(TAG, "MediaDrm.EVENT_KEY_REQUIRED");
                    if (mProvisioningPending) {
                        return;
                    }
                    SessionInfo sessionInfo = mSessionManager.get(sessionId);
                    MediaDrm.KeyRequest request = null;
                    try {
                        request = getKeyRequest(sessionId, data, sessionInfo.mimeType(),
                                sessionInfo.keyType(), null);
                    } catch (android.media.NotProvisionedException e) {
                        Log.e(TAG, "Device not provisioned", e);
                        startProvisioning();
                        return;
                    }
                    if (request != null) {
                        onSessionMessage(sessionId, request);
                    } else {
                        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
                            onSessionKeysChange(sessionId,
                                    getDummyKeysInfo(MediaDrm.KeyStatus.STATUS_INTERNAL_ERROR)
                                            .toArray(),
                                    false);
                        }
                        Log.e(TAG, "EventListener: getKeyRequest failed.");
                        return;
                    }
                    break;
                case MediaDrm.EVENT_KEY_EXPIRED:
                    Log.d(TAG, "MediaDrm.EVENT_KEY_EXPIRED");
                    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
                        onSessionKeysChange(sessionId,
                                getDummyKeysInfo(MediaDrm.KeyStatus.STATUS_EXPIRED).toArray(),
                                false);
                    }
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

    @TargetApi(Build.VERSION_CODES.M)
    @MainDex
    private class KeyStatusChangeListener implements MediaDrm.OnKeyStatusChangeListener {
        private List<KeyStatus> getKeysInfo(List<MediaDrm.KeyStatus> keyInformation) {
            List<KeyStatus> keysInfo = new ArrayList<KeyStatus>();
            for (MediaDrm.KeyStatus keyStatus : keyInformation) {
                keysInfo.add(new KeyStatus(keyStatus.getKeyId(), keyStatus.getStatusCode()));
            }
            return keysInfo;
        }

        @Override
        public void onKeyStatusChange(MediaDrm md, byte[] drmSessionId,
                List<MediaDrm.KeyStatus> keyInformation, boolean hasNewUsableKey) {
            SessionId sessionId = getSessionIdByDrmId(drmSessionId);

            assert sessionId != null;

            Log.d(TAG, "KeysStatusChange: " + sessionId.toHexString() + ", " + hasNewUsableKey);

            onSessionKeysChange(sessionId, getKeysInfo(keyInformation).toArray(), hasNewUsableKey);
        }
    }

    @TargetApi(Build.VERSION_CODES.M)
    @MainDex
    private class ExpirationUpdateListener implements MediaDrm.OnExpirationUpdateListener {
        @Override
        public void onExpirationUpdate(MediaDrm md, byte[] drmSessionId, long expirationTime) {
            SessionId sessionId = getSessionIdByDrmId(drmSessionId);

            assert sessionId != null;

            Log.d(TAG, "ExpirationUpdate: " + sessionId.toHexString() + ", " + expirationTime);
            onSessionExpirationUpdate(sessionId, expirationTime);
        }
    }

    // Native functions. At the native side, must post the task immediately to
    // avoid reentrancy issues.
    private native void nativeOnMediaCryptoReady(
            long nativeMediaDrmBridge, MediaCrypto mediaCrypto);

    private native void nativeOnStartProvisioning(
            long nativeMediaDrmBridge, String defaultUrl, byte[] requestData);

    private native void nativeOnPromiseResolved(long nativeMediaDrmBridge, long promiseId);
    private native void nativeOnPromiseResolvedWithSession(
            long nativeMediaDrmBridge, long promiseId, byte[] emeSessionId);
    private native void nativeOnPromiseRejected(
            long nativeMediaDrmBridge, long promiseId, String errorMessage);

    private native void nativeOnSessionMessage(
            long nativeMediaDrmBridge, byte[] emeSessionId, int requestType, byte[] message);
    private native void nativeOnSessionClosed(long nativeMediaDrmBridge, byte[] emeSessionId);
    private native void nativeOnSessionKeysChange(long nativeMediaDrmBridge, byte[] emeSessionId,
            Object[] keysInfo, boolean hasAdditionalUsableKey);
    private native void nativeOnSessionExpirationUpdate(
            long nativeMediaDrmBridge, byte[] emeSessionId, long expirationTime);

    private native void nativeOnResetDeviceCredentialsCompleted(
            long nativeMediaDrmBridge, boolean success);
}
