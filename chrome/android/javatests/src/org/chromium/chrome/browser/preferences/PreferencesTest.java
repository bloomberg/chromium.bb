// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Context;
import android.content.Intent;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.preference.PreferenceScreen;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.accessibility.FontSizePrefs;
import org.chromium.chrome.browser.preferences.website.ContentSetting;
import org.chromium.chrome.browser.preferences.website.GeolocationInfo;
import org.chromium.chrome.browser.preferences.website.WebsitePreferenceBridge;
import org.chromium.chrome.browser.search_engines.TemplateUrlService;
import org.chromium.chrome.browser.search_engines.TemplateUrlService.LoadListener;
import org.chromium.chrome.browser.search_engines.TemplateUrlService.TemplateUrl;
import org.chromium.content.browser.test.NativeLibraryTestRule;
import org.chromium.content.browser.test.util.UiUtils;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.text.NumberFormat;
import java.util.List;

/**
 * Tests for the Settings menu.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class PreferencesTest {
    @Rule
    public NativeLibraryTestRule mActivityTestRule = new NativeLibraryTestRule();

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.loadNativeLibraryAndInitBrowserProcess();
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
        Assert.assertTrue(activity instanceof Preferences);
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
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableIf.Build(hardware_is = "sprout", message = "crashes on android-one: crbug.com/540720")
    @RetryOnFailure
    public void testSearchEnginePreference() throws Exception {
        ensureTemplateUrlServiceLoaded();

        final Preferences prefActivity =
                startPreferences(InstrumentationRegistry.getInstrumentation(),
                        SearchEnginePreference.class.getName());

        // Set the second search engine as the default using TemplateUrlService.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                SearchEnginePreference pref =
                        (SearchEnginePreference) prefActivity.getFragmentForTest();
                pref.setValueForTesting("1");
            }
        });


        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // Ensure that the second search engine in the list is selected.
                SearchEnginePreference pref =
                        (SearchEnginePreference) prefActivity.getFragmentForTest();
                Assert.assertNotNull(pref);
                Assert.assertEquals("1", pref.getValueForTesting());

                // Simulate selecting the third search engine, ensure that TemplateUrlService is
                // updated, and location permission granted by default for the new engine.
                String keyword2 = pref.setValueForTesting("2");
                TemplateUrlService templateUrlService = TemplateUrlService.getInstance();
                Assert.assertEquals(keyword2,
                        templateUrlService.getDefaultSearchEngineTemplateUrl().getKeyword());
                Assert.assertEquals(
                        ContentSetting.ALLOW, locationPermissionForSearchEngine(keyword2));

                // Simulate selecting the fourth search engine and but set a blocked permission
                // first and ensure that location permission is NOT granted.
                String keyword3 = pref.getKeywordFromIndexForTesting(3);
                String url = templateUrlService.getSearchEngineUrlFromTemplateUrl(keyword3);
                WebsitePreferenceBridge.nativeSetGeolocationSettingForOrigin(
                        url, url, ContentSetting.BLOCK.toInt(), false);
                keyword3 = pref.setValueForTesting("3");
                Assert.assertEquals(keyword3,
                        TemplateUrlService.getInstance()
                                .getDefaultSearchEngineTemplateUrl()
                                .getKeyword());
                Assert.assertEquals(
                        ContentSetting.BLOCK, locationPermissionForSearchEngine(keyword3));
                Assert.assertEquals(
                        ContentSetting.ASK, locationPermissionForSearchEngine(keyword2));

                // Make sure a pre-existing ALLOW value does not get deleted when switching away
                // from a search engine. For this to work we need to change the DSE setting to true
                // for search engine 3 before changing to search engine 2. Otherwise the false DSE
                // setting will cause the content setting for search engine 2 to be reset when we
                // switch to it.
                WebsitePreferenceBridge.setDSEGeolocationSetting(true);
                keyword2 = pref.getKeywordFromIndexForTesting(2);
                url = templateUrlService.getSearchEngineUrlFromTemplateUrl(keyword2);
                WebsitePreferenceBridge.nativeSetGeolocationSettingForOrigin(
                        url, url, ContentSetting.ALLOW.toInt(), false);
                keyword2 = pref.setValueForTesting("2");
                Assert.assertEquals(keyword2,
                        TemplateUrlService.getInstance()
                                .getDefaultSearchEngineTemplateUrl()
                                .getKeyword());

                Assert.assertEquals(
                        ContentSetting.ALLOW, locationPermissionForSearchEngine(keyword2));
                pref.setValueForTesting("3");
                Assert.assertEquals(
                        ContentSetting.ALLOW, locationPermissionForSearchEngine(keyword2));
            }
        });
    }

    /**
     * Make sure that when a user switches to a search engine that uses HTTP, the location
     * permission is not added.
     */
    /*
     * @SmallTest
     * @Feature({"Preferences"})
     * BUG=crbug.com/540706
     */
    @Test
    @FlakyTest
    @DisableIf.Build(hardware_is = "sprout", message = "fails on android-one: crbug.com/540706")
    public void testSearchEnginePreferenceHttp() throws Exception {
        ensureTemplateUrlServiceLoaded();

        final Preferences prefActivity =
                startPreferences(InstrumentationRegistry.getInstrumentation(),
                        SearchEnginePreference.class.getName());

        // Set the first search engine as the default using TemplateUrlService.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                SearchEnginePreference pref =
                        (SearchEnginePreference) prefActivity.getFragmentForTest();
                pref.setValueForTesting("0");
            }
        });

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                // Ensure that the first search engine in the list is selected.
                SearchEnginePreference pref =
                        (SearchEnginePreference) prefActivity.getFragmentForTest();
                Assert.assertNotNull(pref);
                Assert.assertEquals("0", pref.getValueForTesting());

                // Simulate selecting a search engine that uses HTTP.
                int index = indexOfFirstHttpSearchEngine(pref);
                String keyword = pref.setValueForTesting(Integer.toString(index));

                TemplateUrlService templateUrlService = TemplateUrlService.getInstance();
                Assert.assertEquals(keyword,
                        templateUrlService.getDefaultSearchEngineTemplateUrl().getKeyword());
                Assert.assertEquals(ContentSetting.ASK, locationPermissionForSearchEngine(keyword));
            }
        });
    }

    private int indexOfFirstHttpSearchEngine(SearchEnginePreference pref) {
        TemplateUrlService templateUrlService = TemplateUrlService.getInstance();
        List<TemplateUrl> urls = templateUrlService.getSearchEngines();
        int index;
        for (index = 0; index < urls.size(); ++index) {
            String keyword = pref.getKeywordFromIndexForTesting(index);
            String url = templateUrlService.getSearchEngineUrlFromTemplateUrl(keyword);
            if (url.startsWith("http:")) {
                return index;
            }
        }
        Assert.fail();
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

    private ContentSetting locationPermissionForSearchEngine(String keyword) {
        String url = TemplateUrlService.getInstance().getSearchEngineUrlFromTemplateUrl(keyword);
        GeolocationInfo locationSettings = new GeolocationInfo(url, null, false);
        ContentSetting locationPermission = locationSettings.getContentSetting();
        return locationPermission;
    }

    /**
     * Tests setting FontScaleFactor and ForceEnableZoom in AccessibilityPreferences and ensures
     * that ForceEnableZoom changes corresponding to FontScaleFactor.
     */
    @Test
    @SmallTest
    @Feature({"Accessibility"})
    public void testAccessibilityPreferences() throws Exception {
        String accessibilityPrefClassname = AccessibilityPreferences.class.getName();
        AccessibilityPreferences accessibilityPref = (AccessibilityPreferences) startPreferences(
                InstrumentationRegistry.getInstrumentation(), accessibilityPrefClassname)
                                                             .getFragmentForTest();
        SeekBarPreference textScalePref = (SeekBarPreference) accessibilityPref.findPreference(
                AccessibilityPreferences.PREF_TEXT_SCALE);
        SeekBarLinkedCheckBoxPreference forceEnableZoomPref =
                (SeekBarLinkedCheckBoxPreference) accessibilityPref
                        .findPreference(AccessibilityPreferences.PREF_FORCE_ENABLE_ZOOM);
        NumberFormat percentFormat = NumberFormat.getPercentInstance();
        // Arbitrary value 0.4f to be larger and smaller than threshold.
        float fontSmallerThanThreshold =
                FontSizePrefs.FORCE_ENABLE_ZOOM_THRESHOLD_MULTIPLIER - 0.4f;
        float fontBiggerThanThreshold =
                FontSizePrefs.FORCE_ENABLE_ZOOM_THRESHOLD_MULTIPLIER + 0.4f;

        // Set the textScaleFactor above the threshold.
        userSetTextScale(accessibilityPref, textScalePref, fontBiggerThanThreshold);
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        // Since above the threshold, this will check the force enable zoom button.
        Assert.assertEquals(
                percentFormat.format(fontBiggerThanThreshold), textScalePref.getSummary());
        Assert.assertTrue(forceEnableZoomPref.isChecked());
        assertFontSizePrefs(true, fontBiggerThanThreshold);

        // Set the textScaleFactor below the threshold.
        userSetTextScale(accessibilityPref, textScalePref, fontSmallerThanThreshold);
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        // Since below the threshold and userSetForceEnableZoom is false, this will uncheck
        // the force enable zoom button.
        Assert.assertEquals(
                percentFormat.format(fontSmallerThanThreshold), textScalePref.getSummary());
        Assert.assertFalse(forceEnableZoomPref.isChecked());
        assertFontSizePrefs(false, fontSmallerThanThreshold);

        userSetTextScale(accessibilityPref, textScalePref, fontBiggerThanThreshold);
        // Sets onUserSetForceEnableZoom to be true.
        userSetForceEnableZoom(accessibilityPref, forceEnableZoomPref, true);
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        // Since userSetForceEnableZoom is true, when the text scale is moved below the threshold
        // ForceEnableZoom should remain checked.
        userSetTextScale(accessibilityPref, textScalePref, fontSmallerThanThreshold);
        Assert.assertTrue(forceEnableZoomPref.isChecked());
        assertFontSizePrefs(true, fontSmallerThanThreshold);
    }

    private void assertFontSizePrefs(final boolean expectedForceEnableZoom,
            final float expectedFontScale) {
        final Context targetContext =
                InstrumentationRegistry.getInstrumentation().getTargetContext();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                FontSizePrefs fontSizePrefs = FontSizePrefs.getInstance(targetContext);
                Assert.assertEquals(expectedForceEnableZoom, fontSizePrefs.getForceEnableZoom());
                Assert.assertEquals(expectedFontScale, fontSizePrefs.getFontScaleFactor(), 0.001f);
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
