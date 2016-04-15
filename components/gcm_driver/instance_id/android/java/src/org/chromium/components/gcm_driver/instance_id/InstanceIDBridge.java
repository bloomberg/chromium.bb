// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.gcm_driver.instance_id;

import android.content.Context;
import android.os.AsyncTask;
import android.os.Bundle;

import com.google.android.gms.iid.InstanceID;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.io.IOException;

/**
 * Wraps InstanceID and InstanceIDWithSubtype so they can be used over JNI.
 * Performs disk/network operations on a background thread and replies asynchronously.
 */
@JNINamespace("instance_id")
public class InstanceIDBridge {
    /** Underlying InstanceID. May be shared by multiple InstanceIDBridges. */
    private final InstanceID mInstanceID;
    private long mNativeInstanceIDAndroid;

    private InstanceIDBridge(
            long nativeInstanceIDAndroid, Context context, String subtype) {
        mInstanceID = InstanceIDWithSubtype.getInstance(context, subtype);
        mNativeInstanceIDAndroid = nativeInstanceIDAndroid;
    }

    /**
     * Returns a wrapped {@link InstanceIDWithSubtype}. Multiple InstanceIDBridge instances may
     * share an underlying InstanceIDWithSubtype.
     */
    @CalledByNative
    public static InstanceIDBridge create(
            long nativeInstanceIDAndroid, Context context, String subtype) {
        // TODO(johnme): This should also be async.
        return new InstanceIDBridge(nativeInstanceIDAndroid, context, subtype);
    }

    /**
     * Called when our C++ counterpart is destroyed. Clears the handle to our native C++ object,
     * ensuring it's not called by pending async tasks.
     */
    @CalledByNative
    private void destroy() {
        mNativeInstanceIDAndroid = 0;
    }

    /** Wrapper for {@link InstanceID#getId}. */
    @CalledByNative
    public String getId() {
        // TODO(johnme): This should also be async.
        return mInstanceID.getId();
    }

    /** Wrapper for {@link InstanceID#getCreationTime}. */
    @CalledByNative
    public long getCreationTime() {
        // TODO(johnme): This should also be async.
        return mInstanceID.getCreationTime();
    }

    /** Async wrapper for {@link InstanceID#getToken(String, String, Bundle)}. */
    @CalledByNative
    private void getToken(final int requestId, final String authorizedEntity, final String scope,
            String[] extrasStrings) {
        final Bundle extras = new Bundle();
        assert extrasStrings.length % 2 == 0;
        for (int i = 0; i < extrasStrings.length; i += 2) {
            extras.putString(extrasStrings[i], extrasStrings[i + 1]);
        }
        new AsyncTask<Void, Void, String>() {
            @Override
            protected String doInBackground(Void... params) {
                try {
                    return mInstanceID.getToken(authorizedEntity, scope, extras);
                } catch (IOException ex) {
                    return "";
                }
            }
            @Override
            protected void onPostExecute(String token) {
                if (mNativeInstanceIDAndroid != 0) {
                    nativeDidGetToken(mNativeInstanceIDAndroid, requestId, token);
                }
            }
        }.execute();
    }

    /** Async wrapper for {@link InstanceID#deleteToken(String, String)}. */
    @CalledByNative
    private void deleteToken(
            final int requestId, final String authorizedEntity, final String scope) {
        new AsyncTask<Void, Void, Boolean>() {
            @Override
            protected Boolean doInBackground(Void... params) {
                try {
                    mInstanceID.deleteToken(authorizedEntity, scope);
                    return true;
                } catch (IOException ex) {
                    return false;
                }
            }
            @Override
            protected void onPostExecute(Boolean success) {
                if (mNativeInstanceIDAndroid != 0) {
                    nativeDidDeleteToken(mNativeInstanceIDAndroid, requestId, success);
                }
            }
        }.execute();
    }

    /** Async wrapper for {@link InstanceID#deleteInstanceID}. */
    @CalledByNative
    private void deleteInstanceID(final int requestId) {
        new AsyncTask<Void, Void, Boolean>() {
            @Override
            protected Boolean doInBackground(Void... params) {
                try {
                    mInstanceID.deleteInstanceID();
                    return true;
                } catch (IOException ex) {
                    return false;
                }
            }
            @Override
            protected void onPostExecute(Boolean success) {
                if (mNativeInstanceIDAndroid != 0) {
                    nativeDidDeleteID(mNativeInstanceIDAndroid, requestId, success);
                }
            }
        }.execute();
    }

    private native void nativeDidGetToken(
            long nativeInstanceIDAndroid, int requestId, String token);
    private native void nativeDidDeleteToken(
            long nativeInstanceIDAndroid, int requestId, boolean success);
    private native void nativeDidDeleteID(
            long nativeInstanceIDAndroid, int requestId, boolean success);
}