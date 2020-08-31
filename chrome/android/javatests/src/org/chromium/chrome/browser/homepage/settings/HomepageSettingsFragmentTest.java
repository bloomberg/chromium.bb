// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.homepage.settings;

import android.support.test.filters.SmallTest;
import android.view.View;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.homepage.HomepageTestRule;
import org.chromium.chrome.browser.homepage.settings.HomepageMetricsEnums.HomepageLocationType;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithEditText;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.test.util.UiRestriction;

/**
 * Test for {@link HomepageSettings}.
 *
 * This test is created to check whether the UI components and the interaction when
 * {@link ChromeFeatureList#HOMEPAGE_SETTINGS_UI_CONVERSION } is enabled. Tests for {@link
 * HomepageSettings} when feature flag is turned off can be found in {@link
 * HomepageSettingsFragmentWithEditorTest}, {@link
 * org.chromium.chrome.browser.partnercustomizations.PartnerHomepageIntegrationTest} and {@link
 * org.chromium.chrome.browser.homepage.HomepagePolicyIntegrationTest}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@Features.EnableFeatures(ChromeFeatureList.HOMEPAGE_SETTINGS_UI_CONVERSION)
@Features.DisableFeatures(ChromeFeatureList.CHROME_DUET)
public class HomepageSettingsFragmentTest {
    // clang-format on
    private static final String ASSERT_MESSAGE_SWITCH_ENABLE = "Switch should be enabled.";
    private static final String ASSERT_MESSAGE_SWITCH_DISABLE = "Switch should be disabled.";
    private static final String ASSERT_MESSAGE_RADIO_BUTTON_ENABLED =
            "RadioButton should be enabled.";
    private static final String ASSERT_MESSAGE_RADIO_BUTTON_DISABLED =
            "RadioButton should be disabled.";
    private static final String ASSERT_MESSAGE_TITLE_ENABLED =
            "Title for RadioButtonGroup should be enabled.";
    private static final String ASSERT_MESSAGE_TITLE_DISABLED =
            "Title for RadioButtonGroup should be disabled.";

    private static final String ASSERT_MESSAGE_SWITCH_CHECK =
            "Switch preference state is not consistent with test settings.";
    private static final String ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK =
            "NTP Radio button does not check the expected option in test settings.";
    private static final String ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK =
            "Customized Radio button does not check the expected option in test settings.";
    private static final String ASSERT_MESSAGE_EDIT_TEXT =
            "EditText does not contains the expected homepage in test settings.";
    private static final String ASSERT_HOMEPAGE_MANAGER_SETTINGS =
            "HomepageManager#getHomepageUri is different than test homepage settings.";

    private static final String ASSERT_MESSAGE_DUET_SWITCH_INVISIBLE =
            "Switch should not be visible when duet is enabled.";

    private static final String ASSERT_HOMEPAGE_LOCATION_HISTOGRAM_COUNT =
            "Count for user action <Settings.Homepage.LocationChanged_V2> is different.";
    private static final String ASSERT_HOMEPAGE_LOCATION_TYPE_MISMATCH =
            "HomepageLocationType is different than test settings.";

    private static final String TEST_URL_FOO = "http://127.0.0.1:8000/foo.html";
    private static final String TEST_URL_BAR = "http://127.0.0.1:8000/bar.html";
    private static final String CHROME_NTP = UrlConstants.NTP_NON_NATIVE_URL;

    @Rule
    public SettingsActivityTestRule<HomepageSettings> mTestRule =
            new SettingsActivityTestRule<>(HomepageSettings.class);

    @Rule
    public HomepageTestRule mHomepageTestRule = new HomepageTestRule();

    @Rule
    public TestRule mFeatureProcessor = new Features.InstrumentationProcessor();

    private ChromeSwitchPreference mSwitch;
    private RadioButtonGroupHomepagePreference mRadioGroupPreference;
    private TextMessagePreference mManagedText;

    private TextView mTitleTextView;
    private RadioButtonWithDescription mChromeNtpRadioButton;
    private RadioButtonWithEditText mCustomUriRadioButton;

    private void launchSettingsActivity() {
        mTestRule.startSettingsActivity();
        HomepageSettings fragment = mTestRule.getFragment();
        Assert.assertNotNull(fragment);

        mSwitch = (ChromeSwitchPreference) fragment.findPreference(
                HomepageSettings.PREF_HOMEPAGE_SWITCH);
        mRadioGroupPreference = (RadioButtonGroupHomepagePreference) fragment.findPreference(
                HomepageSettings.PREF_HOMEPAGE_RADIO_GROUP);
        mManagedText =
                (TextMessagePreference) fragment.findPreference(HomepageSettings.PREF_TEXT_MANAGED);

        Assert.assertTrue(
                "RadioGroupPreference should be visible when Homepage Conversion is enabled.",
                mRadioGroupPreference.isVisible());

        mTitleTextView = mRadioGroupPreference.getTitleTextView();
        mChromeNtpRadioButton = mRadioGroupPreference.getChromeNTPRadioButton();
        mCustomUriRadioButton = mRadioGroupPreference.getCustomUriRadioButton();

        Assert.assertNotNull("Title text view is null.", mTitleTextView);
        Assert.assertNotNull("Chrome NTP radio button is null.", mChromeNtpRadioButton);
        Assert.assertNotNull("Custom URI radio button is null.", mCustomUriRadioButton);
    }

    private void finishSettingsActivity() {
        mTestRule.getActivity().finish();
        mTestRule.waitTillActivityIsDestroyed();
    }

    @Test
    @SmallTest
    @Feature({"Homepage"})
    public void testStartUp_ChromeNTP() {
        mHomepageTestRule.useCustomizedHomepageForTest(TEST_URL_BAR);
        mHomepageTestRule.useChromeNTPForTest();

        launchSettingsActivity();

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_ENABLE, mSwitch.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mCustomUriRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_TITLE_ENABLED, mTitleTextView.isEnabled());

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(ASSERT_MESSAGE_EDIT_TEXT, TEST_URL_BAR,
                mCustomUriRadioButton.getPrimaryText().toString());
        Assert.assertEquals(ASSERT_HOMEPAGE_LOCATION_TYPE_MISMATCH,
                HomepageLocationType.USER_CUSTOMIZED_NTP,
                HomepageManager.getInstance().getHomepageLocationType());
    }

    @Test
    @SmallTest
    @Feature({"Homepage"})
    public void testStartUp_ChromeNTP_WithPartner() {
        PartnerBrowserCustomizations.getInstance().setHomepageForTests(TEST_URL_FOO);
        mHomepageTestRule.useChromeNTPForTest();

        launchSettingsActivity();

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_ENABLE, mSwitch.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mCustomUriRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_TITLE_ENABLED, mTitleTextView.isEnabled());

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(ASSERT_MESSAGE_EDIT_TEXT, TEST_URL_FOO,
                mCustomUriRadioButton.getPrimaryText().toString());

        Assert.assertEquals(ASSERT_HOMEPAGE_LOCATION_TYPE_MISMATCH,
                HomepageLocationType.USER_CUSTOMIZED_NTP,
                HomepageManager.getInstance().getHomepageLocationType());

        // Reset partner provided information
        PartnerBrowserCustomizations.destroy();
    }

    @Test
    @SmallTest
    @Feature({"Homepage"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testStartUp_ChromeNTP_BottomToolbar() {
        mHomepageTestRule.useCustomizedHomepageForTest(TEST_URL_BAR);
        mHomepageTestRule.useChromeNTPForTest();

        HomepageSettings.setIsHomeButtonOnBottomToolbar(true);

        launchSettingsActivity();

        Assert.assertFalse(ASSERT_MESSAGE_DUET_SWITCH_INVISIBLE, mSwitch.isVisible());

        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mCustomUriRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_TITLE_ENABLED, mTitleTextView.isEnabled());

        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(ASSERT_MESSAGE_EDIT_TEXT, TEST_URL_BAR,
                mCustomUriRadioButton.getPrimaryText().toString());
        Assert.assertEquals(ASSERT_HOMEPAGE_LOCATION_TYPE_MISMATCH,
                HomepageLocationType.USER_CUSTOMIZED_NTP,
                HomepageManager.getInstance().getHomepageLocationType());

        HomepageSettings.setIsHomeButtonOnBottomToolbar(false);
    }

    @Test
    @SmallTest
    @Feature({"Homepage"})
    public void testStartUp_Customized() {
        mHomepageTestRule.useCustomizedHomepageForTest(TEST_URL_BAR);

        launchSettingsActivity();

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_ENABLE, mSwitch.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mCustomUriRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_TITLE_ENABLED, mTitleTextView.isEnabled());

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertTrue(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(ASSERT_MESSAGE_EDIT_TEXT, TEST_URL_BAR,
                mCustomUriRadioButton.getPrimaryText().toString());
        Assert.assertEquals(ASSERT_HOMEPAGE_LOCATION_TYPE_MISMATCH,
                HomepageLocationType.USER_CUSTOMIZED_OTHER,
                HomepageManager.getInstance().getHomepageLocationType());
    }

    @Test
    @SmallTest
    @Feature({"Homepage"})
    @Features.EnableFeatures(ChromeFeatureList.HOMEPAGE_LOCATION_POLICY)
    public void testStartUp_Policies_Customized() {
        // Set mock policies
        mHomepageTestRule.setHomepagePolicyForTest(TEST_URL_BAR);

        launchSettingsActivity();

        // When policy enabled, all components should be disabled.
        Assert.assertFalse(ASSERT_MESSAGE_SWITCH_DISABLE, mSwitch.isEnabled());
        Assert.assertFalse(ASSERT_MESSAGE_RADIO_BUTTON_DISABLED, mCustomUriRadioButton.isEnabled());
        Assert.assertFalse(ASSERT_MESSAGE_TITLE_DISABLED, mTitleTextView.isEnabled());

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());
        Assert.assertTrue(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(ASSERT_MESSAGE_EDIT_TEXT, TEST_URL_BAR,
                mCustomUriRadioButton.getPrimaryText().toString());

        // Additional verification - text message should be displayed, NTP button should be hidden.
        Assert.assertEquals("NTP Button should not be visible.", View.GONE,
                mChromeNtpRadioButton.getVisibility());
        Assert.assertEquals("Customized Button should be visible.", View.VISIBLE,
                mCustomUriRadioButton.getVisibility());
        Assert.assertFalse(
                "Managed text message preference should be in visible when duet is disabled.",
                mManagedText.isVisible());
        Assert.assertEquals(ASSERT_HOMEPAGE_LOCATION_TYPE_MISMATCH,
                HomepageLocationType.POLICY_OTHER,
                HomepageManager.getInstance().getHomepageLocationType());
    }

    @Test
    @SmallTest
    @Feature({"Homepage"})
    @Features.EnableFeatures(ChromeFeatureList.HOMEPAGE_LOCATION_POLICY)
    public void testStartUp_Policies_NTP() {
        // Set mock policies
        mHomepageTestRule.setHomepagePolicyForTest(CHROME_NTP);

        launchSettingsActivity();

        Assert.assertFalse(ASSERT_MESSAGE_SWITCH_DISABLE, mSwitch.isEnabled());
        Assert.assertFalse(ASSERT_MESSAGE_RADIO_BUTTON_DISABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertFalse(ASSERT_MESSAGE_TITLE_DISABLED, mTitleTextView.isEnabled());

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());

        // Additional verification - customized radio button should be disabled.
        Assert.assertEquals("NTP Button should be visible.", View.VISIBLE,
                mChromeNtpRadioButton.getVisibility());
        Assert.assertEquals("Customized Button should not be visible.", View.GONE,
                mCustomUriRadioButton.getVisibility());
        Assert.assertFalse(
                "Managed text message preference should be invisible when duet is disabled.",
                mManagedText.isVisible());
        Assert.assertEquals(ASSERT_HOMEPAGE_LOCATION_TYPE_MISMATCH, HomepageLocationType.POLICY_NTP,
                HomepageManager.getInstance().getHomepageLocationType());
    }

    @Test
    @SmallTest
    @Feature({"Homepage"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @Features.EnableFeatures(ChromeFeatureList.HOMEPAGE_LOCATION_POLICY)
    public void testStartUp_Policies_Customized_BottomToolbar() {
        // Set mock policies
        mHomepageTestRule.setHomepagePolicyForTest(TEST_URL_BAR);

        HomepageSettings.setIsHomeButtonOnBottomToolbar(true);

        launchSettingsActivity();

        Assert.assertFalse(ASSERT_MESSAGE_DUET_SWITCH_INVISIBLE, mSwitch.isVisible());

        Assert.assertFalse(ASSERT_MESSAGE_RADIO_BUTTON_DISABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertFalse(ASSERT_MESSAGE_RADIO_BUTTON_DISABLED, mCustomUriRadioButton.isEnabled());
        Assert.assertFalse(ASSERT_MESSAGE_TITLE_DISABLED, mTitleTextView.isEnabled());

        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mCustomUriRadioButton.isChecked());

        // Additional verification - managed text should be visible
        Assert.assertEquals("NTP Button should not be visible.", View.GONE,
                mChromeNtpRadioButton.getVisibility());
        Assert.assertEquals("Customized Button should not be visible.", View.VISIBLE,
                mCustomUriRadioButton.getVisibility());
        Assert.assertTrue("Managed text message preference should be visible when duet enabled.",
                mManagedText.isVisible());
        Assert.assertFalse(
                "Managed text message preference should be disabled.", mManagedText.isEnabled());
        Assert.assertEquals(ASSERT_HOMEPAGE_LOCATION_TYPE_MISMATCH,
                HomepageLocationType.POLICY_OTHER,
                HomepageManager.getInstance().getHomepageLocationType());

        // Reset policy
        HomepageSettings.setIsHomeButtonOnBottomToolbar(false);
    }

    @Test
    @SmallTest
    @Feature({"Homepage"})
    public void testStartUp_DefaultToPartner() {
        PartnerBrowserCustomizations.getInstance().setHomepageForTests(TEST_URL_FOO);
        mHomepageTestRule.useDefaultHomepageForTest();

        launchSettingsActivity();

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_ENABLE, mSwitch.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mCustomUriRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_TITLE_ENABLED, mTitleTextView.isEnabled());

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertTrue(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(ASSERT_MESSAGE_EDIT_TEXT, TEST_URL_FOO,
                mCustomUriRadioButton.getPrimaryText().toString());

        Assert.assertEquals(ASSERT_HOMEPAGE_LOCATION_TYPE_MISMATCH,
                HomepageLocationType.PARTNER_PROVIDED_OTHER,
                HomepageManager.getInstance().getHomepageLocationType());

        // Reset partner provided information
        PartnerBrowserCustomizations.destroy();
    }

    @Test
    @SmallTest
    @Feature({"Homepage"})
    public void testStartUp_DefaultToNTP() {
        mHomepageTestRule.useDefaultHomepageForTest();

        launchSettingsActivity();

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_ENABLE, mSwitch.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mCustomUriRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_TITLE_ENABLED, mTitleTextView.isEnabled());

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());

        Assert.assertEquals(ASSERT_HOMEPAGE_LOCATION_TYPE_MISMATCH,
                HomepageLocationType.DEFAULT_NTP,
                HomepageManager.getInstance().getHomepageLocationType());

        // When no default homepage provided, the string should just be empty.
        Assert.assertEquals(
                ASSERT_MESSAGE_EDIT_TEXT, "", mCustomUriRadioButton.getPrimaryText().toString());
    }

    @Test
    @SmallTest
    @Feature({"Homepage"})
    public void testStartUp_HomepageDisabled() {
        mHomepageTestRule.useCustomizedHomepageForTest(TEST_URL_BAR);
        mHomepageTestRule.disableHomepageForTest();

        launchSettingsActivity();

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_ENABLE, mSwitch.isEnabled());
        Assert.assertFalse(ASSERT_MESSAGE_RADIO_BUTTON_DISABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertFalse(ASSERT_MESSAGE_RADIO_BUTTON_DISABLED, mCustomUriRadioButton.isEnabled());
        Assert.assertFalse(ASSERT_MESSAGE_TITLE_DISABLED, mTitleTextView.isEnabled());

        Assert.assertFalse(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertTrue(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(ASSERT_MESSAGE_EDIT_TEXT, TEST_URL_BAR,
                mCustomUriRadioButton.getPrimaryText().toString());

        Assert.assertNull(ASSERT_HOMEPAGE_MANAGER_SETTINGS, HomepageManager.getHomepageUri());
    }

    /**
     * Test toggle switch to enable/disable homepage.
     */
    @Test
    @SmallTest
    @Feature({"Homepage"})
    public void testToggleSwitch() {
        mHomepageTestRule.useCustomizedHomepageForTest(TEST_URL_FOO);
        mHomepageTestRule.useChromeNTPForTest();

        launchSettingsActivity();
        LocationChangedCounter actionCounter = new LocationChangedCounter();

        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_ENABLE, mSwitch.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_TITLE_ENABLED, mTitleTextView.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mCustomUriRadioButton.isEnabled());
        Assert.assertTrue("Homepage should be enabled.", HomepageManager.isHomepageEnabled());

        // Check the widget status
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(ASSERT_MESSAGE_EDIT_TEXT, TEST_URL_FOO,
                mCustomUriRadioButton.getPrimaryText().toString());

        // Click the switch
        TestThreadUtils.runOnUiThreadBlocking(() -> mSwitch.performClick());
        Assert.assertFalse("After toggle the switch, " + ASSERT_MESSAGE_TITLE_DISABLED,
                mTitleTextView.isEnabled());
        Assert.assertFalse("After toggle the switch, " + ASSERT_MESSAGE_RADIO_BUTTON_DISABLED,
                mChromeNtpRadioButton.isEnabled());
        Assert.assertFalse("After toggle the switch, " + ASSERT_MESSAGE_RADIO_BUTTON_DISABLED,
                mCustomUriRadioButton.isEnabled());
        Assert.assertFalse("Homepage should be disabled after toggle switch.",
                HomepageManager.isHomepageEnabled());

        // Check the widget status - everything should remain unchanged.
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(ASSERT_MESSAGE_EDIT_TEXT, TEST_URL_FOO,
                mCustomUriRadioButton.getPrimaryText().toString());

        TestThreadUtils.runOnUiThreadBlocking(() -> mSwitch.performClick());
        Assert.assertTrue(ASSERT_MESSAGE_TITLE_ENABLED, mTitleTextView.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mChromeNtpRadioButton.isEnabled());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_ENABLED, mCustomUriRadioButton.isEnabled());
        Assert.assertTrue("Homepage should be enabled again.", HomepageManager.isHomepageEnabled());

        // Check the widget status - everything should remain unchanged.
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(ASSERT_MESSAGE_EDIT_TEXT, TEST_URL_FOO,
                mCustomUriRadioButton.getPrimaryText().toString());

        Assert.assertEquals(
                "Histogram for location change should not change when toggling switch preference.",
                0, actionCounter.locationChangedCount);
        actionCounter.tearDown();
    }

    /**
     * Test checking different radio button to change the homepage.
     */
    @Test
    @SmallTest
    @Feature({"Homepage"})
    public void testCheckRadioButtons() {
        mHomepageTestRule.useCustomizedHomepageForTest(TEST_URL_FOO);
        launchSettingsActivity();
        LocationChangedCounter counter = new LocationChangedCounter();

        // Initial state check
        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());
        Assert.assertTrue(ASSERT_MESSAGE_TITLE_ENABLED, mTitleTextView.isEnabled());
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertTrue(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(ASSERT_MESSAGE_EDIT_TEXT, TEST_URL_FOO,
                mCustomUriRadioButton.getPrimaryText().toString());
        Assert.assertEquals(
                ASSERT_HOMEPAGE_MANAGER_SETTINGS, TEST_URL_FOO, HomepageManager.getHomepageUri());
        Assert.assertEquals(
                ASSERT_HOMEPAGE_LOCATION_HISTOGRAM_COUNT, 0, counter.locationChangedCount);

        // Check radio button to select NTP as homepage. Homepage is not changed yet at this time.
        checkRadioButtonAndWait(mChromeNtpRadioButton);

        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(ASSERT_MESSAGE_EDIT_TEXT, TEST_URL_FOO,
                mCustomUriRadioButton.getPrimaryText().toString());
        Assert.assertEquals(
                ASSERT_HOMEPAGE_LOCATION_HISTOGRAM_COUNT, 0, counter.locationChangedCount);

        // Check back to customized radio button
        checkRadioButtonAndWait(mCustomUriRadioButton);

        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertEquals(ASSERT_MESSAGE_EDIT_TEXT, TEST_URL_FOO,
                mCustomUriRadioButton.getPrimaryText().toString());

        // End the activity. The homepage should be the customized url, and the location counter
        // should stay at 0 as nothing is changed.
        finishSettingsActivity();
        Assert.assertEquals(
                ASSERT_HOMEPAGE_MANAGER_SETTINGS, TEST_URL_FOO, HomepageManager.getHomepageUri());
        Assert.assertEquals(
                ASSERT_HOMEPAGE_LOCATION_HISTOGRAM_COUNT, 0, counter.locationChangedCount);
    }

    /**
     * Test if changing uris in EditText will change homepage accordingly.
     */
    @Test
    @SmallTest
    @Feature({"Homepage"})
    public void testChangeCustomized() {
        mHomepageTestRule.useChromeNTPForTest();
        launchSettingsActivity();
        LocationChangedCounter actionCounter = new LocationChangedCounter();

        // Initial state check
        Assert.assertTrue(ASSERT_MESSAGE_SWITCH_CHECK, mSwitch.isChecked());
        Assert.assertTrue(ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());
        Assert.assertEquals(
                ASSERT_MESSAGE_EDIT_TEXT, "", mCustomUriRadioButton.getPrimaryText().toString());
        Assert.assertTrue(ASSERT_HOMEPAGE_MANAGER_SETTINGS,
                NewTabPage.isNTPUrl(HomepageManager.getHomepageUri()));
        Assert.assertEquals(
                ASSERT_HOMEPAGE_LOCATION_HISTOGRAM_COUNT, 0, actionCounter.locationChangedCount);

        // Update the text box. To do this, request focus for customized radio button so that the
        // checked option will be changed.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mCustomUriRadioButton.getEditTextForTests().requestFocus();
            mCustomUriRadioButton.setPrimaryText(TEST_URL_FOO);
        });
        CriteriaHelper.pollUiThread(new Criteria("EditText never got the focus.") {
            @Override
            public boolean isSatisfied() {
                return mCustomUriRadioButton.getEditTextForTests().isFocused();
            }
        });

        // Radio Button should switched to customized homepage.
        Assert.assertFalse(
                ASSERT_MESSAGE_RADIO_BUTTON_NTP_CHECK, mChromeNtpRadioButton.isChecked());
        Assert.assertTrue(
                ASSERT_MESSAGE_RADIO_BUTTON_CUSTOMIZED_CHECK, mCustomUriRadioButton.isChecked());

        // Update the text box and exit the activity, homepage should change accordingly.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mCustomUriRadioButton.setPrimaryText(TEST_URL_BAR));
        finishSettingsActivity();

        Assert.assertEquals(
                ASSERT_HOMEPAGE_MANAGER_SETTINGS, TEST_URL_BAR, HomepageManager.getHomepageUri());
        Assert.assertEquals(
                ASSERT_HOMEPAGE_LOCATION_HISTOGRAM_COUNT, 1, actionCounter.locationChangedCount);

        actionCounter.tearDown();
    }

    private void checkRadioButtonAndWait(RadioButtonWithDescription radioButton) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { TouchCommon.singleClickView(radioButton, 5, 5); });
        CriteriaHelper.pollUiThread(radioButton::isChecked, "Radio button is never checked.");
    }

    /**
     * Record user action "Settings.Homepage.LocationChanged"
     */
    private static class LocationChangedCounter extends UserActionTester {
        public int locationChangedCount;

        @Override
        public void onActionRecorded(String action) {
            if (action.equals("Settings.Homepage.LocationChanged_V2")) ++locationChangedCount;
        }
    }
}
