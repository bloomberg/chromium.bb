// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.doesNotExist;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.core.StringContains.containsString;
import static org.junit.Assert.assertNotNull;

import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.Espresso;
import android.support.test.espresso.NoMatchingViewException;
import android.support.test.filters.LargeTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabIncognitoManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.IncognitoDataTestUtils.ActivityType;
import org.chromium.chrome.browser.incognito.IncognitoDataTestUtils.TestParams;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.device.geolocation.LocationProviderOverrider;
import org.chromium.device.geolocation.MockLocationProvider;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * This test class checks permission leakage between all different
 * pairs of Activity types with a constraint that one of the
 * interacting activity must be either Incognito Tab or Incognito CCT.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@EnableFeatures({ChromeFeatureList.CCT_INCOGNITO})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class IncognitoPermissionLeakageTest {
    private String mPermissionTestPage;
    private EmbeddedTestServer mTestServer;

    private static final String PERMISSION_HTML_PATH =
            "/content/test/data/android/geolocation.html";

    @Rule
    public ChromeActivityTestRule<ChromeTabbedActivity> mChromeActivityTestRule =
            new ChromeActivityTestRule<>(ChromeTabbedActivity.class);

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Before
    public void setUp() {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mPermissionTestPage = mTestServer.getURL(PERMISSION_HTML_PATH);
        mChromeActivityTestRule.startMainActivityOnBlankPage();

        // Permission related settings.
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        LocationProviderOverrider.setLocationProviderImpl(new MockLocationProvider());
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> IncognitoDataTestUtils.closeTabs(mChromeActivityTestRule));
        mTestServer.stopAndDestroyServer();
    }

    private void requestLocationPermission(Tab tab) throws TimeoutException {
        CriteriaHelper.pollUiThread(() -> { assertNotNull(tab.getWebContents()); });
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                tab.getWebContents(), "initiate_getCurrentPosition()");
    }

    private void assertDialogIsShown() throws NoMatchingViewException {
        Espresso.onView(withId(R.id.text)).check(matches(withText(containsString("location"))));
    }

    private void assertDialogIsNotShown() throws NoMatchingViewException {
        Espresso.onView(withId(R.id.text)).check(doesNotExist());
    }

    private void grantPermission() {
        Espresso.onView(withText(containsString("Allow"))).perform(click());
    }

    private void blockPermission() {
        Espresso.onView(withText(containsString("Block"))).perform(click());
    }

    public static class RegularAndIncognito implements ParameterProvider {
        @Override
        public List<ParameterSet> getParameters() {
            List<ParameterSet> result = new ArrayList<>();
            result.addAll(new TestParams.IncognitoToRegular().getParameters());
            result.addAll(new TestParams.RegularToIncognito().getParameters());
            return result;
        }
    }

    @Test
    @LargeTest
    @UseMethodParameter(RegularAndIncognito.class)
    public void testAllowPermissionDoNotLeakBetweenRegularAndIncognito(
            String activityType1, String activityType2) throws Exception {
        ActivityType activity1 = ActivityType.valueOf(activityType1);
        ActivityType activity2 = ActivityType.valueOf(activityType2);

        Tab tab1 = activity1.launchUrl(
                mChromeActivityTestRule, mCustomTabActivityTestRule, mPermissionTestPage);

        // Request permission in activity1's tab and accept it.
        requestLocationPermission(tab1);
        assertDialogIsShown();
        grantPermission();

        Tab tab2 = activity2.launchUrl(
                mChromeActivityTestRule, mCustomTabActivityTestRule, mPermissionTestPage);

        // Request permission in activity2's tab.
        requestLocationPermission(tab2);
        // Permission is asked again.
        assertDialogIsShown();
    }

    @Test
    @LargeTest
    @UseMethodParameter(TestParams.IncognitoToIncognito.class)
    public void testAllowPermissionDoNotLeakFromIncognitoToIncognito(
            String incognitoActivityType1, String incognitoActivityType2) throws Exception {
        // At least one of the incognitoActivity is an incognito CCT.
        ActivityType incognitoActivity1 = ActivityType.valueOf(incognitoActivityType1);
        ActivityType incognitoActivity2 = ActivityType.valueOf(incognitoActivityType2);

        Tab tab1 = incognitoActivity1.launchUrl(
                mChromeActivityTestRule, mCustomTabActivityTestRule, mPermissionTestPage);

        // Request permission in incognitoActivity1's tab and accept it.
        requestLocationPermission(tab1);
        assertDialogIsShown();
        grantPermission();

        Tab tab2 = incognitoActivity2.launchUrl(
                mChromeActivityTestRule, mCustomTabActivityTestRule, mPermissionTestPage);

        // Request permission in incognitoActivity2's tab.
        requestLocationPermission(tab2);

        // Incognito CCTs with isolated profiles should not inherit permissions from other sessions.
        if (CustomTabIncognitoManager.hasIsolatedProfile()) {
            // Permission is asked again, therefore the previous permission wasn't inherited.
            assertDialogIsShown();
        } else {
            assertDialogIsNotShown();
        }
    }

    @Test
    @LargeTest
    @UseMethodParameter(TestParams.IncognitoToIncognito.class)
    public void testBlockPermissionDoNotLeakFromIncognitoToIncognito(
            String incognitoActivityType1, String incognitoActivityType2) throws Exception {
        ActivityType incognitoActivity1 = ActivityType.valueOf(incognitoActivityType1);
        ActivityType incognitoActivity2 = ActivityType.valueOf(incognitoActivityType2);

        Tab tab1 = incognitoActivity1.launchUrl(
                mChromeActivityTestRule, mCustomTabActivityTestRule, mPermissionTestPage);

        // Request permission in incognitoActivity1's tab and block it.
        requestLocationPermission(tab1);
        assertDialogIsShown();
        blockPermission();

        Tab tab2 = incognitoActivity2.launchUrl(
                mChromeActivityTestRule, mCustomTabActivityTestRule, mPermissionTestPage);

        // Request permission now in incognitoActivity2's tab.
        requestLocationPermission(tab2);

        // Incognito CCTs with isolated profiles should not inherit permissions from other sessions.
        if (CustomTabIncognitoManager.hasIsolatedProfile()) {
            // Permission is asked again, therefore the previous permission wasn't inherited.
            assertDialogIsShown();
        } else {
            assertDialogIsNotShown();
        }
    }

    @Test
    @LargeTest
    @UseMethodParameter(TestParams.RegularToIncognito.class)
    public void testBlockPermissionLeakFromRegularToIncognito(
            String regularActivityType, String incognitoActivityType) throws Exception {
        ActivityType regularActivity = ActivityType.valueOf(regularActivityType);
        ActivityType incognitoActivity = ActivityType.valueOf(incognitoActivityType);

        Tab tab1 = regularActivity.launchUrl(
                mChromeActivityTestRule, mCustomTabActivityTestRule, mPermissionTestPage);

        // Request permission in regularActivity's tab and block it.
        requestLocationPermission(tab1);
        assertDialogIsShown();
        blockPermission();

        Tab tab2 = incognitoActivity.launchUrl(
                mChromeActivityTestRule, mCustomTabActivityTestRule, mPermissionTestPage);

        // Request permission in incognitoActivity's tab.
        requestLocationPermission(tab2);

        // Dialog is not shown again to the new tab as the permission was blocked for this site in
        // regular.
        assertDialogIsNotShown();
    }

    @Test
    @LargeTest
    @UseMethodParameter(TestParams.IncognitoToRegular.class)
    public void testBlockPermissionDoNotLeakFromIncognitoToRegular(
            String incognitoActivityType, String regularActivityType) throws Exception {
        ActivityType incognitoActivity = ActivityType.valueOf(incognitoActivityType);
        ActivityType regularActivity = ActivityType.valueOf(regularActivityType);

        Tab tab1 = incognitoActivity.launchUrl(
                mChromeActivityTestRule, mCustomTabActivityTestRule, mPermissionTestPage);

        // Request permission in incognitoActivity's tab and accept it.
        requestLocationPermission(tab1);

        assertDialogIsShown();
        blockPermission();

        Tab tab2 = regularActivity.launchUrl(
                mChromeActivityTestRule, mCustomTabActivityTestRule, mPermissionTestPage);

        // Request permission in regularActivity's tab.
        requestLocationPermission(tab2);

        // Dialog is shown again in tab2 and the permission is therefore not leaked.
        assertDialogIsShown();
    }
}