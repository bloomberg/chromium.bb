// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import org.chromium.chrome.browser.ContentSettingsType;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.Map;
import java.util.Set;

/**
 * Utility class that asynchronously fetches any Websites and the permissions
 * that the user has set for them.
 */
public class WebsitePermissionsFetcher {
    /**
     * A callback to pass to WebsitePermissionsFetcher. This is run when the
     * website permissions have been fetched.
     */
    public interface WebsitePermissionsCallback {
        void onWebsitePermissionsAvailable(
                Map<String, Set<Website>> sitesByOrigin, Map<String, Set<Website>> sitesByHost);
    }

    // This is a 1 <--> 1..N mapping between origin and Website.
    // TODO(mvanouwerkerk): The Website class has no equals or hashCode methods so storing them in
    // a HashSet is really confusing to readers of this code. There is no deduplication at all.
    private final Map<String, Set<Website>> mSitesByOrigin = new HashMap<>();

    // This is a 1 <--> 1..N mapping between host and Website.
    // TODO(mvanouwerkerk): The Website class has no equals or hashCode methods so storing them in
    // a HashSet is really confusing to readers of this code. There is no deduplication at all.
    private final Map<String, Set<Website>> mSitesByHost = new HashMap<>();

    // The callback to run when the permissions have been fetched.
    private final WebsitePermissionsCallback mCallback;

    /**
     * @param callback The callback to run when the fetch is complete.
     */
    public WebsitePermissionsFetcher(WebsitePermissionsCallback callback) {
        mCallback = callback;
    }

    /**
     * Fetches preferences for all sites that have them.
     * TODO(mvanouwerkerk): Add an argument |url| to only fetch permissions for
     * sites from the same origin as that of |url| - https://crbug.com/459222.
     */
    public void fetchAllPreferences() {
        TaskQueue queue = new TaskQueue();
        // Populate features from more specific to less specific.
        // Geolocation lookup permission is per-origin and per-embedder.
        queue.add(new GeolocationInfoFetcher());
        // Midi sysex access permission is per-origin and per-embedder.
        queue.add(new MidiInfoFetcher());
        // Cookies are stored per-host.
        queue.add(new CookieExceptionInfoFetcher());
        // Fullscreen are stored per-origin.
        queue.add(new FullscreenInfoFetcher());
        // Keygen permissions are per-origin.
        queue.add(new KeygenInfoFetcher());
        // Local storage info is per-origin.
        queue.add(new LocalStorageInfoFetcher());
        // Website storage is per-host.
        queue.add(new WebStorageInfoFetcher());
        // Popup exceptions are host-based patterns (unless we start
        // synchronizing popup exceptions with desktop Chrome).
        queue.add(new PopupExceptionInfoFetcher());
        // JavaScript exceptions are host-based patterns.
        queue.add(new JavaScriptExceptionInfoFetcher());
        // Protected media identifier permission is per-origin and per-embedder.
        queue.add(new ProtectedMediaIdentifierInfoFetcher());
        // Notification permission is per-origin.
        queue.add(new NotificationInfoFetcher());
        // Camera capture permission is per-origin and per-embedder.
        queue.add(new CameraCaptureInfoFetcher());
        // Micropohone capture permission is per-origin and per-embedder.
        queue.add(new MicrophoneCaptureInfoFetcher());
        // Background sync permission is per-origin.
        queue.add(new BackgroundSyncExceptionInfoFetcher());
        // Autoplay permission is per-origin.
        queue.add(new AutoplayInfoFetcher());

        queue.add(new PermissionsAvailableCallbackRunner());

        queue.next();
    }

    /**
     * Fetches all preferences within a specific category.
     *
     * @param catgory A category to fetch.
     */
    public void fetchPreferencesForCategory(SiteSettingsCategory category) {
        if (category.showAllSites()) {
            fetchAllPreferences();
            return;
        }

        TaskQueue queue = new TaskQueue();
        // Populate features from more specific to less specific.
        if (category.showGeolocationSites()) {
            // Geolocation lookup permission is per-origin and per-embedder.
            queue.add(new GeolocationInfoFetcher());
        } else if (category.showCookiesSites()) {
            // Cookies exceptions are patterns.
            queue.add(new CookieExceptionInfoFetcher());
        } else if (category.showStorageSites()) {
            // Local storage info is per-origin.
            queue.add(new LocalStorageInfoFetcher());
            // Website storage is per-host.
            queue.add(new WebStorageInfoFetcher());
        } else if (category.showFullscreenSites()) {
            // Full screen is per-origin.
            queue.add(new FullscreenInfoFetcher());
        } else if (category.showCameraSites()) {
            // Camera capture permission is per-origin and per-embedder.
            queue.add(new CameraCaptureInfoFetcher());
        } else if (category.showMicrophoneSites()) {
            // Micropohone capture permission is per-origin and per-embedder.
            queue.add(new MicrophoneCaptureInfoFetcher());
        } else if (category.showPopupSites()) {
            // Popup exceptions are host-based patterns (unless we start
            // synchronizing popup exceptions with desktop Chrome.)
            queue.add(new PopupExceptionInfoFetcher());
        } else if (category.showJavaScriptSites()) {
            // JavaScript exceptions are host-based patterns.
            queue.add(new JavaScriptExceptionInfoFetcher());
        } else if (category.showNotificationsSites()) {
            // Push notification permission is per-origin.
            queue.add(new NotificationInfoFetcher());
        } else if (category.showBackgroundSyncSites()) {
            // Background sync info is per-origin.
            queue.add(new BackgroundSyncExceptionInfoFetcher());
        } else if (category.showProtectedMediaSites()) {
            // Protected media identifier permission is per-origin and per-embedder.
            queue.add(new ProtectedMediaIdentifierInfoFetcher());
        } else if (category.showAutoplaySites()) {
            // Autoplay permission is per-origin.
            queue.add(new AutoplayInfoFetcher());
        }
        queue.add(new PermissionsAvailableCallbackRunner());
        queue.next();
    }

    private Website createSiteByOriginAndHost(WebsiteAddress address) {
        String origin = address.getOrigin();
        String host = address.getHost();
        Website site = new Website(address);
        if (!mSitesByOrigin.containsKey(origin)) mSitesByOrigin.put(origin, new HashSet<Website>());
        mSitesByOrigin.get(origin).add(site);
        if (!mSitesByHost.containsKey(host)) mSitesByHost.put(host, new HashSet<Website>());
        mSitesByHost.get(host).add(site);
        return site;
    }

    private Set<Website> findOrCreateSitesByOrigin(WebsiteAddress address) {
        String origin = address.getOrigin();
        if (!mSitesByOrigin.containsKey(origin)) createSiteByOriginAndHost(address);
        return mSitesByOrigin.get(origin);
    }

    private Set<Website> findOrCreateSitesByHost(WebsiteAddress address) {
        String host = address.getHost();
        if (!mSitesByHost.containsKey(host)) {
            mSitesByHost.put(host, new HashSet<Website>());
            mSitesByHost.get(host).add(new Website(address));
        }
        return mSitesByHost.get(host);
    }

    /**
     * A single task in the WebsitePermissionsFetcher task queue. We need fetching of features to be
     * serialized, as we need to have all the origins in place prior to populating the hosts.
     */
    private abstract class Task {
        /** Override this method to implement a synchronous task. */
        void run() {}

        /**
         * Override this method to implement an asynchronous task. Call queue.next() once execution
         * is complete.
         */
        void runAsync(TaskQueue queue) {
            run();
            queue.next();
        }
    }

    /**
     * A queue used to store the sequence of tasks to run to fetch the website preferences. Each
     * task is run sequentially, and some of the tasks may run asynchronously.
     */
    private static class TaskQueue extends LinkedList<Task> {
        void next() {
            if (!isEmpty()) removeFirst().runAsync(this);
        }
    }

    private class AutoplayInfoFetcher extends Task {
        @Override
        public void run() {
            for (ContentSettingException exception
                    : WebsitePreferenceBridge.getContentSettingsExceptions(
                            ContentSettingsType.CONTENT_SETTINGS_TYPE_AUTOPLAY)) {
                // The pattern "*" represents the default setting, not a specific website.
                if (exception.getPattern().equals("*")) continue;
                WebsiteAddress address = WebsiteAddress.create(exception.getPattern());
                if (address == null) continue;
                Set<Website> sites = findOrCreateSitesByHost(address);
                for (Website site : sites) {
                    site.setAutoplayException(exception);
                }
            }
        }
    }

    private class GeolocationInfoFetcher extends Task {
        @Override
        public void run() {
            for (GeolocationInfo info : WebsitePreferenceBridge.getGeolocationInfo()) {
                WebsiteAddress address = WebsiteAddress.create(info.getOrigin());
                if (address == null) continue;
                createSiteByOriginAndHost(address).setGeolocationInfo(info);
            }
        }
    }

    private class MidiInfoFetcher extends Task {
        @Override
        public void run() {
            for (MidiInfo info : WebsitePreferenceBridge.getMidiInfo()) {
                WebsiteAddress address = WebsiteAddress.create(info.getOrigin());
                if (address == null) continue;
                createSiteByOriginAndHost(address).setMidiInfo(info);
            }
        }
    }

    private class PopupExceptionInfoFetcher extends Task {
        @Override
        public void run() {
            for (ContentSettingException exception :
                    WebsitePreferenceBridge.getContentSettingsExceptions(
                            ContentSettingsType.CONTENT_SETTINGS_TYPE_POPUPS)) {
                // The pattern "*" represents the default setting, not a
                // specific website.
                if (exception.getPattern().equals("*")) continue;
                WebsiteAddress address = WebsiteAddress.create(exception.getPattern());
                if (address == null) continue;
                Set<Website> sites = findOrCreateSitesByHost(address);
                for (Website site : sites) {
                    site.setPopupException(exception);
                }
            }
        }
    }

    private class JavaScriptExceptionInfoFetcher extends Task {
        @Override
        public void run() {
            for (ContentSettingException exception
                    : WebsitePreferenceBridge.getContentSettingsExceptions(
                            ContentSettingsType.CONTENT_SETTINGS_TYPE_JAVASCRIPT)) {
                // The pattern "*" represents the default setting, not a specific website.
                if (exception.getPattern().equals("*")) continue;
                WebsiteAddress address = WebsiteAddress.create(exception.getPattern());
                if (address == null) continue;
                Set<Website> sites = findOrCreateSitesByHost(address);
                for (Website site : sites) {
                    site.setJavaScriptException(exception);
                }
            }
        }
    }

    private class CookieExceptionInfoFetcher extends Task {
        @Override
        public void run() {
            for (ContentSettingException exception :
                    WebsitePreferenceBridge.getContentSettingsExceptions(
                            ContentSettingsType.CONTENT_SETTINGS_TYPE_COOKIES)) {
                // The pattern "*" represents the default setting, not a specific website.
                if (exception.getPattern().equals("*")) continue;
                WebsiteAddress address = WebsiteAddress.create(exception.getPattern());
                if (address == null) continue;
                Set<Website> sites = findOrCreateSitesByHost(address);
                for (Website site : sites) {
                    site.setCookieException(exception);
                }
            }
        }
    }

    private class KeygenInfoFetcher extends Task {
        @Override
        public void run() {
            for (KeygenInfo info : WebsitePreferenceBridge.getKeygenInfo()) {
                WebsiteAddress address = WebsiteAddress.create(info.getOrigin());
                if (address == null) continue;
                createSiteByOriginAndHost(address).setKeygenInfo(info);
            }
        }
    }

    /**
     * Class for fetching the fullscreen information.
     */
    private class FullscreenInfoFetcher extends Task {
        @Override
        public void run() {
            for (FullscreenInfo info : WebsitePreferenceBridge.getFullscreenInfo()) {
                WebsiteAddress address = WebsiteAddress.create(info.getOrigin());
                if (address == null) continue;
                createSiteByOriginAndHost(address).setFullscreenInfo(info);
            }
        }
    }

    private class LocalStorageInfoFetcher extends Task {
        @Override
        public void runAsync(final TaskQueue queue) {
            WebsitePreferenceBridge.fetchLocalStorageInfo(
                    new WebsitePreferenceBridge.LocalStorageInfoReadyCallback() {
                        @SuppressWarnings("unchecked")
                        @Override
                        public void onLocalStorageInfoReady(HashMap map) {
                            for (Object o : map.entrySet()) {
                                Map.Entry<String, LocalStorageInfo> entry =
                                        (Map.Entry<String, LocalStorageInfo>) o;
                                WebsiteAddress address = WebsiteAddress.create(entry.getKey());
                                if (address == null) continue;
                                Set<Website> sites = findOrCreateSitesByOrigin(address);
                                for (Website site : sites) {
                                    site.setLocalStorageInfo(entry.getValue());
                                }
                            }
                            queue.next();
                        }
                    });
        }
    }

    private class WebStorageInfoFetcher extends Task {
        @Override
        public void runAsync(final TaskQueue queue) {
            WebsitePreferenceBridge.fetchStorageInfo(
                    new WebsitePreferenceBridge.StorageInfoReadyCallback() {
                        @SuppressWarnings("unchecked")
                        @Override
                        public void onStorageInfoReady(ArrayList array) {
                            ArrayList<StorageInfo> infoArray = array;
                            for (StorageInfo info : infoArray) {
                                WebsiteAddress address = WebsiteAddress.create(info.getHost());
                                if (address == null) continue;
                                Set<Website> sites = findOrCreateSitesByHost(address);
                                for (Website site : sites) {
                                    site.addStorageInfo(info);
                                }
                            }
                            queue.next();
                        }
                    });
        }
    }

    private class ProtectedMediaIdentifierInfoFetcher extends Task {
        @Override
        public void run() {
            for (ProtectedMediaIdentifierInfo info :
                    WebsitePreferenceBridge.getProtectedMediaIdentifierInfo()) {
                WebsiteAddress address = WebsiteAddress.create(info.getOrigin());
                if (address == null) continue;
                createSiteByOriginAndHost(address).setProtectedMediaIdentifierInfo(info);
            }
        }
    }

    private class NotificationInfoFetcher extends Task {
        @Override
        public void run() {
            for (NotificationInfo info : WebsitePreferenceBridge.getNotificationInfo()) {
                WebsiteAddress address = WebsiteAddress.create(info.getOrigin());
                if (address == null) continue;
                createSiteByOriginAndHost(address).setNotificationInfo(info);
            }
        }
    }

    private class CameraCaptureInfoFetcher extends Task {
        @Override
        public void run() {
            for (CameraInfo info : WebsitePreferenceBridge.getCameraInfo()) {
                WebsiteAddress address = WebsiteAddress.create(info.getOrigin());
                if (address == null) continue;
                createSiteByOriginAndHost(address).setCameraInfo(info);
            }
        }
    }

    private class MicrophoneCaptureInfoFetcher extends Task {
        @Override
        public void run() {
            for (MicrophoneInfo info : WebsitePreferenceBridge.getMicrophoneInfo()) {
                WebsiteAddress address = WebsiteAddress.create(info.getOrigin());
                if (address == null) continue;
                createSiteByOriginAndHost(address).setMicrophoneInfo(info);
            }
        }
    }

    private class BackgroundSyncExceptionInfoFetcher extends Task {
        @Override
        public void run() {
            for (ContentSettingException exception :
                    WebsitePreferenceBridge.getContentSettingsExceptions(
                            ContentSettingsType.CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC)) {
                // The pattern "*" represents the default setting, not a specific website.
                if (exception.getPattern().equals("*")) continue;
                WebsiteAddress address = WebsiteAddress.create(exception.getPattern());
                if (address == null) continue;
                Set<Website> sites = findOrCreateSitesByHost(address);
                for (Website site : sites) {
                    site.setBackgroundSyncException(exception);
                }
            }
        }
    }

    private class PermissionsAvailableCallbackRunner extends Task {
        @Override
        public void run() {
            mCallback.onWebsitePermissionsAvailable(mSitesByOrigin, mSitesByHost);
        }
    }
}
