// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import org.chromium.base.CalledByNative;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

/**
 * Utility class that interacts with native to retrieve and set website settings.
 */
public abstract class WebsitePreferenceBridge {
    private static final String LOG_TAG = "WebsiteSettingsUtils";

    /**
     * Interface for an object that listens to local storage info is ready callback.
     */
    public interface LocalStorageInfoReadyCallback {
        @CalledByNative("LocalStorageInfoReadyCallback")
        public void onLocalStorageInfoReady(HashMap map);
    }

    /**
     * Interface for an object that listens to storage info is ready callback.
     */
    public interface StorageInfoReadyCallback {
        @CalledByNative("StorageInfoReadyCallback")
        public void onStorageInfoReady(ArrayList array);
    }

    /**
     * Interface for an object that listens to storage info is cleared callback.
     */
    public interface StorageInfoClearedCallback {
        @CalledByNative("StorageInfoClearedCallback")
        public void onStorageInfoCleared();
    }

    @SuppressWarnings("unchecked")
    public static List<GeolocationInfo> getGeolocationInfo() {
        // Location can be managed by the custodian of a supervised account or by enterprise policy.
        boolean managedOnly = !PrefServiceBridge.getInstance().isAllowLocationUserModifiable();
        ArrayList<GeolocationInfo> list = new ArrayList<GeolocationInfo>();
        nativeGetGeolocationOrigins(list, managedOnly);
        return list;
    }

    @CalledByNative
    private static void insertGeolocationInfoIntoList(
            ArrayList<GeolocationInfo> list, String origin, String embedder) {
        list.add(new GeolocationInfo(origin, embedder));
    }

    @SuppressWarnings("unchecked")
    public static List<MidiInfo> getMidiInfo() {
        ArrayList<MidiInfo> list = new ArrayList<MidiInfo>();
        nativeGetMidiOrigins(list);
        return list;
    }

    @CalledByNative
    private static void insertMidiInfoIntoList(
            ArrayList<MidiInfo> list, String origin, String embedder) {
        list.add(new MidiInfo(origin, embedder));
    }

    public static List<CookieInfo> getCookieInfo() {
        boolean managedOnly = PrefServiceBridge.getInstance().isAcceptCookiesManaged();
        ArrayList<CookieInfo> list = new ArrayList<CookieInfo>();
        nativeGetCookieOrigins(list, managedOnly);
        return list;
    }

    @CalledByNative
    private static void insertCookieInfoIntoList(
            ArrayList<CookieInfo> list, String origin, String embedder) {
        list.add(new CookieInfo(origin, embedder));
    }

    @CalledByNative
    private static Object createStorageInfoList() {
        return new ArrayList<StorageInfo>();
    }

    @CalledByNative
    private static void insertStorageInfoIntoList(
            ArrayList<StorageInfo> list, String host, int type, long size) {
        list.add(new StorageInfo(host, type, size));
    }

    @CalledByNative
    private static Object createLocalStorageInfoMap() {
        return new HashMap<String, LocalStorageInfo>();
    }

    @SuppressWarnings("unchecked")
    @CalledByNative
    private static void insertLocalStorageInfoIntoMap(
            HashMap map, String origin, String fullOrigin, long size) {
        ((HashMap<String, LocalStorageInfo>) map).put(origin, new LocalStorageInfo(origin, size));
    }

    /**
     * @return List of all the origin/embedder combinations of protected media identifier
     *         permissions.
     */
    @SuppressWarnings("unchecked")
    public static List<ProtectedMediaIdentifierInfo> getProtectedMediaIdentifierInfo() {
        ArrayList<ProtectedMediaIdentifierInfo> list =
                new ArrayList<ProtectedMediaIdentifierInfo>();
        nativeGetProtectedMediaIdentifierOrigins(list);
        return list;
    }

    @CalledByNative
    private static void insertProtectedMediaIdentifierInfoIntoList(
            ArrayList<ProtectedMediaIdentifierInfo> list, String origin, String embedder) {
        list.add(new ProtectedMediaIdentifierInfo(origin, embedder));
    }

    @SuppressWarnings("unchecked")
    public static List<PushNotificationInfo> getPushNotificationInfo() {
        ArrayList<PushNotificationInfo> list = new ArrayList<PushNotificationInfo>();
        nativeGetPushNotificationOrigins(list);
        return list;
    }

    @CalledByNative
    private static void insertPushNotificationIntoList(
            ArrayList<PushNotificationInfo> list, String origin, String embedder) {
        list.add(new PushNotificationInfo(origin, embedder));
    }

    /**
     * @return List of all the origin/embedder combinations of voice and video capture
     *         permissions.
     */
    @SuppressWarnings("unchecked")
    public static List<VoiceAndVideoCaptureInfo> getVoiceAndVideoCaptureInfo() {
        ArrayList<VoiceAndVideoCaptureInfo> list =
                new ArrayList<VoiceAndVideoCaptureInfo>();
        // Camera and Microphone can be managed by the custodian of a supervised account or
        // by enterprise policy.
        boolean managedOnly = !PrefServiceBridge.getInstance().isCameraMicUserModifiable();
        nativeGetVoiceAndVideoCaptureOrigins(list, managedOnly);
        return list;
    }

    @CalledByNative
    private static void insertVoiceAndVideoCaptureInfoIntoList(
            ArrayList<VoiceAndVideoCaptureInfo> list, String origin, String embedder) {
        for (int i = 0; i < list.size(); i++) {
            if (list.get(i).getOrigin().equals(origin)
                        && list.get(i).getEmbedder().equals(embedder)) {
                return;
            }
        }
        list.add(new VoiceAndVideoCaptureInfo(origin, embedder));
    }

    public static List<JavaScriptExceptionInfo> getJavaScriptExceptionInfo() {
        List<JavaScriptExceptionInfo> exceptions =
                PrefServiceBridge.getInstance().getJavaScriptExceptions();
        if (!PrefServiceBridge.getInstance().javaScriptManaged()) {
            return exceptions;
        }

        List<JavaScriptExceptionInfo> managedExceptions = new ArrayList<JavaScriptExceptionInfo>();
        for (JavaScriptExceptionInfo exception : exceptions) {
            if (exception.getSource().equals("policy")) {
                managedExceptions.add(exception);
            }
        }
        return managedExceptions;
    }

    public static List<PopupExceptionInfo> getPopupExceptionInfo() {
        List<PopupExceptionInfo> exceptions =
                PrefServiceBridge.getInstance().getPopupExceptions();
        if (!PrefServiceBridge.getInstance().isPopupsManaged()) {
            return exceptions;
        }

        List<PopupExceptionInfo> managedExceptions = new ArrayList<PopupExceptionInfo>();
        for (PopupExceptionInfo exception : exceptions) {
            if (exception.getSource().equals("policy")) {
                managedExceptions.add(exception);
            }
        }
        return managedExceptions;
    }

    public static void fetchLocalStorageInfo(LocalStorageInfoReadyCallback callback) {
        nativeFetchLocalStorageInfo(callback);
    }

    public static void fetchStorageInfo(StorageInfoReadyCallback callback) {
        nativeFetchStorageInfo(callback);
    }

    private static native void nativeGetGeolocationOrigins(Object list, boolean managedOnly);
    static native int nativeGetGeolocationSettingForOrigin(String origin, String embedder);
    static native void nativeSetGeolocationSettingForOrigin(String origin, String embedder,
            int value);
    private static native void nativeGetMidiOrigins(Object list);
    static native int nativeGetMidiSettingForOrigin(String origin, String embedder);
    static native void nativeSetMidiSettingForOrigin(String origin, String embedder,
            int value);
    private static native void nativeGetPushNotificationOrigins(Object list);
    static native int nativeGetPushNotificationSettingForOrigin(String origin, String embedder);
    static native void nativeSetPushNotificationSettingForOrigin(String origin, String embedder,
            int value);
    private static native void nativeGetProtectedMediaIdentifierOrigins(Object list);
    static native int nativeGetProtectedMediaIdentifierSettingForOrigin(String origin,
            String embedder);
    static native void nativeSetProtectedMediaIdentifierSettingForOrigin(String origin,
            String embedder, int value);
    private static native void nativeGetVoiceAndVideoCaptureOrigins(
            Object list, boolean managedOnly);
    static native int nativeGetVoiceCaptureSettingForOrigin(String origin, String embedder);
    static native int nativeGetVideoCaptureSettingForOrigin(String origin, String embedder);
    static native void nativeSetVoiceCaptureSettingForOrigin(String origin, String embedder,
            int value);
    static native void nativeSetVideoCaptureSettingForOrigin(String origin, String embedder,
            int value);
    private static native void nativeGetCookieOrigins(Object list, boolean managedOnly);
    static native int nativeGetCookieSettingForOrigin(String origin, String embedder);
    static native void nativeSetCookieSettingForOrigin(String origin, String embedder, int setting);
    static native void nativeClearCookieData(String path);
    static native void nativeClearLocalStorageData(String path);
    static native void nativeClearStorageData(String origin, int type, Object callback);
    private static native void nativeFetchLocalStorageInfo(Object callback);
    private static native void nativeFetchStorageInfo(Object callback);
    static native boolean nativeIsContentSettingsPatternValid(String pattern);
}
