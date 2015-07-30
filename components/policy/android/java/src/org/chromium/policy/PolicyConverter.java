// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.policy;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Allows converting Java policies, contained as key/value pairs in {@link android.os.Bundle}s to
 * native {@code PolicyBundle}s.
 *
 * This class is to be used to send key/value pairs to its native equivalent, that can then be used
 * to retrieve the native {@code PolicyBundle}.
 *
 * It should be created by calling {@link #create(long)} from the native code, and sending it back
 * to Java.
 */
@JNINamespace("policy::android")
public class PolicyConverter {
    private long mNativePolicyConverter;

    private PolicyConverter(long nativePolicyConverter) {
        mNativePolicyConverter = nativePolicyConverter;
    }

    /** Convert and send the key/value pair for a policy to the native {@code PolicyConverter}. */
    public void setPolicy(String key, Object value) {
        assert mNativePolicyConverter != 0;

        if (value instanceof Boolean) {
            nativeSetPolicyBoolean(mNativePolicyConverter, key, (Boolean) value);
            return;
        }
        if (value instanceof String) {
            nativeSetPolicyString(mNativePolicyConverter, key, (String) value);
            return;
        }
        if (value instanceof Integer) {
            nativeSetPolicyInteger(mNativePolicyConverter, key, (Integer) value);
            return;
        }
        if (value instanceof String[]) {
            nativeSetPolicyStringArray(mNativePolicyConverter, key, (String[]) value);
            return;
        }
        assert false : "Invalid setting " + value + " for key " + key;
    }

    @CalledByNative
    private static PolicyConverter create(long nativePolicyConverter) {
        return new PolicyConverter(nativePolicyConverter);
    }

    @CalledByNative
    private void onNativeDestroyed() {
        mNativePolicyConverter = 0;
    }

    private native void nativeSetPolicyBoolean(
            long nativePolicyConverter, String policyKey, boolean value);
    private native void nativeSetPolicyInteger(
            long nativePolicyConverter, String policyKey, int value);
    private native void nativeSetPolicyString(
            long nativePolicyConverter, String policyKey, String value);
    private native void nativeSetPolicyStringArray(
            long nativePolicyConverter, String policyKey, String[] value);
}
