// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.AsyncTask;
import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.browsing_data.UrlFilters;
import org.chromium.testing.local.BackgroundShadowAsyncTask;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * Tests the WebappRegistry class by ensuring that it persists data to
 * SharedPreferences as expected.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {BackgroundShadowAsyncTask.class})
public class WebappRegistryTest {

    // These were copied from WebappRegistry for backward compatibility checking.
    private static final String REGISTRY_FILE_NAME = "webapp_registry";
    private static final String KEY_WEBAPP_SET = "webapp_set";
    private static final String KEY_LAST_CLEANUP = "last_cleanup";

    private static final int INITIAL_TIME = 0;

    private SharedPreferences mSharedPreferences;
    private boolean mCallbackCalled;

    private static class FetchCallback implements WebappRegistry.FetchCallback {
        boolean mCallbackCalled;

        Set<String> mExpected;

        FetchCallback(Set<String> expected) {
            mCallbackCalled = false;
            mExpected = expected;
        }

        @Override
        public void onWebappIdsRetrieved(Set<String> actual) {
            mCallbackCalled = true;
            assertEquals(mExpected, actual);
        }

        boolean getCallbackCalled() {
            return mCallbackCalled;
        }
    }

    private static class FetchStorageCallback
            implements WebappRegistry.FetchWebappDataStorageCallback {
        Intent mShortcutIntent;
        boolean mCallbackCalled;

        FetchStorageCallback(Intent shortcutIntent) {
            mCallbackCalled = false;
            mShortcutIntent = shortcutIntent;
        }

        @Override
        public void onWebappDataStorageRetrieved(WebappDataStorage storage) {
            mCallbackCalled = true;
            storage.updateFromShortcutIntent(mShortcutIntent);
            storage.updateLastUsedTime();
        }

        boolean getCallbackCalled() {
            return mCallbackCalled;
        }
    }

    private static class FetchStorageByUrlCallback
            implements WebappRegistry.FetchWebappDataStorageCallback {
        String mUrl;
        String mScope;
        boolean mCallbackCalled;

        FetchStorageByUrlCallback(String url, String scope) {
            mCallbackCalled = false;
            mUrl = url;
            mScope = scope;
        }

        @Override
        public void onWebappDataStorageRetrieved(WebappDataStorage storage) {
            mCallbackCalled = true;
            assertEquals(mUrl, storage.getUrl());
            assertEquals(mScope, storage.getScope());
        }

        boolean getCallbackCalled() {
            return mCallbackCalled;
        }
    }

    private static class CallbackRunner implements Runnable {
        boolean mCallbackCalled;

        public CallbackRunner() {
            mCallbackCalled = false;
        }

        @Override
        public void run() {
            mCallbackCalled = true;
        }

        boolean getCallbackCalled() {
            return mCallbackCalled;
        }
    }

    @Before
    public void setUp() throws Exception {
        ContextUtils.initApplicationContextForTests(RuntimeEnvironment.application);
        mSharedPreferences = ContextUtils.getApplicationContext().getSharedPreferences(
                REGISTRY_FILE_NAME, Context.MODE_PRIVATE);
        mSharedPreferences.edit().putLong(KEY_LAST_CLEANUP, INITIAL_TIME).commit();

        mCallbackCalled = false;
    }

    @After
    public void tearDown() {
        mSharedPreferences.edit().clear().apply();
    }

    @Test
    @Feature({"Webapp"})
    public void testBackwardCompatibility() {
        assertEquals(REGISTRY_FILE_NAME, WebappRegistry.REGISTRY_FILE_NAME);
        assertEquals(KEY_WEBAPP_SET, WebappRegistry.KEY_WEBAPP_SET);
        assertEquals(KEY_LAST_CLEANUP, WebappRegistry.KEY_LAST_CLEANUP);
    }

    @Test
    @Feature({"Webapp"})
    public void testWebappRegistrationAddsToSharedPrefs() throws Exception {
        WebappRegistry.registerWebapp("test", null);

        BackgroundShadowAsyncTask.runBackgroundTasks();

        Set<String> actual = mSharedPreferences.getStringSet(
                WebappRegistry.KEY_WEBAPP_SET, Collections.<String>emptySet());
        assertEquals(1, actual.size());
        assertTrue(actual.contains("test"));
    }

    @Test
    @Feature({"Webapp"})
    public void testWebappRegistrationUpdatesLastUsed() throws Exception {
        WebappRegistry.registerWebapp("test", null);

        BackgroundShadowAsyncTask.runBackgroundTasks();
        long after = System.currentTimeMillis();

        SharedPreferences webAppPrefs = ContextUtils.getApplicationContext().getSharedPreferences(
                WebappDataStorage.SHARED_PREFS_FILE_PREFIX + "test", Context.MODE_PRIVATE);
        long actual = webAppPrefs.getLong(WebappDataStorage.KEY_LAST_USED,
                WebappDataStorage.LAST_USED_INVALID);
        assertTrue("Timestamp is out of range", actual <= after);
    }

    @Test
    @Feature({"Webapp"})
    public void testWebappIdsRetrieval() throws Exception {
        final Set<String> expected = addWebappsToRegistry("first", "second");

        FetchCallback callback = new FetchCallback(expected);
        WebappRegistry.getRegisteredWebappIds(callback);

        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();

        assertTrue(callback.getCallbackCalled());
    }

    @Test
    @Feature({"Webapp"})
    public void testWebappIdsRetrievalRegisterRetrival() throws Exception {
        final Set<String> expected = addWebappsToRegistry("first");

        FetchCallback callback = new FetchCallback(expected);
        WebappRegistry.getRegisteredWebappIds(callback);

        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();

        assertTrue(callback.getCallbackCalled());

        WebappRegistry.registerWebapp("second", null);

        BackgroundShadowAsyncTask.runBackgroundTasks();

        // A copy of the expected set needs to be made as the SharedPreferences is using the copy
        // that was paassed to it.
        final Set<String> secondExpected = new HashSet<>(expected);
        secondExpected.add("second");

        callback = new FetchCallback(secondExpected);
        WebappRegistry.getRegisteredWebappIds(callback);

        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();

        assertTrue(callback.getCallbackCalled());
    }

    @Test
    @Feature({"Webapp"})
    public void testUnregisterRunsCallback() throws Exception {
        CallbackRunner callback = new CallbackRunner();
        WebappRegistry.unregisterWebappsForUrls(new UrlFilters.AllUrls(), callback);
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();

        assertTrue(callback.getCallbackCalled());
    }

    @Test
    @Feature({"Webapp"})
    public void testUnregisterClearsRegistry() throws Exception {
        Map<String, String> apps = new HashMap<>();
        apps.put("webapp1", "http://example.com/index.html");
        apps.put("webapp2", "https://www.google.com/foo/bar");
        apps.put("webapp3", "https://www.chrome.com");

        for (Map.Entry<String, String> app : apps.entrySet()) {
            WebappRegistry.registerWebapp(
                    app.getKey(), new FetchStorageCallback(createShortcutIntent(app.getValue())));
        }
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();

        // Partial deletion.
        CallbackRunner callback = new CallbackRunner();
        WebappRegistry.unregisterWebappsForUrls(
                new UrlFilters.OneUrl("http://example.com/index.html"), callback);
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();

        assertTrue(callback.getCallbackCalled());
        Set<String> registeredWebapps = getRegisteredWebapps();
        assertEquals(2, registeredWebapps.size());
        for (String appName : apps.keySet()) {
            assertEquals(!TextUtils.equals(appName, "webapp1"),
                         registeredWebapps.contains(appName));
        }

        // Full deletion.
        callback = new CallbackRunner();
        WebappRegistry.unregisterWebappsForUrls(new UrlFilters.AllUrls(), callback);
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();

        assertTrue(callback.getCallbackCalled());
        assertTrue(getRegisteredWebapps().isEmpty());
    }

    @Test
    @Feature({"Webapp"})
    public void testUnregisterClearsWebappDataStorage() throws Exception {
        Map<String, String> apps = new HashMap<>();
        apps.put("webapp1", "http://example.com/index.html");
        apps.put("webapp2", "https://www.google.com/foo/bar");
        apps.put("webapp3", "https://www.chrome.com");

        for (Map.Entry<String, String> app : apps.entrySet()) {
            WebappRegistry.registerWebapp(
                    app.getKey(), new FetchStorageCallback(createShortcutIntent(app.getValue())));
        }
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();

        for (String appName : apps.keySet()) {
            SharedPreferences webAppPrefs =
                    ContextUtils.getApplicationContext().getSharedPreferences(
                            WebappDataStorage.SHARED_PREFS_FILE_PREFIX + appName,
                            Context.MODE_PRIVATE);
            webAppPrefs.edit().putLong(WebappDataStorage.KEY_LAST_USED, 100L).apply();
        }

        // Partial deletion.
        WebappRegistry.unregisterWebappsForUrls(
                new UrlFilters.OneUrl("http://example.com/index.html"), null);

        BackgroundShadowAsyncTask.runBackgroundTasks();
        for (String appName : apps.keySet()) {
            SharedPreferences webAppPrefs =
                    ContextUtils.getApplicationContext().getSharedPreferences(
                            WebappDataStorage.SHARED_PREFS_FILE_PREFIX + appName,
                            Context.MODE_PRIVATE);
            assertEquals(TextUtils.equals(appName, "webapp1"), webAppPrefs.getAll().isEmpty());
        }

        // Full deletion.
        WebappRegistry.unregisterWebappsForUrls(new UrlFilters.AllUrls(), null);
        BackgroundShadowAsyncTask.runBackgroundTasks();
        for (String appName : apps.keySet()) {
            SharedPreferences webAppPrefs =
                    ContextUtils.getApplicationContext().getSharedPreferences(
                            WebappDataStorage.SHARED_PREFS_FILE_PREFIX + appName,
                            Context.MODE_PRIVATE);
            assertTrue(webAppPrefs.getAll().isEmpty());
        }
    }

    @Test
    @Feature({"Webapp"})
      public void testCleanupDoesNotRunTooOften() throws Exception {
        // Put the current time to just before the task should run.
        long currentTime = INITIAL_TIME + WebappRegistry.FULL_CLEANUP_DURATION - 1;

        addWebappsToRegistry("oldWebapp");
        SharedPreferences webAppPrefs = ContextUtils.getApplicationContext().getSharedPreferences(
                WebappDataStorage.SHARED_PREFS_FILE_PREFIX + "oldWebapp", Context.MODE_PRIVATE);
        webAppPrefs.edit().putLong(WebappDataStorage.KEY_LAST_USED, Long.MIN_VALUE).apply();

        WebappRegistry.unregisterOldWebapps(currentTime);
        BackgroundShadowAsyncTask.runBackgroundTasks();

        Set<String> actual = mSharedPreferences.getStringSet(
                WebappRegistry.KEY_WEBAPP_SET, Collections.<String>emptySet());
        assertEquals(new HashSet<>(Arrays.asList("oldWebapp")), actual);

        long actualLastUsed = webAppPrefs.getLong(WebappDataStorage.KEY_LAST_USED,
                WebappDataStorage.LAST_USED_INVALID);
        assertEquals(Long.MIN_VALUE, actualLastUsed);

        // The last cleanup time was set to 0 in setUp() so check that this hasn't changed.
        long lastCleanup = mSharedPreferences.getLong(WebappRegistry.KEY_LAST_CLEANUP, -1);
        assertEquals(INITIAL_TIME, lastCleanup);
    }

    @Test
    @Feature({"Webapp"})
    public void testCleanupDoesNotRemoveRecentApps() throws Exception {
        // Put the current time such that the task runs.
        long currentTime = INITIAL_TIME + WebappRegistry.FULL_CLEANUP_DURATION;

        // Put the last used time just inside the no-cleanup window.
        addWebappsToRegistry("recentWebapp");
        SharedPreferences webAppPrefs = ContextUtils.getApplicationContext().getSharedPreferences(
                WebappDataStorage.SHARED_PREFS_FILE_PREFIX + "recentWebapp", Context.MODE_PRIVATE);
        long lastUsed = currentTime - WebappRegistry.WEBAPP_UNOPENED_CLEANUP_DURATION + 1;
        webAppPrefs.edit().putLong(WebappDataStorage.KEY_LAST_USED, lastUsed).apply();

        // Because the time is just inside the window, there should be a cleanup but the web app
        // should not be deleted as it was used recently. The last cleanup time should also be
        // set to the current time.
        WebappRegistry.unregisterOldWebapps(currentTime);
        BackgroundShadowAsyncTask.runBackgroundTasks();

        Set<String> actual = mSharedPreferences.getStringSet(
                WebappRegistry.KEY_WEBAPP_SET, Collections.<String>emptySet());
        assertEquals(new HashSet<>(Arrays.asList("recentWebapp")), actual);

        long actualLastUsed = webAppPrefs.getLong(WebappDataStorage.KEY_LAST_USED,
                WebappDataStorage.LAST_USED_INVALID);
        assertEquals(lastUsed, actualLastUsed);

        long lastCleanup = mSharedPreferences.getLong(WebappRegistry.KEY_LAST_CLEANUP, -1);
        assertEquals(currentTime, lastCleanup);
    }

    @Test
    @Feature({"Webapp"})
    public void testCleanupRemovesOldApps() throws Exception {
        // Put the current time such that the task runs.
        long currentTime = INITIAL_TIME + WebappRegistry.FULL_CLEANUP_DURATION;

        // Put the last used time just outside the no-cleanup window.
        addWebappsToRegistry("oldWebapp");
        SharedPreferences webAppPrefs = ContextUtils.getApplicationContext().getSharedPreferences(
                WebappDataStorage.SHARED_PREFS_FILE_PREFIX + "oldWebapp", Context.MODE_PRIVATE);
        long lastUsed = currentTime - WebappRegistry.WEBAPP_UNOPENED_CLEANUP_DURATION;
        webAppPrefs.edit().putLong(WebappDataStorage.KEY_LAST_USED, lastUsed).apply();

        // Because the time is just inside the window, there should be a cleanup of old web apps and
        // the last cleaned up time should be set to the current time.
        WebappRegistry.unregisterOldWebapps(currentTime);
        BackgroundShadowAsyncTask.runBackgroundTasks();

        Set<String> actual = mSharedPreferences.getStringSet(
                WebappRegistry.KEY_WEBAPP_SET, Collections.<String>emptySet());
        assertTrue(actual.isEmpty());

        long actualLastUsed = webAppPrefs.getLong(WebappDataStorage.KEY_LAST_USED,
                WebappDataStorage.LAST_USED_INVALID);
        assertEquals(WebappDataStorage.LAST_USED_INVALID, actualLastUsed);

        long lastCleanup = mSharedPreferences.getLong(WebappRegistry.KEY_LAST_CLEANUP, -1);
        assertEquals(currentTime, lastCleanup);
    }

    @Test
    @Feature({"WebApk"})
    public void testCleanupRemovesUninstalledWebApks() throws Exception {
        String webappId1 = "webapk:uninstalledWebApk1";
        String webApkPackage1 = "uninstalledWebApk1";
        String webappId2 = "webapk:uninstalledWebApk2";
        String webApkPackage2 = "uninstalledWebApk2";

        FetchStorageCallback storageCallback1 = new FetchStorageCallback(
                createWebApkIntent(webappId1, webApkPackage1));
        WebappRegistry.registerWebapp(webappId1, storageCallback1);
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();
        assertTrue(storageCallback1.getCallbackCalled());

        FetchStorageCallback storageCallback2 = new FetchStorageCallback(
                createWebApkIntent(webappId1, webApkPackage2));
        WebappRegistry.registerWebapp(webappId2, storageCallback2);
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();
        assertTrue(storageCallback2.getCallbackCalled());

        // Verify that both WebAPKs are registered.
        Set<String> actual = mSharedPreferences.getStringSet(
                WebappRegistry.KEY_WEBAPP_SET, Collections.<String>emptySet());
        assertEquals(2, actual.size());
        assertTrue(actual.contains(webappId1));
        assertTrue(actual.contains(webappId2));

        // Set the current time such that the task runs.
        long currentTime = System.currentTimeMillis() + WebappRegistry.FULL_CLEANUP_DURATION;
        // Because the time is just inside the window, there should be a cleanup of
        // uninstalled WebAPKs and the last cleaned up time should be set to the
        // current time.
        WebappRegistry.unregisterOldWebapps(currentTime);
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();

        actual = mSharedPreferences.getStringSet(
                WebappRegistry.KEY_WEBAPP_SET, Collections.<String>emptySet());
        assertTrue(actual.isEmpty());

        long lastCleanup = mSharedPreferences.getLong(
                WebappRegistry.KEY_LAST_CLEANUP, -1);
        assertEquals(currentTime, lastCleanup);
    }

    @Test
    @Feature({"WebApk"})
    public void testCleanupDoesNotRemoveInstalledWebApks() throws Exception {
        String webappId = "webapk:installedWebApk";
        String webApkPackage = "installedWebApk";
        String uninstalledWebappId = "webapk:uninstalledWebApk";
        String uninstalledWebApkPackage = "uninstalledWebApk";

        FetchStorageCallback storageCallback = new FetchStorageCallback(
                createWebApkIntent(webappId, webApkPackage));
        WebappRegistry.registerWebapp(webappId, storageCallback);
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();
        assertTrue(storageCallback.getCallbackCalled());

        FetchStorageCallback storageCallback2 = new FetchStorageCallback(
                createWebApkIntent(uninstalledWebappId, uninstalledWebApkPackage));
        WebappRegistry.registerWebapp(uninstalledWebappId, storageCallback2);
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();
        assertTrue(storageCallback2.getCallbackCalled());

        // Verify that both WebAPKs are registered.
        Set<String> actual = mSharedPreferences.getStringSet(
                WebappRegistry.KEY_WEBAPP_SET, Collections.<String>emptySet());
        assertEquals(2, actual.size());
        assertTrue(actual.contains(webappId));
        assertTrue(actual.contains(uninstalledWebappId));

        RuntimeEnvironment.getRobolectricPackageManager().addPackage(webApkPackage);

        // Set the current time such that the task runs.
        long currentTime = System.currentTimeMillis() + WebappRegistry.FULL_CLEANUP_DURATION;
        // Because the time is just inside the window, there should be a cleanup of
        // uninstalled WebAPKs and the last cleaned up time should be set to the
        // current time.
        WebappRegistry.unregisterOldWebapps(currentTime);
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();

        actual = mSharedPreferences.getStringSet(
                WebappRegistry.KEY_WEBAPP_SET, Collections.<String>emptySet());
        assertEquals(1, actual.size());
        assertTrue(actual.contains(webappId));

        long lastCleanup = mSharedPreferences.getLong(
                WebappRegistry.KEY_LAST_CLEANUP, -1);
        assertEquals(currentTime, lastCleanup);
    }

    @Test
    @Feature({"Webapp"})
    public void testClearWebappHistoryRunsCallback() throws Exception {
        CallbackRunner callback = new CallbackRunner();
        WebappRegistry.clearWebappHistoryForUrls(new UrlFilters.AllUrls(), callback);
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();

        assertTrue(callback.getCallbackCalled());
    }

    @Test
    @Feature({"Webapp"})
    public void testClearWebappHistory() throws Exception {
        final String webapp1Url = "https://www.google.com";
        final String webapp2Url = "https://drive.google.com";
        Intent shortcutIntent1 = createShortcutIntent(webapp1Url);
        Intent shortcutIntent2 = createShortcutIntent(webapp2Url);

        FetchStorageCallback storageCallback1 = new FetchStorageCallback(shortcutIntent1);
        WebappRegistry.registerWebapp("webapp1", storageCallback1);
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();
        assertTrue(storageCallback1.getCallbackCalled());

        FetchStorageCallback storageCallback2 = new FetchStorageCallback(shortcutIntent2);
        WebappRegistry.registerWebapp("webapp2", storageCallback2);
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();
        assertTrue(storageCallback2.getCallbackCalled());

        SharedPreferences webapp1Prefs = ContextUtils.getApplicationContext().getSharedPreferences(
                WebappDataStorage.SHARED_PREFS_FILE_PREFIX + "webapp1", Context.MODE_PRIVATE);
        SharedPreferences webapp2Prefs = ContextUtils.getApplicationContext().getSharedPreferences(
                WebappDataStorage.SHARED_PREFS_FILE_PREFIX + "webapp2", Context.MODE_PRIVATE);

        long webapp1OriginalLastUsed = webapp2Prefs.getLong(
                WebappDataStorage.KEY_LAST_USED, WebappDataStorage.LAST_USED_UNSET);
        long webapp2OriginalLastUsed = webapp2Prefs.getLong(
                WebappDataStorage.KEY_LAST_USED, WebappDataStorage.LAST_USED_UNSET);
        assertTrue(webapp1OriginalLastUsed != WebappDataStorage.LAST_USED_UNSET);
        assertTrue(webapp2OriginalLastUsed != WebappDataStorage.LAST_USED_UNSET);

        // Clear data for |webapp1Url|.
        CallbackRunner callback = new CallbackRunner();
        WebappRegistry.clearWebappHistoryForUrls(new UrlFilters.OneUrl(webapp1Url), callback);
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();
        assertTrue(callback.getCallbackCalled());

        Set<String> actual = mSharedPreferences.getStringSet(
                WebappRegistry.KEY_WEBAPP_SET, Collections.<String>emptySet());
        assertEquals(2, actual.size());
        assertTrue(actual.contains("webapp1"));
        assertTrue(actual.contains("webapp2"));

        // Verify that the last used time for the first web app is
        // WebappDataStorage.LAST_USED_UNSET, while for the second one it's unchanged.
        long actualLastUsed = webapp1Prefs.getLong(
                WebappDataStorage.KEY_LAST_USED, WebappDataStorage.LAST_USED_UNSET);
        assertEquals(WebappDataStorage.LAST_USED_UNSET, actualLastUsed);
        actualLastUsed = webapp2Prefs.getLong(
                WebappDataStorage.KEY_LAST_USED, WebappDataStorage.LAST_USED_UNSET);
        assertEquals(webapp2OriginalLastUsed, actualLastUsed);

        // Verify that the URL and scope for the first web app is WebappDataStorage.URL_INVALID,
        // while for the second one it's unchanged.
        String actualScope = webapp1Prefs.getString(
                WebappDataStorage.KEY_SCOPE, WebappDataStorage.URL_INVALID);
        assertEquals(WebappDataStorage.URL_INVALID, actualScope);
        String actualUrl = webapp1Prefs.getString(
                WebappDataStorage.KEY_URL, WebappDataStorage.URL_INVALID);
        assertEquals(WebappDataStorage.URL_INVALID, actualUrl);
        actualScope = webapp2Prefs.getString(
                WebappDataStorage.KEY_SCOPE, WebappDataStorage.URL_INVALID);
        assertEquals(webapp2Url + "/", actualScope);
        actualUrl = webapp2Prefs.getString(
                WebappDataStorage.KEY_URL, WebappDataStorage.URL_INVALID);
        assertEquals(webapp2Url, actualUrl);

        // Clear data for all urls.
        callback = new CallbackRunner();
        WebappRegistry.clearWebappHistoryForUrls(new UrlFilters.AllUrls(), callback);
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();
        assertTrue(callback.getCallbackCalled());

        // Verify that the last used time for both web apps is WebappDataStorage.LAST_USED_UNSET.
        actualLastUsed = webapp1Prefs.getLong(
                WebappDataStorage.KEY_LAST_USED, WebappDataStorage.LAST_USED_UNSET);
        assertEquals(WebappDataStorage.LAST_USED_UNSET, actualLastUsed);
        actualLastUsed = webapp2Prefs.getLong(
                WebappDataStorage.KEY_LAST_USED, WebappDataStorage.LAST_USED_UNSET);
        assertEquals(WebappDataStorage.LAST_USED_UNSET, actualLastUsed);

        // Verify that the URL and scope for both web apps is WebappDataStorage.URL_INVALID.
        actualScope = webapp1Prefs.getString(
                WebappDataStorage.KEY_SCOPE, WebappDataStorage.URL_INVALID);
        assertEquals(WebappDataStorage.URL_INVALID, actualScope);
        actualUrl = webapp1Prefs.getString(
                WebappDataStorage.KEY_URL, WebappDataStorage.URL_INVALID);
        assertEquals(WebappDataStorage.URL_INVALID, actualUrl);
        actualScope = webapp2Prefs.getString(
                WebappDataStorage.KEY_SCOPE, WebappDataStorage.URL_INVALID);
        assertEquals(WebappDataStorage.URL_INVALID, actualScope);
        actualUrl = webapp2Prefs.getString(
                WebappDataStorage.KEY_URL, WebappDataStorage.URL_INVALID);
        assertEquals(WebappDataStorage.URL_INVALID, actualUrl);
    }

    @Test
    @Feature({"Webapp"})
    public void testGetAfterClearWebappHistory() throws Exception {
        WebappRegistry.registerWebapp("webapp", null);
        BackgroundShadowAsyncTask.runBackgroundTasks();

        SharedPreferences webappPrefs = ContextUtils.getApplicationContext().getSharedPreferences(
                WebappDataStorage.SHARED_PREFS_FILE_PREFIX + "webapp", Context.MODE_PRIVATE);
        CallbackRunner callback = new CallbackRunner();
        WebappRegistry.clearWebappHistoryForUrls(new UrlFilters.AllUrls(), callback);
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();
        assertTrue(callback.getCallbackCalled());

        // Open the webapp up to set the last used time.
        FetchStorageCallback storageCallback = new FetchStorageCallback(null);
        WebappRegistry.getWebappDataStorage("webapp", storageCallback);
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();
        assertTrue(storageCallback.getCallbackCalled());

        // Verify that the last used time is valid.
        long actualLastUsed = webappPrefs.getLong(
                WebappDataStorage.KEY_LAST_USED, WebappDataStorage.LAST_USED_INVALID);
        assertTrue(WebappDataStorage.LAST_USED_INVALID != actualLastUsed);
        assertTrue(WebappDataStorage.LAST_USED_UNSET != actualLastUsed);
    }

    @Test
    @Feature({"Webapp"})
    public void testUpdateAfterClearWebappHistory() throws Exception {
        final String webappUrl = "http://www.google.com";
        final String webappScope = "http://www.google.com/";
        final Intent shortcutIntent = createShortcutIntent(webappUrl);
        WebappRegistry.registerWebapp("webapp", new FetchStorageCallback(shortcutIntent));
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();

        SharedPreferences webappPrefs = ContextUtils.getApplicationContext().getSharedPreferences(
                WebappDataStorage.SHARED_PREFS_FILE_PREFIX + "webapp", Context.MODE_PRIVATE);

        // Verify that the URL and scope match the original in the intent.
        String actualUrl = webappPrefs.getString(
                WebappDataStorage.KEY_URL, WebappDataStorage.URL_INVALID);
        assertEquals(webappUrl, actualUrl);
        String actualScope = webappPrefs.getString(
                WebappDataStorage.KEY_SCOPE, WebappDataStorage.URL_INVALID);
        assertEquals(webappScope, actualScope);

        WebappRegistry.clearWebappHistoryForUrls(new UrlFilters.AllUrls(), new CallbackRunner());
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();

        // Update the webapp from the intent again.
        WebappRegistry.getWebappDataStorage("webapp", new FetchStorageCallback(shortcutIntent));
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();

        // Verify that the URL and scope match the original in the intent.
        actualUrl = webappPrefs.getString(WebappDataStorage.KEY_URL, WebappDataStorage.URL_INVALID);
        assertEquals(webappUrl, actualUrl);
        actualScope = webappPrefs.getString(
                WebappDataStorage.KEY_SCOPE, WebappDataStorage.URL_INVALID);
        assertEquals(webappScope, actualScope);
    }

    @Test
    @Feature({"Webapp"})
    public void testGetWebappDataStorageForUrl() throws Exception {
        // Ensure that getWebappDataStorageForUrl returns the correct WebappDataStorage object.
        // URLs should return the WebappDataStorage with the longest scope that the URL starts with.
        final String webapp1Url = "https://www.google.com/";
        final String webapp2Url = "https://drive.google.com/";
        final String webapp3Url = "https://www.google.com/drive/index.html";
        final String webapp4Url = "https://www.google.com/drive/docs/index.html";

        final String webapp3Scope = "https://www.google.com/drive/";
        final String webapp4Scope = "https://www.google.com/drive/docs/";

        final String test1Url = "https://www.google.com/index.html";
        final String test2Url = "https://www.google.com/drive/recent.html";
        final String test3Url = "https://www.google.com/drive/docs/recent.html";
        final String test4Url = "https://www.google.com/drive/docs/recent/index.html";
        final String test5Url = "https://drive.google.com/docs/recent/trash";
        final String test6Url = "https://maps.google.com/";

        Intent shortcutIntent1 = createShortcutIntent(webapp1Url);
        Intent shortcutIntent2 = createShortcutIntent(webapp2Url);
        Intent shortcutIntent3 = createShortcutIntent(webapp3Url);
        Intent shortcutIntent4 = createShortcutIntent(webapp4Url);

        // Register the four web apps.
        WebappRegistry.registerWebapp("webapp1", new FetchStorageCallback(shortcutIntent1));
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();

        WebappRegistry.registerWebapp("webapp2", new FetchStorageCallback(shortcutIntent2));
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();

        WebappRegistry.registerWebapp("webapp3", new FetchStorageCallback(shortcutIntent3));
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();

        WebappRegistry.registerWebapp("webapp4", new FetchStorageCallback(shortcutIntent4));
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();

        // test1Url should return webapp1.
        FetchStorageByUrlCallback callback = new FetchStorageByUrlCallback(webapp1Url, webapp1Url);
        WebappRegistry.getWebappDataStorageForUrl(test1Url, callback);
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();
        assertTrue(callback.getCallbackCalled());

        // test2Url should return webapp3.
        callback = new FetchStorageByUrlCallback(webapp3Url, webapp3Scope);
        WebappRegistry.getWebappDataStorageForUrl(test2Url, callback);
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();
        assertTrue(callback.getCallbackCalled());

        // test3Url should return webapp4.
        callback = new FetchStorageByUrlCallback(webapp4Url, webapp4Scope);
        WebappRegistry.getWebappDataStorageForUrl(test3Url, callback);
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();
        assertTrue(callback.getCallbackCalled());

        // test4Url should return webapp4.
        callback = new FetchStorageByUrlCallback(webapp4Url, webapp4Scope);
        WebappRegistry.getWebappDataStorageForUrl(test4Url, callback);
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();
        assertTrue(callback.getCallbackCalled());

        // test5Url should return webapp2.
        callback = new FetchStorageByUrlCallback(webapp2Url, webapp2Url);
        WebappRegistry.getWebappDataStorageForUrl(test5Url, callback);
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();
        assertTrue(callback.getCallbackCalled());

        // test6Url doesn't correspond to a web app, so the storage returned is null.
        // This must use a member variable; local variables must be final or effectively final to be
        // accessible inside an inner class.
        mCallbackCalled = false;
        WebappRegistry.getWebappDataStorageForUrl(
                test6Url, new WebappRegistry.FetchWebappDataStorageCallback() {
                    @Override
                    public void onWebappDataStorageRetrieved(WebappDataStorage storage) {
                        assertEquals(null, storage);
                        mCallbackCalled = true;
                    }
                });
        BackgroundShadowAsyncTask.runBackgroundTasks();
        ShadowLooper.runUiThreadTasks();
        assertTrue(mCallbackCalled);
    }

    private Set<String> addWebappsToRegistry(String... webapps) {
        final Set<String> expected = new HashSet<>(Arrays.asList(webapps));
        mSharedPreferences.edit().putStringSet(WebappRegistry.KEY_WEBAPP_SET, expected).apply();
        return expected;
    }

    private Set<String> getRegisteredWebapps() {
        return mSharedPreferences.getStringSet(
                WebappRegistry.KEY_WEBAPP_SET, Collections.<String>emptySet());
    }

    private Intent createShortcutIntent(final String url) throws Exception {
        AsyncTask<Void, Void, Intent> shortcutIntentTask = new AsyncTask<Void, Void, Intent>() {
            @Override
            protected Intent doInBackground(Void... nothing) {
                return ShortcutHelper.createWebappShortcutIntentForTesting("id", url);
            }
        };
        return shortcutIntentTask.execute().get();
    }

    private Intent createWebApkIntent(String webappId, String webApkPackage) {
        Intent intent = new Intent();
        intent.putExtra(ShortcutHelper.EXTRA_ID, webappId)
              .putExtra(ShortcutHelper.EXTRA_URL, "https://foo.com")
              .putExtra(ShortcutHelper.EXTRA_WEBAPK_PACKAGE_NAME, webApkPackage);
        return intent;
    }
}
