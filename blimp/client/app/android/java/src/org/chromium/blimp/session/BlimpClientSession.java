// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.session;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.blimp.R;
import org.chromium.blimp.assignment.Result;

import java.util.ArrayList;
import java.util.List;

/**
 * The Java representation of a native BlimpClientSession.  This is primarily used to provide access
 * to the native session methods and to facilitate passing a BlimpClientSession object between Java
 * classes with native counterparts.
 */
@JNINamespace("blimp::client")
public class BlimpClientSession {
    /**
     * An observer for when the session needs to notify the UI about the state of the Blimp session.
     */
    public interface ConnectionObserver {
        /**
         * Called when an engine assignment has been successful or failed.
         * @param result                     The result code of the assignment.  See
         *                                   assignment_source.h for details.  Maps to a value in
         *                                   {@link Result}.
         * @param suggestedMessageResourceId A suggested resource id for a string to display to the
         *                                   user if necessary.
         * @param engineInfo                 IP address and version of blimp engine.
         */
        void onAssignmentReceived(
                int result, int suggestedMessageResourceId, EngineInfo engineInfo);

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

    private final String mAssignerUrl;
    private final List<ConnectionObserver> mObservers;
    private long mNativeBlimpClientSessionAndroidPtr;

    public BlimpClientSession(String assignerUrl) {
        mAssignerUrl = assignerUrl;
        mObservers = new ArrayList<ConnectionObserver>();
        mNativeBlimpClientSessionAndroidPtr = nativeInit(mAssignerUrl);
    }

    /**
     * Add an observer to be notified about the connection status.
     * @param observer The observer to add.
     */
    public void addObserver(ConnectionObserver observer) {
        mObservers.add(observer);
    }

    /**
     * Remove an observer from the observer list.
     * @param observer The observer to remove.
     */
    public void removeObserver(ConnectionObserver observer) {
        mObservers.remove(observer);
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

        mObservers.clear();
        nativeDestroy(mNativeBlimpClientSessionAndroidPtr);
        mNativeBlimpClientSessionAndroidPtr = 0;
    }

    // Methods that are called by native via JNI.
    @CalledByNative
    private void onAssignmentReceived(int result, String engineIP, String engineVersion) {
        if (mObservers.isEmpty()) return;

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
        for (ConnectionObserver observer : mObservers) {
            observer.onAssignmentReceived(result, resultMessageResourceId,
                    new EngineInfo(engineIP, engineVersion, mAssignerUrl));
        }
    }

    @CalledByNative
    void onConnected() {
        for (ConnectionObserver observer : mObservers) {
            observer.onConnected();
        }
    }

    @CalledByNative
    void onDisconnected(String reason) {
        for (ConnectionObserver observer : mObservers) {
            observer.onDisconnected(reason);
        }
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
