// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import android.content.Intent;
import android.os.Bundle;
import android.os.Environment;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.preferences.ChromeBaseCheckBoxPreference;
import org.chromium.chrome.browser.preferences.ChromeBaseListPreference;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreference;
import org.chromium.chrome.browser.preferences.LocationSettings;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.PreferencesTest;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.Callable;

/**
 * Tests for everything under Settings > Site Settings.
 */
public class SiteSettingsPreferencesTest extends ChromeActivityTestCaseBase<ChromeActivity> {

    private EmbeddedTestServer mTestServer;

    public SiteSettingsPreferencesTest() {
        super(ChromeActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTestServer = EmbeddedTestServer.createAndStartFileServer(
                getInstrumentation().getContext(), Environment.getExternalStorageDirectory());
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    private void setAllowLocation(final boolean enabled) {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        final Preferences preferenceActivity =
                startSiteSettingsCategory(SiteSettingsPreferences.LOCATION_KEY);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                SingleCategoryPreferences websitePreferences = (SingleCategoryPreferences)
                        preferenceActivity.getFragmentForTest();
                ChromeSwitchPreference location = (ChromeSwitchPreference)
                        websitePreferences.findPreference(
                                SingleCategoryPreferences.READ_WRITE_TOGGLE_KEY);

                websitePreferences.onPreferenceChange(location, enabled);
                assertEquals("Location should be " + (enabled ? "allowed" : "blocked"), enabled,
                        LocationSettings.getInstance().areAllLocationSettingsEnabled());
                preferenceActivity.finish();
            }
        });
    }

    private InfoBarTestAnimationListener setInfoBarAnimationListener() {
        return ThreadUtils.runOnUiThreadBlockingNoException(
                new Callable<InfoBarTestAnimationListener>() {
                    @Override
                    public InfoBarTestAnimationListener call() throws Exception {
                        InfoBarContainer container =
                                getActivity().getActivityTab().getInfoBarContainer();
                        InfoBarTestAnimationListener listener =  new InfoBarTestAnimationListener();
                        container.setAnimationListener(listener);
                        return listener;
                    }
                });
    }

    /**
     * Sets Allow Location Enabled to be true and make sure it is set correctly.
     */
    @SmallTest
    @Feature({"Preferences"})
    @FlakyTest(message = "https://crbug.com/609226")
    public void testSetAllowLocationEnabled() throws Exception {
        setAllowLocation(true);
        InfoBarTestAnimationListener listener = setInfoBarAnimationListener();

        // Launch a page that uses geolocation and make sure an infobar shows up.
        loadUrl(mTestServer.getURL(
                "/chrome/test/data/geolocation/geolocation_on_load.html"));
        assertTrue("InfoBar not added.", listener.addInfoBarAnimationFinished());

        assertEquals("Wrong infobar count", 1, getInfoBars().size());
    }

    /**
     * Sets Allow Location Enabled to be false and make sure it is set correctly.
     */
    @SmallTest
    @Feature({"Preferences"})
    @FlakyTest(message = "https://crbug.com/609226")
    public void testSetAllowLocationNotEnabled() throws Exception {
        setAllowLocation(false);

        // Launch a page that uses geolocation.
        loadUrl(mTestServer.getURL(
                "/chrome/test/data/geolocation/geolocation_on_load.html"));

        // No infobars are expected.
        assertTrue(getInfoBars().isEmpty());
    }

    private Preferences startSiteSettingsCategory(String category) {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putString(SingleCategoryPreferences.EXTRA_CATEGORY, category);
        Intent intent = PreferencesLauncher.createIntentForSettingsPage(
                getInstrumentation().getTargetContext(), SingleCategoryPreferences.class.getName());
        intent.putExtra(Preferences.EXTRA_SHOW_FRAGMENT_ARGUMENTS, fragmentArgs);
        return (Preferences) getInstrumentation().startActivitySync(intent);
    }

    private Preferences startSingleWebsitePreferences(Website site) {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putSerializable(SingleWebsitePreferences.EXTRA_SITE, site);
        Intent intent = PreferencesLauncher.createIntentForSettingsPage(
                getInstrumentation().getTargetContext(), SingleWebsitePreferences.class.getName());
        intent.putExtra(Preferences.EXTRA_SHOW_FRAGMENT_ARGUMENTS, fragmentArgs);
        return (Preferences) getInstrumentation().startActivitySync(intent);
    }

    private void setCookiesEnabled(final Preferences preferenceActivity, final boolean enabled) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                final SingleCategoryPreferences websitePreferences =
                        (SingleCategoryPreferences) preferenceActivity.getFragmentForTest();
                final ChromeSwitchPreference cookies =
                        (ChromeSwitchPreference) websitePreferences.findPreference(
                                SingleCategoryPreferences.READ_WRITE_TOGGLE_KEY);
                final ChromeBaseCheckBoxPreference thirdPartyCookies =
                        (ChromeBaseCheckBoxPreference) websitePreferences.findPreference(
                                SingleCategoryPreferences.THIRD_PARTY_COOKIES_TOGGLE_KEY);

                assertEquals("Third-party cookie toggle should be "
                        + (doesAcceptCookies() ? "enabled" : " disabled"),
                        doesAcceptCookies(), thirdPartyCookies.isEnabled());
                websitePreferences.onPreferenceChange(cookies, enabled);
                assertEquals("Cookies should be " + (enabled ? "allowed" : "blocked"),
                        doesAcceptCookies(), enabled);
            }

            private boolean doesAcceptCookies() {
                return PrefServiceBridge.getInstance().isAcceptCookiesEnabled();
            }
        });
    }

    private void setThirdPartyCookiesEnabled(final Preferences preferenceActivity,
            final boolean enabled) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                final SingleCategoryPreferences websitePreferences =
                        (SingleCategoryPreferences) preferenceActivity.getFragmentForTest();
                final ChromeBaseCheckBoxPreference thirdPartyCookies =
                        (ChromeBaseCheckBoxPreference) websitePreferences.findPreference(
                                SingleCategoryPreferences.THIRD_PARTY_COOKIES_TOGGLE_KEY);

                websitePreferences.onPreferenceChange(thirdPartyCookies, enabled);
                assertEquals("Third-party cookies should be " + (enabled ? "allowed" : "blocked"),
                        !PrefServiceBridge.getInstance().isBlockThirdPartyCookiesEnabled(),
                        enabled);
            }
        });
    }

    private void setEnablePopups(final boolean enabled) {
        final Preferences preferenceActivity =
                startSiteSettingsCategory(SiteSettingsPreferences.POPUPS_KEY);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                SingleCategoryPreferences websitePreferences =
                        (SingleCategoryPreferences) preferenceActivity.getFragmentForTest();
                ChromeSwitchPreference popups = (ChromeSwitchPreference)
                        websitePreferences.findPreference(
                                SingleCategoryPreferences.READ_WRITE_TOGGLE_KEY);
                websitePreferences.onPreferenceChange(popups, enabled);
                assertEquals("Popups should be " + (enabled ? "allowed" : "blocked"), enabled,
                        PrefServiceBridge.getInstance().popupsEnabled());
            }
        });
        preferenceActivity.finish();
    }

    private void setEnableCamera(final boolean enabled) {
        final Preferences preferenceActivity =
                startSiteSettingsCategory(SiteSettingsPreferences.CAMERA_KEY);

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                SingleCategoryPreferences websitePreferences =
                        (SingleCategoryPreferences) preferenceActivity.getFragmentForTest();
                ChromeSwitchPreference toggle = (ChromeSwitchPreference)
                        websitePreferences.findPreference(
                                SingleCategoryPreferences.READ_WRITE_TOGGLE_KEY);
                websitePreferences.onPreferenceChange(toggle, enabled);
                assertEquals("Camera should be " + (enabled ? "allowed" : "blocked"),
                        enabled, PrefServiceBridge.getInstance().isCameraEnabled());
            }
        });
        preferenceActivity.finish();
    }

    private void setEnableMic(final boolean enabled) {
        final Preferences preferenceActivity =
                startSiteSettingsCategory(SiteSettingsPreferences.MICROPHONE_KEY);

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                SingleCategoryPreferences websitePreferences =
                        (SingleCategoryPreferences) preferenceActivity.getFragmentForTest();
                ChromeSwitchPreference toggle = (ChromeSwitchPreference)
                        websitePreferences.findPreference(
                                SingleCategoryPreferences.READ_WRITE_TOGGLE_KEY);
                websitePreferences.onPreferenceChange(toggle, enabled);
                assertEquals("Mic should be " + (enabled ? "allowed" : "blocked"),
                        enabled, PrefServiceBridge.getInstance().isMicEnabled());
            }
        });
        preferenceActivity.finish();
    }

    private void setAutoDetectEncoding(final boolean enabled) {
        Intent intent = PreferencesLauncher.createIntentForSettingsPage(
                getInstrumentation().getTargetContext(), LanguagePreferences.class.getName());
        final Preferences preferenceActivity =
                (Preferences) getInstrumentation().startActivitySync(intent);

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                LanguagePreferences languagePreferences =
                        (LanguagePreferences) preferenceActivity.getFragmentForTest();
                ChromeBaseCheckBoxPreference checkbox = (ChromeBaseCheckBoxPreference)
                        languagePreferences.findPreference(
                                LanguagePreferences.PREF_AUTO_DETECT_CHECKBOX);
                if (checkbox.isChecked() != enabled) {
                    PreferencesTest.clickPreference(languagePreferences, checkbox);
                }
                assertEquals("Auto detect encoding should be " + (enabled ? "enabled" : "disabled"),
                        enabled, PrefServiceBridge.getInstance().isAutoDetectEncodingEnabled());
            }
        });

        preferenceActivity.finish();
    }

    private void setEnableKeygen(final String origin, final boolean enabled) {
        Website website = new Website(WebsiteAddress.create(origin));
        website.setKeygenInfo(new KeygenInfo(origin, origin, false));
        final Preferences preferenceActivity = startSingleWebsitePreferences(website);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                SingleWebsitePreferences websitePreferences =
                        (SingleWebsitePreferences) preferenceActivity.getFragmentForTest();
                ChromeBaseListPreference keygen =
                        (ChromeBaseListPreference) websitePreferences.findPreference(
                                SingleWebsitePreferences.PREF_KEYGEN_PERMISSION);
                websitePreferences.onPreferenceChange(keygen, enabled
                                ? ContentSetting.ALLOW.toString()
                                : ContentSetting.BLOCK.toString());
            }
        });
        preferenceActivity.finish();
    }

    private void setEnableBackgroundSync(final boolean enabled) {
        final Preferences preferenceActivity =
                startSiteSettingsCategory(SiteSettingsCategory.CATEGORY_BACKGROUND_SYNC);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                SingleCategoryPreferences backgroundSyncPreferences =
                        (SingleCategoryPreferences) preferenceActivity.getFragmentForTest();
                ChromeSwitchPreference toggle =
                        (ChromeSwitchPreference) backgroundSyncPreferences.findPreference(
                                SingleCategoryPreferences.READ_WRITE_TOGGLE_KEY);
                backgroundSyncPreferences.onPreferenceChange(toggle, enabled);
            }
        });
    }

    // TODO(finnur): Write test for Autoplay.

    /**
     * Tests that disabling cookies turns off the third-party cookie toggle.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Preferences"})
    public void testThirdPartyCookieToggleGetsDisabled() throws Exception {
        Preferences preferenceActivity =
                startSiteSettingsCategory(SiteSettingsPreferences.COOKIES_KEY);
        setCookiesEnabled(preferenceActivity, true);
        setThirdPartyCookiesEnabled(preferenceActivity, false);
        setThirdPartyCookiesEnabled(preferenceActivity, true);
        setCookiesEnabled(preferenceActivity, false);
        preferenceActivity.finish();
    }

    /**
     * Allows cookies to be set and ensures that they are.
     */
    @SmallTest
    @Feature({"Preferences"})
    public void testCookiesNotBlocked() throws Exception {
        Preferences preferenceActivity =
                startSiteSettingsCategory(SiteSettingsPreferences.COOKIES_KEY);
        setCookiesEnabled(preferenceActivity, true);
        preferenceActivity.finish();

        final String url = mTestServer.getURL("/chrome/test/data/android/cookie.html");

        // Load the page and clear any set cookies.
        loadUrl(url + "#clear");
        assertEquals("\"\"", runJavaScriptCodeInCurrentTab("getCookie()"));
        runJavaScriptCodeInCurrentTab("setCookie()");
        assertEquals("\"Foo=Bar\"", runJavaScriptCodeInCurrentTab("getCookie()"));

        // Load the page again and ensure the cookie still is set.
        loadUrl(url);
        assertEquals("\"Foo=Bar\"", runJavaScriptCodeInCurrentTab("getCookie()"));
    }

    /**
     * Blocks cookies from being set and ensures that no cookies can be set.
     */
    @SmallTest
    @Feature({"Preferences"})
    public void testCookiesBlocked() throws Exception {
        Preferences preferenceActivity =
                startSiteSettingsCategory(SiteSettingsPreferences.COOKIES_KEY);
        setCookiesEnabled(preferenceActivity, false);
        preferenceActivity.finish();

        final String url = mTestServer.getURL("/chrome/test/data/android/cookie.html");

        // Load the page and clear any set cookies.
        loadUrl(url + "#clear");
        assertEquals("\"\"", runJavaScriptCodeInCurrentTab("getCookie()"));
        runJavaScriptCodeInCurrentTab("setCookie()");
        assertEquals("\"\"", runJavaScriptCodeInCurrentTab("getCookie()"));

        // Load the page again and ensure the cookie remains unset.
        loadUrl(url);
        assertEquals("\"\"", runJavaScriptCodeInCurrentTab("getCookie()"));
    }

    /**
     * Sets Allow Popups Enabled to be false and make sure it is set correctly.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Preferences"})
    public void testPopupsBlocked() throws Exception {
        setEnablePopups(false);

        // Test that the popup doesn't open.
        loadUrl(mTestServer.getURL("/chrome/test/data/android/popup.html"));

        getInstrumentation().waitForIdleSync();
        assertEquals(1, getTabCount());
    }

    /**
     * Sets Allow Popups Enabled to be true and make sure it is set correctly.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Preferences"})
    public void testPopupsNotBlocked() throws Exception {
        setEnablePopups(true);

        // Test that a popup opens.
        loadUrl(mTestServer.getURL("/chrome/test/data/android/popup.html"));
        getInstrumentation().waitForIdleSync();

        assertEquals(2, getTabCount());
    }

    /**
     * Sets Allow Keygen Enabled to be false and make sure it is set correctly.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Preferences"})
    public void testKeygenBlocked() throws Exception {
        final String origin = "http://example.com/";
        setEnableKeygen(origin, false);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Website site = new Website(WebsiteAddress.create(origin));
                site.setKeygenInfo(new KeygenInfo(origin, origin, false));
                assertEquals(site.getKeygenPermission(), ContentSetting.BLOCK);
            }
        });
    }

    /**
     * Sets Allow Keygen Enabled to be true and make sure it is set correctly.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Preferences"})
    public void testKeygenNotBlocked() throws Exception {
        final String origin = "http://example.com/";
        setEnableKeygen(origin, true);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Website site = new Website(WebsiteAddress.create(origin));
                site.setKeygenInfo(new KeygenInfo(origin, origin, false));
                assertEquals(site.getKeygenPermission(), ContentSetting.ALLOW);
            }
        });
    }

    /**
     * Tests Reset Site not crashing on host names (issue 600232).
     * @throws Exception
     */
    @SmallTest
    @Feature({"Preferences"})
    public void testResetCrash600232() throws Exception {
        Website website = new Website(WebsiteAddress.create("example.com"));
        final Preferences preferenceActivity = startSingleWebsitePreferences(website);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                SingleWebsitePreferences websitePreferences =
                        (SingleWebsitePreferences) preferenceActivity.getFragmentForTest();
                websitePreferences.resetSite();
            }
        });
        preferenceActivity.finish();
    }

    /**
     * Sets Allow Camera Enabled to be false and make sure it is set correctly.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Preferences"})
    @CommandLineFlags.Add(ChromeSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM)
    public void testCameraBlocked() throws Exception {
        setEnableCamera(false);

        // Test that the camera permission doesn't get requested.
        loadUrl(mTestServer.getURL("/content/test/data/media/getusermedia.html"));
        runJavaScriptCodeInCurrentTab("getUserMediaAndStop({video: true, audio: false});");

        // No infobars are expected.
        assertTrue(getInfoBars().isEmpty());
    }

    /**
     * Sets Allow Mic Enabled to be false and make sure it is set correctly.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Preferences"})
    @CommandLineFlags.Add(ChromeSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM)
    @FlakyTest(message = "https://crbug.com/609226")
    public void testMicBlocked() throws Exception {
        setEnableMic(false);

        // Test that the microphone permission doesn't get requested.
        loadUrl(mTestServer.getURL("/content/test/data/media/getusermedia.html"));
        runJavaScriptCodeInCurrentTab("getUserMediaAndStop({video: false, audio: true});");

        // No infobars are expected.
        assertTrue(getInfoBars().isEmpty());
    }

    /**
     * Sets Allow Camera Enabled to be true and make sure it is set correctly.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Preferences"})
    @CommandLineFlags.Add(ChromeSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM)
    @FlakyTest(message = "https://crbug.com/609226")
    public void testCameraNotBlocked() throws Exception {
        setEnableCamera(true);

        InfoBarTestAnimationListener listener = setInfoBarAnimationListener();

        // Launch a page that uses camera and make sure an infobar shows up.
        loadUrl(mTestServer.getURL("/content/test/data/media/getusermedia.html"));
        runJavaScriptCodeInCurrentTab("getUserMediaAndStop({video: true, audio: false});");

        assertTrue("InfoBar not added.", listener.addInfoBarAnimationFinished());
        assertEquals("Wrong infobar count", 1, getInfoBars().size());
    }

    /**
     * Sets Allow Mic Enabled to be true and make sure it is set correctly.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Preferences"})
    @CommandLineFlags.Add(ChromeSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM)
    @FlakyTest(message = "https://crbug.com/609226")
    public void testMicNotBlocked() throws Exception {
        setEnableCamera(true);

        InfoBarTestAnimationListener listener = setInfoBarAnimationListener();

        // Launch a page that uses the microphone and make sure an infobar shows up.
        loadUrl(mTestServer.getURL("/content/test/data/media/getusermedia.html"));
        runJavaScriptCodeInCurrentTab("getUserMediaAndStop({video: false, audio: true});");

        assertTrue("InfoBar not added.", listener.addInfoBarAnimationFinished());
        assertEquals("Wrong infobar count", 1, getInfoBars().size());
    }

    /**
     * Toggles auto detect encoding, makes sure it is set correctly, and makes sure the page is
     * encoded correctly.
     */
    @SmallTest
    @Feature({"Preferences"})
    public void testToggleAutoDetectEncoding() throws Exception {
        String testUrl = mTestServer.getURL(
                "/chrome/test/data/encoding_tests/auto_detect/"
                + "Big5_with_no_encoding_specified.html");

        setAutoDetectEncoding(false);
        loadUrl(testUrl);
        assertEquals("Wrong page encoding while auto detect encoding disabled", "windows-1252",
                getActivity().getCurrentContentViewCore().getWebContents().getEncoding());

        setAutoDetectEncoding(true);
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                getActivity().getActivityTab().reload();
            }
        });
        ChromeTabUtils.waitForTabPageLoaded(getActivity().getActivityTab(), testUrl);
        assertEquals("Wrong page encoding while auto detect encoding enabled", "Big5",
                getActivity().getCurrentContentViewCore().getWebContents().getEncoding());

    }

    /**
     * Helper function to test allowing and blocking background sync.
     * @param enabled true to test enabling background sync, false to test disabling the feature.
     */
    private void doTestBackgroundSyncPermission(final boolean enabled) {
        setEnableBackgroundSync(enabled);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                assertEquals("Background Sync should be " + (enabled ? "enabled" : "disabled"),
                        PrefServiceBridge.getInstance().isBackgroundSyncAllowed(), enabled);
            }
        });
    }

    @SmallTest
    @Feature({"Preferences"})
    public void testAllowBackgroundSync() {
        doTestBackgroundSyncPermission(true);
    }

    @SmallTest
    @Feature({"Preferences"})
    public void testBlockBackgroundSync() {
        doTestBackgroundSyncPermission(false);
    }

    private int getTabCount() {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Integer>() {
            @Override
            public Integer call() throws Exception {
                if (FeatureUtilities.isDocumentMode(getInstrumentation().getTargetContext())) {
                    return ChromeApplication.getDocumentTabModelSelector().getTotalTabCount();
                } else {
                    return getActivity().getTabModelSelector().getTotalTabCount();
                }
            }
        });
    }
}
