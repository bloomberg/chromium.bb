// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Provides access to native implementations of content storage and journal storage.
 */
@JNINamespace("feed")
public class FeedStorageBridge {
    private long mNativeFeedStorageBridge;

    /**
     * Creates a {@link FeedStorageBridge} for accessing native content and journal storage
     * implementation for the current user, and initial native side bridge.
     *
     * @param profile {@link Profile} of the user we are rendering the Feed for.
     */
    public FeedStorageBridge() {}

    /**
     * Inits native side bridge.
     *
     * @param profile {@link Profile} of the user we are rendering the Feed for.
     */
    public void init(Profile profile) {
        mNativeFeedStorageBridge = nativeInit(profile);
    }

    /** Cleans up native half of this bridge. */
    public void destroy() {
        assert mNativeFeedStorageBridge != 0;
        nativeDestroy(mNativeFeedStorageBridge);
        mNativeFeedStorageBridge = 0;
    }

    public void loadContent(List<String> keys, Callback<Map<String, byte[]>> callback) {
        assert mNativeFeedStorageBridge != 0;
        String[] keysArray = keys.toArray(new String[keys.size()]);
        nativeLoadContent(mNativeFeedStorageBridge, keysArray, callback);
    }

    public void loadContentByPrefix(String prefix, Callback<Map<String, byte[]>> callback) {
        assert mNativeFeedStorageBridge != 0;
        nativeLoadContentByPrefix(mNativeFeedStorageBridge, prefix, callback);
    }

    public void loadAllContentKeys(Callback<List<String>> callback) {
        assert mNativeFeedStorageBridge != 0;
        nativeLoadAllContentKeys(mNativeFeedStorageBridge, callback);
    }

    public void saveContent(String[] keys, byte[][] data, Callback<Boolean> callback) {
        assert mNativeFeedStorageBridge != 0;
        nativeSaveContent(mNativeFeedStorageBridge, keys, data, callback);
    }

    public void deleteContent(List<String> keys, Callback<Boolean> callback) {
        assert mNativeFeedStorageBridge != 0;
        String[] keysArray = keys.toArray(new String[keys.size()]);
        nativeDeleteContent(mNativeFeedStorageBridge, keysArray, callback);
    }

    public void deleteContentByPrefix(String prefix, Callback<Boolean> callback) {
        assert mNativeFeedStorageBridge != 0;
        nativeDeleteContentByPrefix(mNativeFeedStorageBridge, prefix, callback);
    }

    public void deleteAllContent(Callback<Boolean> callback) {
        assert mNativeFeedStorageBridge != 0;
        nativeDeleteAllContent(mNativeFeedStorageBridge, callback);
    }

    @CalledByNative
    private static Object createKeyAndDataMap(String[] keys, byte[][] data) {
        assert keys.length == data.length;
        Map<String, byte[]> valueMap = new HashMap<>(keys.length);
        for (int i = 0; i < keys.length && i < data.length; ++i) {
            valueMap.put(keys[i], data[i]);
        }
        return valueMap;
    }

    @CalledByNative
    private static List<String> createJavaList(String[] keys) {
        return Arrays.asList(keys);
    }

    private native long nativeInit(Profile profile);
    private native void nativeDestroy(long nativeFeedStorageBridge);
    private native void nativeLoadContent(
            long nativeFeedStorageBridge, String[] keys, Callback<Map<String, byte[]>> callback);
    private native void nativeLoadContentByPrefix(
            long nativeFeedStorageBridge, String prefix, Callback<Map<String, byte[]>> callback);
    private native void nativeLoadAllContentKeys(
            long nativeFeedStorageBridge, Callback<List<String>> callback);
    private native void nativeSaveContent(
            long nativeFeedStorageBridge, String[] keys, byte[][] data, Callback<Boolean> callback);
    private native void nativeDeleteContent(
            long nativeFeedStorageBridge, String[] keys, Callback<Boolean> callback);
    private native void nativeDeleteContentByPrefix(
            long nativeFeedStorageBridge, String prefix, Callback<Boolean> callback);
    private native void nativeDeleteAllContent(
            long nativeFeedStorageBridge, Callback<Boolean> callback);
}
