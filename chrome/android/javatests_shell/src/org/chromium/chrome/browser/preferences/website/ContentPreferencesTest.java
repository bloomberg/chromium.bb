// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import android.content.Intent;
import android.os.Bundle;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.location.LocationSettingsTestUtil;
import org.chromium.chrome.browser.preferences.ChromeBaseCheckBoxPreference;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreference;
import org.chromium.chrome.browser.preferences.LocationSettings;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.shell.ChromeShellTestBase;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;
import org.chromium.chrome.test.util.TestHttpServerClient;
import org.chromium.content.browser.test.util.JavaScriptUtils;

import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;

/**
 * Tests for everything under Settings > Site Settings.
 */
public class ContentPreferencesTest extends ChromeShellTestBase {

    private void setAllowLocation(final boolean enabled) {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        final Preferences preferenceActivity =
                startContentSettingsCategory(ContentPreferences.LOCATION_KEY);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                WebsitePreferences websitePreferences = (WebsitePreferences)
                        preferenceActivity.getFragmentForTest();
                ChromeSwitchPreference location = (ChromeSwitchPreference)
                        websitePreferences.findPreference(WebsitePreferences.READ_WRITE_TOGGLE_KEY);

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
                                getActivity().getActiveTab().getInfoBarContainer();
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
    public void testSetAllowLocationEnabled() throws Exception {
        setAllowLocation(true);
        InfoBarTestAnimationListener listener = setInfoBarAnimationListener();

        // Launch a page that uses geolocation and make sure an infobar shows up.
        loadUrl(TestHttpServerClient.getUrl(
                "chrome/test/data/geolocation/geolocation_on_load.html"));
        assertTrue("InfoBar not added.", listener.addInfoBarAnimationFinished());

        assertEquals("Wrong infobar count", 1, getInfoBars().size());
    }

    /**
     * Sets Allow Location Enabled to be false and make sure it is set correctly.
     */
    @SmallTest
    @Feature({"Preferences"})
    public void testSetAllowLocationNotEnabled() throws Exception {
        setAllowLocation(false);

        // Launch a page that uses geolocation.
        loadUrl(TestHttpServerClient.getUrl(
                "chrome/test/data/geolocation/geolocation_on_load.html"));

        // No infobars are expected.
        assertTrue(getInfoBars().isEmpty());
    }

    private Preferences startContentSettingsCategory(
            String categoryKey) {
        // Launch main activity for initial ContentPreferences initialization.
        launchChromeShellWithBlankPage();

        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putString(WebsitePreferences.EXTRA_CATEGORY, categoryKey);
        Intent intent = PreferencesLauncher.createIntentForSettingsPage(
                getInstrumentation().getTargetContext(), WebsitePreferences.class.getName());
        intent.putExtra(Preferences.EXTRA_SHOW_FRAGMENT_ARGUMENTS, fragmentArgs);
        return (Preferences) getInstrumentation().startActivitySync(intent);
    }

    private void setCookiesEnabled(final Preferences preferenceActivity, final boolean enabled) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                final WebsitePreferences websitePreferences =
                        (WebsitePreferences) preferenceActivity.getFragmentForTest();
                final ChromeSwitchPreference cookies =
                        (ChromeSwitchPreference) websitePreferences.findPreference(
                                WebsitePreferences.READ_WRITE_TOGGLE_KEY);
                final ChromeBaseCheckBoxPreference thirdPartyCookies =
                        (ChromeBaseCheckBoxPreference) websitePreferences.findPreference(
                                WebsitePreferences.THIRD_PARTY_COOKIES_TOGGLE_KEY);

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
                final WebsitePreferences websitePreferences =
                        (WebsitePreferences) preferenceActivity.getFragmentForTest();
                final ChromeBaseCheckBoxPreference thirdPartyCookies =
                        (ChromeBaseCheckBoxPreference) websitePreferences.findPreference(
                                WebsitePreferences.THIRD_PARTY_COOKIES_TOGGLE_KEY);

                websitePreferences.onPreferenceChange(thirdPartyCookies, enabled);
                assertEquals("Third-party cookies should be " + (enabled ? "allowed" : "blocked"),
                        !PrefServiceBridge.getInstance().isBlockThirdPartyCookiesEnabled(),
                        enabled);
            }
        });
    }

    private void setEnableImages(final boolean enabled) {
        final Preferences preferenceActivity =
                startContentSettingsCategory(ContentPreferences.IMAGES_KEY);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                final WebsitePreferences websitePreferences =
                        (WebsitePreferences) preferenceActivity.getFragmentForTest();
                ChromeSwitchPreference images = (ChromeSwitchPreference)
                        websitePreferences.findPreference(WebsitePreferences.READ_WRITE_TOGGLE_KEY);
                websitePreferences.onPreferenceChange(images, enabled);
                assertEquals("Images should be " + (enabled ? "allowed" : "blocked"), enabled,
                        PrefServiceBridge.getInstance().imagesEnabled());
            }
        });
        preferenceActivity.finish();
    }

    private void setEnablePopups(final boolean enabled) {
        final Preferences preferenceActivity =
                startContentSettingsCategory(ContentPreferences.POPUPS_KEY);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                WebsitePreferences websitePreferences =
                        (WebsitePreferences) preferenceActivity.getFragmentForTest();
                ChromeSwitchPreference popups = (ChromeSwitchPreference)
                        websitePreferences.findPreference(WebsitePreferences.READ_WRITE_TOGGLE_KEY);
                websitePreferences.onPreferenceChange(popups, enabled);
                assertEquals("Popups should be " + (enabled ? "allowed" : "blocked"), enabled,
                        PrefServiceBridge.getInstance().popupsEnabled());
            }
        });
        preferenceActivity.finish();
    }

    private void setEnableCamera(final boolean enabled) {
        final Preferences preferenceActivity =
                startContentSettingsCategory(ContentPreferences.CAMERA_KEY);

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                WebsitePreferences websitePreferences =
                        (WebsitePreferences) preferenceActivity.getFragmentForTest();
                ChromeSwitchPreference toggle = (ChromeSwitchPreference)
                        websitePreferences.findPreference(WebsitePreferences.READ_WRITE_TOGGLE_KEY);
                websitePreferences.onPreferenceChange(toggle, enabled);
                assertEquals("Camera should be " + (enabled ? "allowed" : "blocked"),
                        enabled, PrefServiceBridge.getInstance().isCameraEnabled());
            }
        });
        preferenceActivity.finish();
    }

    private void setEnableMic(final boolean enabled) {
        final Preferences preferenceActivity =
                startContentSettingsCategory(ContentPreferences.MICROPHONE_KEY);

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                WebsitePreferences websitePreferences =
                        (WebsitePreferences) preferenceActivity.getFragmentForTest();
                ChromeSwitchPreference toggle = (ChromeSwitchPreference)
                        websitePreferences.findPreference(WebsitePreferences.READ_WRITE_TOGGLE_KEY);
                websitePreferences.onPreferenceChange(toggle, enabled);
                assertEquals("Mic should be " + (enabled ? "allowed" : "blocked"),
                        enabled, PrefServiceBridge.getInstance().isMicEnabled());
            }
        });
        preferenceActivity.finish();
    }
    /**
     * Tests that disabling cookies turns off the third-party cookie toggle.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Preferences"})
    public void testThirdPartyCookieToggleGetsDisabled() throws Exception {
        Preferences preferenceActivity =
                startContentSettingsCategory(ContentPreferences.COOKIES_KEY);
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
                startContentSettingsCategory(ContentPreferences.COOKIES_KEY);
        setCookiesEnabled(preferenceActivity, true);
        preferenceActivity.finish();

        final String url = TestHttpServerClient.getUrl("chrome/test/data/android/cookie.html");

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
                startContentSettingsCategory(ContentPreferences.COOKIES_KEY);
        setCookiesEnabled(preferenceActivity, false);
        preferenceActivity.finish();

        final String url = TestHttpServerClient.getUrl("chrome/test/data/android/cookie.html");

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
     * Sets Images to be disabled and make sure it is set correctly.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Preferences"})
    public void testImagesBlocked() throws Exception {
        setEnableImages(false);

        // Test that images don't load.
        loadUrl(TestHttpServerClient.getUrl("chrome/test/data/android/images.html"));
        assertEquals("0", runJavaScriptCodeInCurrentTab("getImageHeight()"));
    }

    /**
     * Sets Images to be Enabled to be true and make sure it is set correctly.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Preferences"})
    public void testImagesNotBlocked() throws Exception {
        setEnableImages(true);

        // Test that images load.
        loadUrl(TestHttpServerClient.getUrl("chrome/test/data/android/images.html"));
        assertEquals("5", runJavaScriptCodeInCurrentTab("getImageHeight()"));
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
        loadUrl(TestHttpServerClient.getUrl("chrome/test/data/android/popup.html"));

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
        loadUrl(TestHttpServerClient.getUrl("chrome/test/data/android/popup.html"));
        getInstrumentation().waitForIdleSync();

        assertEquals(2, getTabCount());
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
        loadUrl(TestHttpServerClient.getUrl("content/test/data/media/getusermedia.html"));
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
    public void testMicBlocked() throws Exception {
        setEnableMic(false);

        // Test that the microphone permission doesn't get requested.
        loadUrl(TestHttpServerClient.getUrl("content/test/data/media/getusermedia.html"));
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
    public void testCameraNotBlocked() throws Exception {
        setEnableCamera(true);

        InfoBarTestAnimationListener listener = setInfoBarAnimationListener();

        // Launch a page that uses camera and make sure an infobar shows up.
        loadUrl(TestHttpServerClient.getUrl("content/test/data/media/getusermedia.html"));
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
    public void testMicNotBlocked() throws Exception {
        setEnableCamera(true);

        InfoBarTestAnimationListener listener = setInfoBarAnimationListener();

        // Launch a page that uses the microphone and make sure an infobar shows up.
        loadUrl(TestHttpServerClient.getUrl("content/test/data/media/getusermedia.html"));
        runJavaScriptCodeInCurrentTab("getUserMediaAndStop({video: false, audio: true});");

        assertTrue("InfoBar not added.", listener.addInfoBarAnimationFinished());
        assertEquals("Wrong infobar count", 1, getInfoBars().size());
    }

    private String runJavaScriptCodeInCurrentTab(String code) throws InterruptedException,
            TimeoutException {
        return JavaScriptUtils.executeJavaScriptAndWaitForResult(
                getActivity().getActiveContentViewCore().getWebContents(), code);
    }

    private List<InfoBar> getInfoBars() {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<List<InfoBar>>() {
            @Override
            public List<InfoBar> call() throws Exception {
                return getActivity().getActiveTab().getInfoBarContainer().getInfoBars();
            }
        });
    }

    private int getTabCount() {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Integer>() {
            @Override
            public Integer call() throws Exception {
                return getActivity().getTabModelSelector().getCurrentModel().getCount();
            }
        });
    }
}
