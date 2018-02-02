// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.survey;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.SharedPreferences;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.content_public.browser.WebContents;

/**
 * Unit tests for ChromeHomeSurveyController.java.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ChromeHomeSurveyControllerTest {
    private TestChromeHomeSurveyController mTestController;
    private RiggedSurveyController mRiggedController;
    private SharedPreferences mSharedPreferences;

    @Mock
    Tab mTab;

    @Mock
    WebContents mWebContents;

    @Mock
    TabModelSelector mSelector;

    @Before
    public void before() {
        MockitoAnnotations.initMocks(this);
        RecordHistogram.setDisabledForTests(true);

        mTestController = new TestChromeHomeSurveyController();
        mTestController.setTabModelSelector(mSelector);
        mSharedPreferences = ContextUtils.getAppSharedPreferences();
        mSharedPreferences.edit().clear().apply();
        Assert.assertNull("Tab should be null", mTestController.getLastTabInfobarShown());
    }

    @After
    public void after() {
        mSharedPreferences.edit().clear().apply();
        RecordHistogram.setDisabledForTests(false);
    }

    @Test
    public void testInfoBarDisplayedBefore() {
        Assert.assertFalse(mSharedPreferences.contains(
                ChromeHomeSurveyController.SURVEY_INFO_BAR_DISPLAYED_KEY));
        Assert.assertFalse(mTestController.hasInfoBarBeenDisplayed());
        mSharedPreferences.edit()
                .putLong(ChromeHomeSurveyController.SURVEY_INFO_BAR_DISPLAYED_KEY,
                        System.currentTimeMillis())
                .apply();
        Assert.assertTrue(mTestController.hasInfoBarBeenDisplayed());
    }

    @Test
    public void testChromeHomeEnabledForOneWeek() {
        Assert.assertFalse(mTestController.wasChromeHomeEnabledForMinimumOneWeek());
        Assert.assertFalse(mSharedPreferences.contains(
                ChromePreferenceManager.CHROME_HOME_SHARED_PREFERENCES_KEY));
        mSharedPreferences.edit()
                .putLong(ChromePreferenceManager.CHROME_HOME_SHARED_PREFERENCES_KEY,
                        System.currentTimeMillis() - ChromeHomeSurveyController.ONE_WEEK_IN_MILLIS)
                .apply();
        Assert.assertTrue(mTestController.wasChromeHomeEnabledForMinimumOneWeek());
    }

    @Test
    public void testChromeHomeEnabledForLessThanOneWeek() {
        Assert.assertFalse(mTestController.wasChromeHomeEnabledForMinimumOneWeek());
        Assert.assertFalse(mSharedPreferences.contains(
                ChromePreferenceManager.CHROME_HOME_SHARED_PREFERENCES_KEY));
        mSharedPreferences.edit()
                .putLong(ChromePreferenceManager.CHROME_HOME_SHARED_PREFERENCES_KEY,
                        System.currentTimeMillis()
                                - ChromeHomeSurveyController.ONE_WEEK_IN_MILLIS / 2)
                .apply();
        Assert.assertFalse(mTestController.wasChromeHomeEnabledForMinimumOneWeek());
    }

    @Test
    public void testValidTab() {
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(false).when(mTab).isIncognito();

        Assert.assertTrue(mTestController.isValidTabForSurvey(mTab));

        verify(mTab, atLeastOnce()).getWebContents();
        verify(mTab, atLeastOnce()).isIncognito();
    }

    @Test
    public void testNullTab() {
        Assert.assertFalse(mTestController.isValidTabForSurvey(null));
    }

    @Test
    public void testIncognitoTab() {
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(true).when(mTab).isIncognito();

        Assert.assertFalse(mTestController.isValidTabForSurvey(mTab));

        verify(mTab, atLeastOnce()).isIncognito();
    }

    @Test
    public void testTabWithNoWebContents() {
        doReturn(null).when(mTab).getWebContents();

        Assert.assertFalse(mTestController.isValidTabForSurvey(mTab));

        verify(mTab, atLeastOnce()).getWebContents();
        verify(mTab, never()).isIncognito();
    }

    @Test
    public void testSurveyAvailableWebContentsLoaded() {
        doReturn(mTab).when(mSelector).getCurrentTab();
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(false).when(mTab).isIncognito();
        doReturn(true).when(mTab).isUserInteractable();
        doReturn(false).when(mWebContents).isLoading();

        mTestController.onSurveyAvailable(null);
        Assert.assertEquals("Tabs should be equal", mTab, mTestController.getLastTabInfobarShown());

        verify(mSelector, atLeastOnce()).getCurrentTab();
        verify(mTab, atLeastOnce()).isIncognito();
        verify(mTab, atLeastOnce()).isUserInteractable();
        verify(mTab, atLeastOnce()).isLoading();
    }

    @Test
    public void testShowInfoBarTabApplicable() {
        doReturn(true).when(mTab).isUserInteractable();
        doReturn(false).when(mTab).isLoading();

        mTestController.showInfoBarIfApplicable(mTab, null, null);
        Assert.assertEquals("Tabs should be equal", mTab, mTestController.getLastTabInfobarShown());
        verify(mTab, atLeastOnce()).isUserInteractable();
        verify(mTab, atLeastOnce()).isLoading();
    }

    @Test
    public void testShowInfoBarTabNotApplicable() {
        doReturn(false).when(mTab).isUserInteractable();
        doReturn(true).when(mTab).isLoading();

        mTestController.showInfoBarIfApplicable(mTab, null, null);
        Assert.assertNull("Tab should be null", mTestController.getLastTabInfobarShown());
        verify(mTab, atLeastOnce()).isUserInteractable();
    }

    @Test
    public void testSurveyAvailableNullTab() {
        doReturn(null).when(mSelector).getCurrentTab();

        mTestController.onSurveyAvailable(null);
        Assert.assertNull("Tab should be null", mTestController.getLastTabInfobarShown());
        verify(mSelector).addObserver(any());
    }

    @Test
    public void testEligibilityRolledYesterday() {
        mRiggedController = new RiggedSurveyController(0, 5, 10);
        mSharedPreferences.edit().putInt(ChromeHomeSurveyController.DATE_LAST_ROLLED_KEY, 4);
        Assert.assertTrue(
                "Random selection should be true", mRiggedController.isRandomlySelectedForSurvey());
    }

    @Test
    public void testEligibilityRollingTwiceSameDay() {
        mRiggedController = new RiggedSurveyController(0, 5, 10);
        mSharedPreferences.edit()
                .putInt(ChromeHomeSurveyController.DATE_LAST_ROLLED_KEY, 5)
                .apply();
        Assert.assertFalse("Random selection should be false",
                mRiggedController.isRandomlySelectedForSurvey());
    }

    @Test
    public void testEligibilityFirstTimeRollingQualifies() {
        mRiggedController = new RiggedSurveyController(0, 5, 10);
        Assert.assertFalse(
                mSharedPreferences.contains(ChromeHomeSurveyController.DATE_LAST_ROLLED_KEY));
        Assert.assertTrue(
                "Random selection should be true", mRiggedController.isRandomlySelectedForSurvey());
        Assert.assertEquals("Numbers should match", 5,
                mSharedPreferences.getInt(ChromeHomeSurveyController.DATE_LAST_ROLLED_KEY, -1));
    }

    @Test
    public void testEligibilityFirstTimeRollingDoesNotQualify() {
        mRiggedController = new RiggedSurveyController(5, 1, 10);
        Assert.assertFalse(
                mSharedPreferences.contains(ChromeHomeSurveyController.DATE_LAST_ROLLED_KEY));
        Assert.assertFalse(
                "Random selection should be true", mRiggedController.isRandomlySelectedForSurvey());
        Assert.assertEquals("Numbers should match", 1,
                mSharedPreferences.getInt(ChromeHomeSurveyController.DATE_LAST_ROLLED_KEY, -1));
    }

    class RiggedSurveyController extends ChromeHomeSurveyController {
        private int mRandomNumberToReturn;
        private int mDayOfYear;
        private int mMaxNumber;

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

    class TestChromeHomeSurveyController extends ChromeHomeSurveyController {
        private Tab mTab;

        public TestChromeHomeSurveyController() {
            super();
        }

        @Override
        void showSurveyInfoBar(Tab tab, String siteId) {
            mTab = tab;
        }

        public Tab getLastTabInfobarShown() {
            return mTab;
        }
    }
}
