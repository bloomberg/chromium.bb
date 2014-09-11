// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import org.chromium.base.CalledByNative;

/**
 * Wrapper that allows passing a Profile reference around in the Java layer.
 */
public class Profile {

    private long mNativeProfileAndroid;

    private Profile(long nativeProfileAndroid) {
        mNativeProfileAndroid = nativeProfileAndroid;
    }

    public static Profile getLastUsedProfile() {
        return (Profile) nativeGetLastUsedProfile();
    }

    public Profile getOriginalProfile() {
        return (Profile) nativeGetOriginalProfile(mNativeProfileAndroid);
    }

    public Profile getOffTheRecordProfile() {
        return (Profile) nativeGetOffTheRecordProfile(mNativeProfileAndroid);
    }

    public boolean hasOffTheRecordProfile() {
        return nativeHasOffTheRecordProfile(mNativeProfileAndroid);
    }

    public boolean isOffTheRecord() {
        return nativeIsOffTheRecord(mNativeProfileAndroid);
    }

    @CalledByNative
    private static Profile create(long nativeProfileAndroid) {
        return new Profile(nativeProfileAndroid);
    }

    @CalledByNative
    private void destroy() {
        mNativeProfileAndroid = 0;
    }

    @CalledByNative
    private long getNativePointer() {
        return mNativeProfileAndroid;
    }

    private static native Object nativeGetLastUsedProfile();
    private native Object nativeGetOriginalProfile(
            long nativeProfileAndroid);
    private native Object nativeGetOffTheRecordProfile(
            long nativeProfileAndroid);
    private native boolean nativeHasOffTheRecordProfile(
            long nativeProfileAndroid);
    private native boolean nativeIsOffTheRecord(
            long nativeProfileAndroid);
}
