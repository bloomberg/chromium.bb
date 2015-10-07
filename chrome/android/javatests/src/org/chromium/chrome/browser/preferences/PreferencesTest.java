// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.accounts.Account;
import android.app.Activity;
import android.app.DialogFragment;
import android.app.Instrumentation;
import android.content.Context;
import android.content.Intent;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.preference.PreferenceScreen;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.accessibility.FontSizePrefs;
import org.chromium.chrome.browser.preferences.website.ContentSetting;
import org.chromium.chrome.browser.preferences.website.GeolocationInfo;
import org.chromium.chrome.browser.preferences.website.WebsitePreferenceBridge;
import org.chromium.chrome.browser.search_engines.TemplateUrlService;
import org.chromium.chrome.browser.search_engines.TemplateUrlService.LoadListener;
import org.chromium.chrome.browser.search_engines.TemplateUrlService.TemplateUrl;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.sync.ui.ChooseAccountFragment;
import org.chromium.content.browser.test.NativeLibraryTestBase;
import org.chromium.content.browser.test.util.CallbackHelper;
import org.chromium.content.browser.test.util.UiUtils;
import org.chromium.sync.signin.AccountManagerHelper;
import org.chromium.sync.test.util.MockAccountManager;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.text.NumberFormat;
import java.util.List;
import java.util.concurrent.Callable;

/**
 * Tests for the Settings menu.
 */
public class PreferencesTest extends NativeLibraryTestBase {

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        loadNativeLibraryAndInitBrowserProcess();
    }

    /**
     * Launches the preferences menu and starts the preferences activity named fragmentName.
     * Returns the activity that was started.
     */
    public static Preferences startPreferences(Instrumentation instrumentation,
            String fragmentName) {
        Context context = instrumentation.getTargetContext();
        Intent intent = PreferencesLauncher.createIntentForSettingsPage(context, fragmentName);
        Activity activity = instrumentation.startActivitySync(intent);
        assertTrue(activity instanceof Preferences);
        return (Preferences) activity;
    }

    public static void clickPreference(PreferenceFragment fragment, Preference preference) {
        try {
            Method performClick = Preference.class.getDeclaredMethod("performClick",
                    PreferenceScreen.class);
            performClick.invoke(preference, fragment.getPreferenceScreen());
        } catch (IllegalAccessException e) {
            throw new RuntimeException(e);
        } catch (IllegalArgumentException e) {
            throw new RuntimeException(e);
        } catch (NoSuchMethodException e) {
            throw new RuntimeException(e);
        } catch (InvocationTargetException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Change search engine and make sure it works correctly.
     */
    @DisabledTest // Fails on android-one: crbug.com/540720
    @SmallTest
    @Feature({"Preferences"})
    public void testSearchEnginePreference() throws Exception {
        ensureTemplateUrlServiceLoaded();

        // Set the second search engine as the default using TemplateUrlService.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                TemplateUrlService.getInstance().setSearchEngine(1);
            }
        });

        final Preferences prefActivity = startPreferences(getInstrumentation(),
                MainPreferences.class.getName());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // Ensure that the second search engine in the list is selected.
                PreferenceFragment fragment = (PreferenceFragment)
                        prefActivity.getFragmentForTest();
                SearchEnginePreference pref = (SearchEnginePreference)
                        fragment.findPreference(SearchEnginePreference.PREF_SEARCH_ENGINE);
                assertNotNull(pref);
                assertEquals("1", pref.getValueForTesting());

                // Simulate selecting the third search engine, ensure that TemplateUrlService is
                // updated, but location permission not granted for the new engine.
                pref.setValueForTesting("2");
                TemplateUrlService templateUrlService = TemplateUrlService.getInstance();
                assertEquals(2, templateUrlService.getDefaultSearchEngineIndex());
                assertEquals(ContentSetting.ASK, locationPermissionForSearchEngine(2));

                // Simulate selecting the fourth search engine and but set a blocked permission
                // first and ensure that location permission is NOT granted.
                String url = templateUrlService.getSearchEngineUrlFromTemplateUrl(3);
                WebsitePreferenceBridge.nativeSetGeolocationSettingForOrigin(
                        url, url, ContentSetting.BLOCK.toInt(), false);
                pref.setValueForTesting("3");
                assertEquals(3, TemplateUrlService.getInstance().getDefaultSearchEngineIndex());
                assertEquals(ContentSetting.BLOCK, locationPermissionForSearchEngine(3));
                assertEquals(ContentSetting.ASK, locationPermissionForSearchEngine(2));

                // Make sure a pre-existing ALLOW value does not get deleted when switching away
                // from a search engine.
                url = templateUrlService.getSearchEngineUrlFromTemplateUrl(4);
                WebsitePreferenceBridge.nativeSetGeolocationSettingForOrigin(
                        url, url, ContentSetting.ALLOW.toInt(), false);
                pref.setValueForTesting("4");
                assertEquals(4, TemplateUrlService.getInstance().getDefaultSearchEngineIndex());
                assertEquals(ContentSetting.ALLOW, locationPermissionForSearchEngine(4));
                pref.setValueForTesting("3");
                assertEquals(ContentSetting.ALLOW, locationPermissionForSearchEngine(4));
            }
        });
    }

    /**
     * Make sure that when a user switches to a search engine that uses HTTP, the location
     * permission is not added.
     */
    @DisabledTest // Fails on android-one: crbug.com/540706
    @SmallTest
    @Feature({"Preferences"})
    public void testSearchEnginePreferenceHttp() throws Exception {
        ensureTemplateUrlServiceLoaded();

        final Preferences prefActivity = startPreferences(getInstrumentation(),
                MainPreferences.class.getName());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // Ensure that the second search engine in the list is selected.
                PreferenceFragment fragment = (PreferenceFragment)
                        prefActivity.getFragmentForTest();
                SearchEnginePreference pref = (SearchEnginePreference)
                        fragment.findPreference(SearchEnginePreference.PREF_SEARCH_ENGINE);
                assertNotNull(pref);
                assertEquals("0", pref.getValueForTesting());

                // Simulate selecting a search engine that uses HTTP.
                int index = indexOfFirstHttpSearchEngine();
                pref.setValueForTesting(Integer.toString(index));

                TemplateUrlService templateUrlService = TemplateUrlService.getInstance();
                assertEquals(index, templateUrlService.getDefaultSearchEngineIndex());
                assertEquals(ContentSetting.ASK, locationPermissionForSearchEngine(index));
            }
        });
    }

    private int indexOfFirstHttpSearchEngine() {
        TemplateUrlService templateUrlService = TemplateUrlService.getInstance();
        List<TemplateUrl> urls = templateUrlService.getLocalizedSearchEngines();
        int index;
        for (index = 0; index < urls.size(); ++index) {
            String url = templateUrlService.getSearchEngineUrlFromTemplateUrl(index);
            if (url.startsWith("http:")) {
                return index;
            }
        }
        fail();
        return index;
    }

    private void ensureTemplateUrlServiceLoaded() throws Exception {
        // Make sure the template_url_service is loaded.
        final CallbackHelper onTemplateUrlServiceLoadedHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                if (TemplateUrlService.getInstance().isLoaded()) {
                    onTemplateUrlServiceLoadedHelper.notifyCalled();
                } else {
                    TemplateUrlService.getInstance().registerLoadListener(new LoadListener() {
                        @Override
                        public void onTemplateUrlServiceLoaded() {
                            onTemplateUrlServiceLoadedHelper.notifyCalled();
                        }
                    });
                    TemplateUrlService.getInstance().load();
                }
            }
        });
        onTemplateUrlServiceLoadedHelper.waitForCallback(0);
    }

    private ContentSetting locationPermissionForSearchEngine(int index) {
        String url = TemplateUrlService.getInstance().getSearchEngineUrlFromTemplateUrl(index);
        GeolocationInfo locationSettings = new GeolocationInfo(url, null, false);
        ContentSetting locationPermission = locationSettings.getContentSetting();
        return locationPermission;
    }

    /**
     * Tests that double-clicking on sign-in doesn't show two sign-in prompts.
     *
     * This is a regression test for http://crbug.com/515055.
     */
    @SmallTest
    @Feature({"Preferences"})
    public void testDoubleSignin() throws Exception {
        // Sets up state so that displayAccountPicker() shows a ChooseAccountFragment.
        setUpTestAccount();
        final Preferences prefActivity = startPreferences(getInstrumentation(),
                MainPreferences.class.getName());
        final MainPreferences mainPrefs = (MainPreferences) prefActivity.getFragmentForTest();

        DialogFragment fragment1 = displayAccountPicker(mainPrefs);
        DialogFragment fragment2 = displayAccountPicker(mainPrefs);
        assertTrue(fragment1 instanceof ChooseAccountFragment);
        assertNull(fragment2);
    }

    private DialogFragment displayAccountPicker(final MainPreferences mainPrefs) {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<DialogFragment>() {
            public DialogFragment call() {
                return mainPrefs.displayAccountPicker();
            }
        });
    }

    private void setUpTestAccount() {
        final Context context = getInstrumentation().getTargetContext();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                SigninManager.get(context).onFirstRunCheckDone();
            }
        });
        Account account = AccountManagerHelper.createAccountFromName("test@chromium.org");
        MockAccountManager accountManager = new MockAccountManager(context, context, account);
        AccountManagerHelper.overrideAccountManagerHelperForTests(context, accountManager);
    }

    // TODO(mvanouwerkerk): Write new preference intent tests for notification settings.
    // https://crbug.com/461885

    /**
     * Tests setting FontScaleFactor and ForceEnableZoom in AccessibilityPreferences and ensures
     * that ForceEnableZoom changes corresponding to FontScaleFactor.
     */
    @SmallTest
    @Feature({"Accessibility"})
    public void testAccessibilityPreferences() throws Exception {
        String accessibilityPrefClassname = AccessibilityPreferences.class.getName();
        AccessibilityPreferences accessibilityPref = (AccessibilityPreferences)
                startPreferences(getInstrumentation(), accessibilityPrefClassname)
                .getFragmentForTest();
        SeekBarPreference textScalePref = (SeekBarPreference) accessibilityPref.findPreference(
                AccessibilityPreferences.PREF_TEXT_SCALE);
        SeekBarLinkedCheckBoxPreference forceEnableZoomPref =
                (SeekBarLinkedCheckBoxPreference) accessibilityPref
                        .findPreference(AccessibilityPreferences.PREF_FORCE_ENABLE_ZOOM);
        NumberFormat percentFormat = NumberFormat.getPercentInstance();
        // Arbitrary value 0.4f to be larger and smaller than threshold.
        float fontSmallerThanThreshold =
                AccessibilityPreferences.FORCE_ENABLE_ZOOM_THRESHOLD_MULTIPLIER - 0.4f;
        float fontBiggerThanThreshold =
                AccessibilityPreferences.FORCE_ENABLE_ZOOM_THRESHOLD_MULTIPLIER + 0.4f;

        // Set the textScaleFactor above the threshold.
        userSetTextScale(accessibilityPref, textScalePref, fontBiggerThanThreshold);
        UiUtils.settleDownUI(getInstrumentation());
        // Since above the threshold, this will check the force enable zoom button.
        assertEquals(percentFormat.format(fontBiggerThanThreshold), textScalePref.getSummary());
        assertTrue(forceEnableZoomPref.isChecked());
        assertFontSizePrefs(true, fontBiggerThanThreshold);

        // Set the textScaleFactor below the threshold.
        userSetTextScale(accessibilityPref, textScalePref, fontSmallerThanThreshold);
        UiUtils.settleDownUI(getInstrumentation());
        // Since below the threshold and userSetForceEnableZoom is false, this will uncheck
        // the force enable zoom button.
        assertEquals(percentFormat.format(fontSmallerThanThreshold), textScalePref.getSummary());
        assertFalse(forceEnableZoomPref.isChecked());
        assertFontSizePrefs(false, fontSmallerThanThreshold);

        userSetTextScale(accessibilityPref, textScalePref, fontBiggerThanThreshold);
        // Sets onUserSetForceEnableZoom to be true.
        userSetForceEnableZoom(accessibilityPref, forceEnableZoomPref, true);
        UiUtils.settleDownUI(getInstrumentation());
        // Since userSetForceEnableZoom is true, when the text scale is moved below the threshold
        // ForceEnableZoom should remain checked.
        userSetTextScale(accessibilityPref, textScalePref, fontSmallerThanThreshold);
        assertTrue(forceEnableZoomPref.isChecked());
        assertFontSizePrefs(true, fontSmallerThanThreshold);
    }

    private void assertFontSizePrefs(final boolean expectedForceEnableZoom,
            final float expectedFontScale) {
        final Context targetContext = getInstrumentation().getTargetContext();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                FontSizePrefs fontSizePrefs = FontSizePrefs.getInstance(targetContext);
                assertEquals(expectedForceEnableZoom, fontSizePrefs.getForceEnableZoom());
                assertEquals(expectedFontScale, fontSizePrefs.getFontScaleFactor(), 0.001f);
            }
        });
    }

    private static void userSetTextScale(final AccessibilityPreferences accessibilityPref,
            final SeekBarPreference textScalePref, final float textScale) {
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                accessibilityPref.onPreferenceChange(textScalePref, textScale);
            }
        });
    }

    private static void userSetForceEnableZoom(final AccessibilityPreferences accessibilityPref,
            final SeekBarLinkedCheckBoxPreference forceEnableZoomPref, final boolean enabled) {
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                accessibilityPref.onPreferenceChange(forceEnableZoomPref, enabled);
            }
        });
    }
}
