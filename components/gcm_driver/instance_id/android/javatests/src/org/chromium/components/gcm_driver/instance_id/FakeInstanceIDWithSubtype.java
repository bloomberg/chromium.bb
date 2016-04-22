// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.gcm_driver.instance_id;

import android.content.Context;
import android.os.Bundle;
import android.os.Looper;

import com.google.android.gms.iid.InstanceID;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.io.IOException;

import java.util.HashMap;
import java.util.Map;
import java.util.Random;

/**
 * Fake for InstanceIDWithSubtype. Doesn't hit the network or filesystem (so instance IDs don't
 * survive restarts, and sending messages to tokens via the GCM server won't work).
 */
@JNINamespace("instance_id")
public class FakeInstanceIDWithSubtype extends InstanceIDWithSubtype {
    private String mId = null;
    private long mCreationTime = 0;
    private Map<String, String> mTokens = new HashMap<>();

    /**
     * Enable this in all InstanceID tests to use this fake instead of hitting the network/disk.
     * @return The previous value.
     */
    @CalledByNative
    public static boolean clearDataAndSetEnabled(boolean enable) {
        synchronized (InstanceID.class) {
            sSubtypeInstances.clear();
            boolean wasEnabled = sFakeFactoryForTesting != null;
            if (enable) {
                sFakeFactoryForTesting = new FakeFactory() {
                    @Override
                    public InstanceIDWithSubtype create(Context context, String subtype) {
                        return new FakeInstanceIDWithSubtype(context, subtype);
                    }
                };
            } else {
                sFakeFactoryForTesting = null;
            }
            return wasEnabled;
        }
    }

    private FakeInstanceIDWithSubtype(Context context, String subtype) {
        super(context, subtype);
    }

    @Override
    public String getId() {
        if (mId == null) {
            mCreationTime = System.currentTimeMillis();
            mId = randomBase64(11 /* length */);
        }
        return mId;
    }

    @Override
    public long getCreationTime() {
        return mCreationTime;
    }

    @Override
    public String getToken(String authorizedEntity, String scope) throws IOException {
        return getToken(authorizedEntity, scope, null);
    }

    @Override
    public String getToken(String authorizedEntity, String scope, Bundle extras)
            throws IOException {
        if (Looper.getMainLooper() == Looper.myLooper()) {
            throw new IOException(InstanceID.ERROR_MAIN_THREAD);
        }
        String key = getSubtype() + ',' + authorizedEntity + ',' + scope;
        String token = mTokens.get(key);
        if (token == null) {
            getId();
            token = mId + ':' + randomBase64(140 /* length */);
            mTokens.put(key, token);
        }
        return token;
    }

    @Override
    public void deleteToken(String authorizedEntity, String scope) throws IOException {
        if (Looper.getMainLooper() == Looper.myLooper()) {
            throw new IOException(InstanceID.ERROR_MAIN_THREAD);
        }
        String key = getSubtype() + ',' + authorizedEntity + ',' + scope;
        mTokens.remove(key);
        // Calling deleteToken causes ID to be generated; can be observed though getCreationTime.
        getId();
    }

    @Override
    public void deleteInstanceID() throws IOException {
        synchronized (InstanceID.class) {
            sSubtypeInstances.remove(getSubtype());
            if (Looper.getMainLooper() == Looper.myLooper()) {
                throw new IOException(InstanceID.ERROR_MAIN_THREAD);
            }
            mTokens.clear();
            mCreationTime = 0;
            mId = null;
        }
    }

    /** Returns a random base64url encoded string. */
    private static String randomBase64(int encodedLength) {
        // It would probably make more sense for this method to produce fixed-length plaintext,
        // rather than fixed-length encodings that correspond to variable-length plaintext.
        // But the added randomness helps avoid us depending on the length of tokens GCM gives us.
        final String base64urlAlphabet =
                "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_";
        Random random = new Random();
        StringBuilder sb = new StringBuilder(encodedLength);
        for (int i = 0; i < encodedLength; i++) {
            int index = random.nextInt(base64urlAlphabet.length());
            sb.append(base64urlAlphabet.charAt(index));
        }
        return sb.toString();
    }
}