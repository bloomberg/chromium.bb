// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.survey;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.SharedPreferences;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Unit tests for ChromeHomeSurveyController.java.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ChromeHomeSurveyControllerTest {
    private ChromeHomeSurveyController mController;
    private SharedPreferences mSharedPreferences;

    @Mock
    Tab mTab;

    @Mock
    WebContents mWebContents;

    @Before
    public void before() {
        MockitoAnnotations.initMocks(this);

        ContextUtils.initApplicationContextForTests(RuntimeEnvironment.application);
        mController = ChromeHomeSurveyController.createChromeHomeSurveyControllerForTests();
        mSharedPreferences = ContextUtils.getAppSharedPreferences();
        mSharedPreferences.edit().clear().apply();
    }

    @After
    public void after() {
        mSharedPreferences.edit().clear().apply();
    }

    @Test
    public void testInfoBarDisplayedBefore() {
        Assert.assertFalse(mSharedPreferences.contains(
                ChromeHomeSurveyController.SURVEY_INFO_BAR_DISPLAYED_KEY));
        Assert.assertFalse(mController.hasInfoBarBeenDisplayed());
        mSharedPreferences.edit()
                .putLong(ChromeHomeSurveyController.SURVEY_INFO_BAR_DISPLAYED_KEY,
                        System.currentTimeMillis())
                .apply();
        Assert.assertTrue(mController.hasInfoBarBeenDisplayed());
    }

    @Test
    public void testChromeHomeEnabledForOneWeek() {
        Assert.assertFalse(mController.wasChromeHomeEnabledForMinimumOneWeek());
        Assert.assertFalse(mSharedPreferences.contains(
                ChromePreferenceManager.CHROME_HOME_SHARED_PREFERENCES_KEY));
        mSharedPreferences.edit()
                .putLong(ChromePreferenceManager.CHROME_HOME_SHARED_PREFERENCES_KEY,
                        System.currentTimeMillis() - ChromeHomeSurveyController.ONE_WEEK_IN_MILLIS)
                .apply();
        Assert.assertTrue(mController.wasChromeHomeEnabledForMinimumOneWeek());
    }

    @Test
    public void testChromeHomeEnabledForLessThanOneWeek() {
        Assert.assertFalse(mController.wasChromeHomeEnabledForMinimumOneWeek());
        Assert.assertFalse(mSharedPreferences.contains(
                ChromePreferenceManager.CHROME_HOME_SHARED_PREFERENCES_KEY));
        mSharedPreferences.edit()
                .putLong(ChromePreferenceManager.CHROME_HOME_SHARED_PREFERENCES_KEY,
                        System.currentTimeMillis()
                                - ChromeHomeSurveyController.ONE_WEEK_IN_MILLIS / 2)
                .apply();
        Assert.assertFalse(mController.wasChromeHomeEnabledForMinimumOneWeek());
    }

    @Test
    public void testValidTab() {
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(false).when(mTab).isIncognito();
        Assert.assertTrue(mController.isValidTabForSurvey(mTab));
        verify(mTab, times(1)).getWebContents();
        verify(mTab, times(1)).isIncognito();
    }

    @Test
    public void testNullTab() {
        Assert.assertFalse(mController.isValidTabForSurvey(null));
    }

    @Test
    public void testIncognitoTab() {
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(true).when(mTab).isIncognito();
        Assert.assertFalse(mController.isValidTabForSurvey(mTab));
        verify(mTab, times(1)).getWebContents();
        verify(mTab, times(1)).isIncognito();
    }

    @Test
    public void testTabWithNoWebContents() {
        doReturn(null).when(mTab).getWebContents();
        Assert.assertFalse(mController.isValidTabForSurvey(mTab));
        verify(mTab, times(1)).getWebContents();
        verify(mTab, never()).isIncognito();
    }

    @Test
    public void testEligibilityRolledYesterday() {
        mController = new RiggedSurveyController(0, 5, 10);
        mSharedPreferences.edit().putInt(ChromeHomeSurveyController.DATE_LAST_ROLLED_KEY, 4);
        Assert.assertTrue(mController.isRandomlySelectedForSurvey());
    }

    @Test
    public void testEligibilityRollingTwiceSameDay() {
        mController = new RiggedSurveyController(0, 5, 10);
        mSharedPreferences.edit()
                .putInt(ChromeHomeSurveyController.DATE_LAST_ROLLED_KEY, 5)
                .apply();
        Assert.assertFalse(mController.isRandomlySelectedForSurvey());
    }

    @Test
    public void testEligibilityFirstTimeRollingQualifies() {
        mController = new RiggedSurveyController(0, 5, 10);
        Assert.assertFalse(
                mSharedPreferences.contains(ChromeHomeSurveyController.DATE_LAST_ROLLED_KEY));
        Assert.assertTrue(mController.isRandomlySelectedForSurvey());
        Assert.assertEquals(
                5, mSharedPreferences.getInt(ChromeHomeSurveyController.DATE_LAST_ROLLED_KEY, -1));
    }

    @Test
    public void testEligibilityFirstTimeRollingDoesNotQualify() {
        mController = new RiggedSurveyController(5, 1, 10);
        Assert.assertFalse(
                mSharedPreferences.contains(ChromeHomeSurveyController.DATE_LAST_ROLLED_KEY));
        Assert.assertFalse(mController.isRandomlySelectedForSurvey());
        Assert.assertEquals(
                1, mSharedPreferences.getInt(ChromeHomeSurveyController.DATE_LAST_ROLLED_KEY, -1));
    }

    class RiggedSurveyController extends ChromeHomeSurveyController {
        int mRandomNumberToReturn;
        int mDayOfYear;
        int mMaxNumber;

        RiggedSurveyController(int randomNumberToReturn, int dayOfYear, int maxNumber) {
            super();
            mRandomNumberToReturn = randomNumberToReturn;
            mDayOfYear = dayOfYear;
            mMaxNumber = maxNumber;
        }

        @Override
        int getRandomNumberUpTo(int max) {
            return mRandomNumberToReturn;
        }

        @Override
        int getDayOfYear() {
            return mDayOfYear;
        }

        @Override
        int getMaxNumber() {
            return mMaxNumber;
        }
    }
}
