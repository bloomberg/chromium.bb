// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.developer;

import static android.app.Notification.FLAG_ONGOING_EVENT;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.app.AlertDialog;
import android.app.Notification;
import android.app.PendingIntent;
import android.content.Context;
import android.preference.CheckBoxPreference;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.support.v4.app.NotificationCompat;
import android.util.Pair;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ButtonPreference;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.PreferencesTest;
import org.chromium.chrome.browser.preferences.TextMessagePreference;
import org.chromium.chrome.browser.tracing.TracingController;
import org.chromium.chrome.browser.tracing.TracingNotificationManager;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.notifications.MockNotificationManagerProxy;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

import java.io.File;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Tests for the Tracing settings menu.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TracingPreferencesTest {
    @Rule
    public final ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private MockNotificationManagerProxy mMockNotificationManager;

    @Before
    public void setUp() throws Exception {
        mMockNotificationManager = new MockNotificationManagerProxy();
        TracingNotificationManager.overrideNotificationManagerForTesting(mMockNotificationManager);
    }

    @After
    public void tearDown() throws Exception {
        TracingNotificationManager.overrideNotificationManagerForTesting(null);
    }

    /**
     * Waits until a notification has been displayed and then returns a NotificationEntry object to
     * the caller. Requires that only a single notification is displayed.
     *
     * @return The NotificationEntry object tracked by the MockNotificationManagerProxy.
     */
    private MockNotificationManagerProxy.NotificationEntry waitForNotification() {
        waitForNotificationManagerMutation();
        List<MockNotificationManagerProxy.NotificationEntry> notifications =
                mMockNotificationManager.getNotifications();
        Assert.assertEquals(1, notifications.size());
        return notifications.get(0);
    }

    /**
     * Waits for a mutation to occur in the mocked notification manager. This indicates that Chrome
     * called into Android to notify or cancel a notification.
     */
    private void waitForNotificationManagerMutation() {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mMockNotificationManager.getMutationCountAndDecrement() > 0;
            }
        }, scaleTimeout(15000) /* maxTimeoutMs */, 50 /* checkIntervalMs */);
    }

    private void waitForTracingControllerInitialization(PreferenceFragment fragment)
            throws Exception {
        final Preference defaultCategoriesPref =
                fragment.findPreference(TracingPreferences.UI_PREF_DEFAULT_CATEGORIES);
        final Preference nonDefaultCategoriesPref =
                fragment.findPreference(TracingPreferences.UI_PREF_NON_DEFAULT_CATEGORIES);
        final Preference modePref = fragment.findPreference(TracingPreferences.UI_PREF_MODE);
        final Preference startTracingButton =
                fragment.findPreference(TracingPreferences.UI_PREF_START_RECORDING);

        CallbackHelper callbackHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            if (TracingController.getInstance().getState()
                    == TracingController.State.INITIALIZING) {
                // Controls should be disabled while initializing.
                Assert.assertFalse(defaultCategoriesPref.isEnabled());
                Assert.assertFalse(nonDefaultCategoriesPref.isEnabled());
                Assert.assertFalse(modePref.isEnabled());
                Assert.assertFalse(startTracingButton.isEnabled());

                TracingController.getInstance().addObserver(new TracingController.Observer() {
                    @Override
                    public void onTracingStateChanged(@TracingController.State int state) {
                        callbackHelper.notifyCalled();
                        TracingController.getInstance().removeObserver(this);
                    }
                });
                return;
            }
            // Already initialized.
            callbackHelper.notifyCalled();
        });
        callbackHelper.waitForCallback(0 /* currentCallCount */);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @Features.EnableFeatures(ChromeFeatureList.DEVELOPER_PREFERENCES)
    @DisableIf.Build(sdk_is_less_than = 21, message = "crbug.com/899894")
    public void testRecordTrace() throws Exception {
        Context context = ContextUtils.getApplicationContext();
        mActivityTestRule.startMainActivityOnBlankPage();
        Preferences activity =
                mActivityTestRule.startPreferences(TracingPreferences.class.getName());
        final PreferenceFragment fragment = (PreferenceFragment) activity.getFragmentForTest();
        final ButtonPreference startTracingButton = (ButtonPreference) fragment.findPreference(
                TracingPreferences.UI_PREF_START_RECORDING);

        waitForTracingControllerInitialization(fragment);

        // Setting to IDLE state tries to dismiss the (non-existent) notification.
        waitForNotificationManagerMutation();
        Assert.assertEquals(0, mMockNotificationManager.getNotifications().size());

        CallbackHelper callbackHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(
                    TracingController.State.IDLE, TracingController.getInstance().getState());
            Assert.assertTrue(startTracingButton.isEnabled());
            Assert.assertEquals(
                    context.getString(R.string.prefs_tracing_start), startTracingButton.getTitle());

            // Tap the button to start recording a trace.
            PreferencesTest.clickPreference(fragment, startTracingButton);

            Assert.assertEquals(
                    TracingController.State.STARTING, TracingController.getInstance().getState());
            Assert.assertFalse(startTracingButton.isEnabled());
            Assert.assertEquals(context.getString(R.string.prefs_tracing_active),
                    startTracingButton.getTitle());

            // Observe state changes to RECORDING, STOPPING, STOPPED, and IDLE.
            TracingController.getInstance().addObserver(new TracingController.Observer() {
                @TracingController.State
                int mExpectedState = TracingController.State.RECORDING;

                @Override
                public void onTracingStateChanged(@TracingController.State int state) {
                    // onTracingStateChanged() should be called four times in total, with the right
                    // order of state changes:
                    Assert.assertEquals(mExpectedState, state);
                    if (state == TracingController.State.RECORDING) {
                        mExpectedState = TracingController.State.STOPPING;
                    } else if (state == TracingController.State.STOPPING) {
                        mExpectedState = TracingController.State.STOPPED;
                    } else if (state == TracingController.State.STOPPED) {
                        mExpectedState = TracingController.State.IDLE;
                    } else {
                        TracingController.getInstance().removeObserver(this);
                    }

                    callbackHelper.notifyCalled();
                }
            });
        });

        // Wait for state change to RECORDING.
        callbackHelper.waitForCallback(0 /* currentCallCount */);

        // Recording started, a notification with a stop button should be displayed.
        Notification notification = waitForNotification().notification;
        Assert.assertEquals(FLAG_ONGOING_EVENT, notification.flags & FLAG_ONGOING_EVENT);
        Assert.assertEquals(null, notification.deleteIntent);
        Assert.assertEquals(1, NotificationCompat.getActionCount(notification));
        PendingIntent stopIntent = NotificationCompat.getAction(notification, 0).actionIntent;

        // Initiate stopping the recording and wait for state changes to STOPPING and STOPPED.
        stopIntent.send();
        callbackHelper.waitForCallback(1 /* currentCallCount */, 2 /* numberOfCallsToWaitFor */,
                scaleTimeout(15000) /* timeout */, TimeUnit.MILLISECONDS);

        // Notification should be replaced twice, once with an "is stopping" notification and then
        // with a notification to share the trace. Because the former is temporary, we can't
        // verify it reliably, so we skip over it and simply expect two notification mutations.
        waitForNotification();
        notification = waitForNotification().notification;
        Assert.assertEquals(0, notification.flags & FLAG_ONGOING_EVENT);
        Assert.assertNotEquals(null, notification.deleteIntent);
        PendingIntent deleteIntent = notification.deleteIntent;

        // The temporary tracing output file should now exist.
        File tempFile = TracingController.getInstance().getTracingTempFile();
        Assert.assertNotEquals(null, tempFile);
        Assert.assertTrue(tempFile.exists());

        // Discard the trace and wait for state change back to IDLE.
        deleteIntent.send();
        callbackHelper.waitForCallback(3 /* currentCallCount */);

        // The temporary file should be deleted asynchronously.
        CriteriaHelper.pollInstrumentationThread(Criteria.equals(false, () -> tempFile.exists()));

        // Notification should be deleted, too.
        waitForNotificationManagerMutation();
        Assert.assertEquals(0, mMockNotificationManager.getNotifications().size());
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Features.EnableFeatures(ChromeFeatureList.DEVELOPER_PREFERENCES)
    public void testNotificationsDisabledMessage() throws Exception {
        mMockNotificationManager.setNotificationsEnabled(false);

        Context context = ContextUtils.getApplicationContext();
        Preferences activity =
                mActivityTestRule.startPreferences(TracingPreferences.class.getName());
        final PreferenceFragment fragment = (PreferenceFragment) activity.getFragmentForTest();
        final ButtonPreference startTracingButton = (ButtonPreference) fragment.findPreference(
                TracingPreferences.UI_PREF_START_RECORDING);
        final TextMessagePreference statusPreference =
                (TextMessagePreference) fragment.findPreference(
                        TracingPreferences.UI_PREF_TRACING_STATUS);

        waitForTracingControllerInitialization(fragment);

        Assert.assertFalse(startTracingButton.isEnabled());
        Assert.assertEquals(context.getString(R.string.tracing_notifications_disabled),
                statusPreference.getTitle());

        mMockNotificationManager.setNotificationsEnabled(true);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @Features.EnableFeatures(ChromeFeatureList.DEVELOPER_PREFERENCES)
    @DisableIf.Build(sdk_is_less_than = 21, message = "crbug.com/908118")
    public void testSelectCategories() throws Exception {
        Context context = ContextUtils.getApplicationContext();
        // We need a renderer so that its tracing categories will be populated.
        mActivityTestRule.startMainActivityOnBlankPage();
        Preferences activity =
                mActivityTestRule.startPreferences(TracingPreferences.class.getName());
        final PreferenceFragment fragment = (PreferenceFragment) activity.getFragmentForTest();
        final Preference defaultCategoriesPref =
                fragment.findPreference(TracingPreferences.UI_PREF_DEFAULT_CATEGORIES);
        final Preference nonDefaultCategoriesPref =
                fragment.findPreference(TracingPreferences.UI_PREF_NON_DEFAULT_CATEGORIES);

        waitForTracingControllerInitialization(fragment);

        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(defaultCategoriesPref.isEnabled());
            Assert.assertTrue(nonDefaultCategoriesPref.isEnabled());
        });

        // Lists preferences for categories of a specific type and an example category name each.
        List<Pair<Preference, String>> categoriesPrefs =
                Arrays.asList(new Pair<>(defaultCategoriesPref, "toplevel"),
                        new Pair<>(nonDefaultCategoriesPref, "disabled-by-default-cc.debug"));
        for (Pair<Preference, String> categoriesPrefAndSampleCategory : categoriesPrefs) {
            Preference categoriesPref = categoriesPrefAndSampleCategory.first;
            String sampleCategoryName = categoriesPrefAndSampleCategory.second;

            // Click the preference, which should open a new preferences fragment in a new activity.
            Preferences categoriesActivity = ActivityUtils.waitForActivity(
                    InstrumentationRegistry.getInstrumentation(), Preferences.class, () -> {
                        ThreadUtils.runOnUiThreadBlocking(() -> {
                            PreferencesTest.clickPreference(fragment, categoriesPref);
                        });
                    });

            PreferenceFragment categoriesFragment = ActivityUtils.waitForFragmentToAttach(
                    categoriesActivity, PreferenceFragment.class);
            Assert.assertEquals(TracingCategoriesPreferences.class, categoriesFragment.getClass());

            CheckBoxPreference sampleCategoryPref =
                    (CheckBoxPreference) categoriesFragment.findPreference(sampleCategoryName);
            Assert.assertNotNull(sampleCategoryPref);

            boolean originallyEnabled =
                    TracingPreferences.getEnabledCategories().contains(sampleCategoryName);
            Assert.assertEquals(originallyEnabled, sampleCategoryPref.isChecked());

            // Simulate selecting / deselecting the category.
            ThreadUtils.runOnUiThreadBlocking(() -> {
                PreferencesTest.clickPreference(categoriesFragment, sampleCategoryPref);
            });
            Assert.assertNotEquals(originallyEnabled, sampleCategoryPref.isChecked());
            boolean finallyEnabled =
                    TracingPreferences.getEnabledCategories().contains(sampleCategoryName);
            Assert.assertNotEquals(originallyEnabled, finallyEnabled);

            // Return to original activity.
            Preferences originalActivity = ActivityUtils.waitForActivity(
                    InstrumentationRegistry.getInstrumentation(), Preferences.class, () -> {
                        ThreadUtils.runOnUiThreadBlocking(() -> { categoriesActivity.finish(); });
                    });

            Assert.assertEquals(activity, originalActivity);
        }
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Features.EnableFeatures(ChromeFeatureList.DEVELOPER_PREFERENCES)
    public void testSelectMode() throws Exception {
        Context context = ContextUtils.getApplicationContext();
        Preferences activity =
                mActivityTestRule.startPreferences(TracingPreferences.class.getName());
        final PreferenceFragment fragment = (PreferenceFragment) activity.getFragmentForTest();
        final ListPreference modePref =
                (ListPreference) fragment.findPreference(TracingPreferences.UI_PREF_MODE);

        waitForTracingControllerInitialization(fragment);

        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(modePref.isEnabled());

            // By default, the "record-until-full" mode is selected.
            Assert.assertEquals("record-until-full", TracingPreferences.getSelectedTracingMode());

            // Clicking the pref should open a dialog.
            PreferencesTest.clickPreference(fragment, modePref);
            Assert.assertNotNull(modePref.getDialog());
            AlertDialog dialog = (AlertDialog) modePref.getDialog();
            Assert.assertEquals(3, dialog.getListView().getAdapter().getCount());
            modePref.getDialog().dismiss();

            // Simulate changing the mode.
            modePref.getOnPreferenceChangeListener().onPreferenceChange(
                    modePref, "record-continuously");
            Assert.assertEquals("record-continuously", TracingPreferences.getSelectedTracingMode());
        });
    }
}
