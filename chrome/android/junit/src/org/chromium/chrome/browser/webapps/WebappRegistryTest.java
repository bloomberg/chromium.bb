// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.SharedPreferences;

import org.chromium.base.test.util.Feature;
import org.chromium.testing.local.BackgroundShadowAsyncTask;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import java.util.Arrays;
import java.util.Collections;
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

    @Before
    public void setUp() throws Exception {
        mSharedPreferences = Robolectric.application
                .getSharedPreferences(REGISTRY_FILE_NAME, Context.MODE_PRIVATE);
        mSharedPreferences.edit().putLong(KEY_LAST_CLEANUP, INITIAL_TIME).commit();

        mCallbackCalled = false;
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
        WebappRegistry.registerWebapp(Robolectric.application, "test");
        BackgroundShadowAsyncTask.runBackgroundTasks();

        Set<String> actual = mSharedPreferences.getStringSet(
                WebappRegistry.KEY_WEBAPP_SET, Collections.<String>emptySet());
        assertEquals(1, actual.size());
        assertTrue(actual.contains("test"));
    }

    @Test
    @Feature({"Webapp"})
    public void testWebappRegistrationUpdatesLastUsed() throws Exception {
        long before = System.currentTimeMillis();
        WebappRegistry.registerWebapp(Robolectric.application, "test");
        BackgroundShadowAsyncTask.runBackgroundTasks();
        long after = System.currentTimeMillis();

        SharedPreferences webAppPrefs = Robolectric.application.getSharedPreferences(
                WebappDataStorage.SHARED_PREFS_FILE_PREFIX + "test", Context.MODE_PRIVATE);
        long actual = webAppPrefs.getLong(WebappDataStorage.KEY_LAST_USED,
                WebappDataStorage.INVALID_LAST_USED);
        assertTrue("Timestamp is out of range", before <= actual && actual <= after);
    }

    @Test
    @Feature({"Webapp"})
    public void testWebappIdsRetrieval() throws Exception {
        final Set<String> expected = addWebappsToRegistry("first", "second");

        WebappRegistry.getRegisteredWebappIds(Robolectric.application,
                new WebappRegistry.FetchCallback() {
                    @Override
                    public void onWebappIdsRetrieved(Set<String> actual) {
                        mCallbackCalled = true;
                        assertEquals(expected, actual);
                    }
                });
        BackgroundShadowAsyncTask.runBackgroundTasks();
        Robolectric.runUiThreadTasks();

        assertTrue(mCallbackCalled);
    }

    @Test
    @Feature({"Webapp"})
    public void testWebappIdsRetrievalRegisterRetrival() throws Exception {
        final Set<String> expected = addWebappsToRegistry("first");

        WebappRegistry.getRegisteredWebappIds(Robolectric.application,
                new WebappRegistry.FetchCallback() {
                    @Override
                    public void onWebappIdsRetrieved(Set<String> actual) {
                        mCallbackCalled = true;
                        assertEquals(expected, actual);
                    }
                });
        BackgroundShadowAsyncTask.runBackgroundTasks();
        Robolectric.runUiThreadTasks();

        assertTrue(mCallbackCalled);
        mCallbackCalled = false;

        WebappRegistry.registerWebapp(Robolectric.application, "second");
        BackgroundShadowAsyncTask.runBackgroundTasks();

        // A copy of the expected set needs to be made as the SharedPreferences is using the copy
        // that was paassed to it.
        final Set<String> secondExpected = new HashSet<String>(expected);
        secondExpected.add("second");

        WebappRegistry.getRegisteredWebappIds(Robolectric.application,
                new WebappRegistry.FetchCallback() {
                    @Override
                    public void onWebappIdsRetrieved(Set<String> actual) {
                        mCallbackCalled = true;
                        assertEquals(secondExpected, actual);
                    }
                });
        BackgroundShadowAsyncTask.runBackgroundTasks();
        Robolectric.runUiThreadTasks();

        assertTrue(mCallbackCalled);
    }

    @Test
    @Feature({"Webapp"})
    public void testUnregisterRunsCallback() throws Exception {
        WebappRegistry.unregisterAllWebapps(Robolectric.application, new Runnable() {
            @Override
            public void run() {
                mCallbackCalled = true;
            }
        });
        BackgroundShadowAsyncTask.runBackgroundTasks();
        Robolectric.runUiThreadTasks();

        assertTrue(mCallbackCalled);
    }

    @Test
    @Feature({"Webapp"})
    public void testUnregisterClearsRegistry() throws Exception {
        addWebappsToRegistry("test");

        WebappRegistry.unregisterAllWebapps(Robolectric.application, null);
        BackgroundShadowAsyncTask.runBackgroundTasks();

        assertTrue(getRegisteredWebapps().isEmpty());
    }

    @Test
    @Feature({"Webapp"})
    public void testUnregisterClearsWebappDataStorage() throws Exception {
        addWebappsToRegistry("test");
        SharedPreferences webAppPrefs = Robolectric.application.getSharedPreferences(
                WebappDataStorage.SHARED_PREFS_FILE_PREFIX + "test", Context.MODE_PRIVATE);
        webAppPrefs.edit()
                .putLong(WebappDataStorage.KEY_LAST_USED, 100L)
                .commit();

        WebappRegistry.unregisterAllWebapps(Robolectric.application, null);
        BackgroundShadowAsyncTask.runBackgroundTasks();

        Map<String, ?> actual = webAppPrefs.getAll();
        assertTrue(actual.isEmpty());
    }

    @Test
    @Feature({"Webapp"})
      public void testCleanupDoesNotRunTooOften() throws Exception {
        // Put the current time to just before the task should run.
        long currentTime = INITIAL_TIME + WebappRegistry.FULL_CLEANUP_DURATION - 1;

        addWebappsToRegistry("oldWebapp");
        SharedPreferences webAppPrefs = Robolectric.application.getSharedPreferences(
                WebappDataStorage.SHARED_PREFS_FILE_PREFIX + "oldWebapp", Context.MODE_PRIVATE);
        webAppPrefs.edit()
                .putLong(WebappDataStorage.KEY_LAST_USED, Long.MIN_VALUE)
                .commit();

        WebappRegistry.unregisterOldWebapps(Robolectric.application, currentTime);
        BackgroundShadowAsyncTask.runBackgroundTasks();

        Set<String> actual = mSharedPreferences.getStringSet(
                WebappRegistry.KEY_WEBAPP_SET, Collections.<String>emptySet());
        assertEquals(new HashSet<String>(Arrays.asList("oldWebapp")), actual);

        long actualLastUsed = webAppPrefs.getLong(WebappDataStorage.KEY_LAST_USED,
                WebappDataStorage.INVALID_LAST_USED);
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
        SharedPreferences webAppPrefs = Robolectric.application.getSharedPreferences(
                WebappDataStorage.SHARED_PREFS_FILE_PREFIX + "recentWebapp", Context.MODE_PRIVATE);
        long lastUsed = currentTime - WebappRegistry.WEBAPP_UNOPENED_CLEANUP_DURATION + 1;
        webAppPrefs.edit()
                .putLong(WebappDataStorage.KEY_LAST_USED, lastUsed)
                .commit();

        // Because the time is just inside the window, there should be a cleanup but the web app
        // should not be deleted as it was used recently. The last cleanup time should also be
        // set to the current time.
        WebappRegistry.unregisterOldWebapps(Robolectric.application, currentTime);
        BackgroundShadowAsyncTask.runBackgroundTasks();

        Set<String> actual = mSharedPreferences.getStringSet(
                WebappRegistry.KEY_WEBAPP_SET, Collections.<String>emptySet());
        assertEquals(new HashSet<String>(Arrays.asList("recentWebapp")), actual);

        long actualLastUsed = webAppPrefs.getLong(WebappDataStorage.KEY_LAST_USED,
                WebappDataStorage.INVALID_LAST_USED);
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
        SharedPreferences webAppPrefs = Robolectric.application.getSharedPreferences(
                WebappDataStorage.SHARED_PREFS_FILE_PREFIX + "oldWebapp", Context.MODE_PRIVATE);
        long lastUsed = currentTime - WebappRegistry.WEBAPP_UNOPENED_CLEANUP_DURATION;
        webAppPrefs.edit()
                .putLong(WebappDataStorage.KEY_LAST_USED, lastUsed)
                .commit();

        // Because the time is just inside the window, there should be a cleanup of old web apps and
        // the last cleaned up time should be set to the current time.
        WebappRegistry.unregisterOldWebapps(Robolectric.application, currentTime);
        BackgroundShadowAsyncTask.runBackgroundTasks();

        Set<String> actual = mSharedPreferences.getStringSet(
                WebappRegistry.KEY_WEBAPP_SET, Collections.<String>emptySet());
        assertTrue(actual.isEmpty());

        long actualLastUsed = webAppPrefs.getLong(WebappDataStorage.KEY_LAST_USED,
                WebappDataStorage.INVALID_LAST_USED);
        assertEquals(WebappDataStorage.INVALID_LAST_USED, actualLastUsed);

        long lastCleanup = mSharedPreferences.getLong(WebappRegistry.KEY_LAST_CLEANUP, -1);
        assertEquals(currentTime, lastCleanup);
    }

    private Set<String> addWebappsToRegistry(String... webapps) {
        final Set<String> expected = new HashSet<String>(Arrays.asList(webapps));
        mSharedPreferences.edit()
                .putStringSet(WebappRegistry.KEY_WEBAPP_SET, expected)
                .commit();
        return expected;
    }

    private Set<String> getRegisteredWebapps() {
        return mSharedPreferences.getStringSet(
                WebappRegistry.KEY_WEBAPP_SET, Collections.<String>emptySet());
    }
}