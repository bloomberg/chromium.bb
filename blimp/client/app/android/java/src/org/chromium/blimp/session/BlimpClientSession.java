// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.session;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.blimp.R;
import org.chromium.blimp.assignment.Result;

/**
 * The Java representation of a native BlimpClientSession.  This is primarily used to provide access
 * to the native session methods and to facilitate passing a BlimpClientSession object between Java
 * classes with native counterparts.
 */
@JNINamespace("blimp::client")
public class BlimpClientSession {
    /**
     * A callback for when the session needs to notify the UI about the state of the Blimp session.
     */
    public interface Callback {
        /**
         * Called when an engine assignment has been successful or failed.
         * @param result                     The result code of the assignment.  See
         *                                   assignment_source.h for details.  Maps to a value in
         *                                   {@link Result}.
         * @param suggestedMessageResourceId A suggested resource id for a string to display to the
         *                                   user if necessary.
         */
        void onAssignmentReceived(int result, int suggestedMessageResourceId);

        /**
         * Called when a connection to the engine was made successfully.
         */
        void onConnected();

        /**
         * Called when the engine connection was dropped.
         * @param reason The string-based error code.
         *               See net/base/net_errors.h for a complete list of codes
         *               and their explanations.
         */
        void onDisconnected(String reason);
    }

    private static final String DEFAULT_ASSIGNER_URL =
            "https://blimp-pa.googleapis.com/v1/assignment";

    private final Callback mCallback;
    private long mNativeBlimpClientSessionAndroidPtr;

    public BlimpClientSession(Callback callback) {
        mCallback = callback;
        mNativeBlimpClientSessionAndroidPtr = nativeInit(DEFAULT_ASSIGNER_URL);
    }

    /**
     * Retrieves an assignment and uses it to connect to the engine.
     * @param token A OAuth2 access token for the account requesting access.
     */
    public void connect(String token) {
        nativeConnect(mNativeBlimpClientSessionAndroidPtr, token);
    }

    /**
     * Destroys the native BlimpClientSession.  This class should not be used after this is called.
     */
    public void destroy() {
        if (mNativeBlimpClientSessionAndroidPtr == 0) return;

        nativeDestroy(mNativeBlimpClientSessionAndroidPtr);
        mNativeBlimpClientSessionAndroidPtr = 0;
    }

    // Methods that are called by native via JNI.
    @CalledByNative
    private void onAssignmentReceived(int result) {
        if (mCallback == null) return;

        int resultMessageResourceId = R.string.assignment_failure_unknown;
        switch (result) {
            case Result.OK:
                resultMessageResourceId = R.string.assignment_success;
                break;
            case Result.BAD_REQUEST:
                resultMessageResourceId = R.string.assignment_failure_bad_request;
                break;
            case Result.BAD_RESPONSE:
                resultMessageResourceId = R.string.assignment_failure_bad_response;
                break;
            case Result.INVALID_PROTOCOL_VERSION:
                resultMessageResourceId = R.string.assignment_failure_bad_version;
                break;
            case Result.EXPIRED_ACCESS_TOKEN:
                resultMessageResourceId = R.string.assignment_failure_expired_token;
                break;
            case Result.USER_INVALID:
                resultMessageResourceId = R.string.assignment_failure_user_invalid;
                break;
            case Result.OUT_OF_VMS:
                resultMessageResourceId = R.string.assignment_failure_out_of_vms;
                break;
            case Result.SERVER_ERROR:
                resultMessageResourceId = R.string.assignment_failure_server_error;
                break;
            case Result.SERVER_INTERRUPTED:
                resultMessageResourceId = R.string.assignment_failure_server_interrupted;
                break;
            case Result.NETWORK_FAILURE:
                resultMessageResourceId = R.string.assignment_failure_network;
                break;
            case Result.UNKNOWN:
            default:
                resultMessageResourceId = R.string.assignment_failure_unknown;
                break;
        }
        mCallback.onAssignmentReceived(result, resultMessageResourceId);
    }

    @CalledByNative
    void onConnected() {
        assert mCallback != null;
        mCallback.onConnected();
    }

    @CalledByNative
    void onDisconnected(String reason) {
        mCallback.onDisconnected(reason);
    }

    @CalledByNative
    private long getNativePtr() {
        assert mNativeBlimpClientSessionAndroidPtr != 0;
        return mNativeBlimpClientSessionAndroidPtr;
    }

    private native long nativeInit(String assignerUrl);
    private native void nativeConnect(long nativeBlimpClientSessionAndroid, String token);
    private native void nativeDestroy(long nativeBlimpClientSessionAndroid);
}
