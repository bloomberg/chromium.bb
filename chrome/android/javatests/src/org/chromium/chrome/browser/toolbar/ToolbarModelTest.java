// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;

/**
 * Tests for ToolbarModel.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ToolbarModelTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    /**
     * After closing all {@link Tab}s, the {@link ToolbarModel} should know that it is not
     * showing any {@link Tab}.
     * @throws InterruptedException
     */
    @Test
    @Feature({"Android-Toolbar"})
    @MediumTest
    @RetryOnFailure
    public void testClosingLastTabReflectedInModel() throws InterruptedException {
        Assert.assertNotSame("No current tab", Tab.INVALID_TAB_ID,
                getCurrentTabId(mActivityTestRule.getActivity()));
        ChromeTabUtils.closeCurrentTab(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        Assert.assertEquals("Didn't close all tabs.", 0,
                ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity()));
        Assert.assertEquals("ToolbarModel is still trying to show a tab.", Tab.INVALID_TAB_ID,
                getCurrentTabId(mActivityTestRule.getActivity()));
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.OMNIBOX_HIDE_SCHEME_DOMAIN_IN_STEADY_STATE)
    public void testDisplayAndEditText_DisabledExperiment() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            TestToolbarModel model = new TestToolbarModel();
            model.mUrl = UrlConstants.NTP_URL;
            assertDisplayAndEditText(model, "", null);

            model.mUrl = "chrome://about";
            model.mDisplayUrl = "chrome://about";
            model.mFullUrl = "chrome://about";
            assertDisplayAndEditText(model, "chrome://about", null);

            model.mUrl = "https://www.foo.com";
            model.mDisplayUrl = "foo.com";
            model.mFullUrl = "https://foo.com";
            assertDisplayAndEditText(model, "https://foo.com", null);
        });
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_HIDE_SCHEME_DOMAIN_IN_STEADY_STATE)
    public void testDisplayAndEditText_EnabledExperiment() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            TestToolbarModel model = new TestToolbarModel();
            model.mUrl = UrlConstants.NTP_URL;
            assertDisplayAndEditText(model, "", null);

            model.mUrl = "chrome://about";
            model.mDisplayUrl = "chrome://about";
            model.mFullUrl = "chrome://about";
            assertDisplayAndEditText(model, "chrome://about", null);

            model.mUrl = "https://www.foo.com";
            model.mDisplayUrl = "foo.com";
            model.mFullUrl = "https://foo.com";
            assertDisplayAndEditText(model, "foo.com", "https://foo.com");
        });
    }

    private void assertDisplayAndEditText(
            ToolbarDataProvider dataProvider, String displayText, String editText) {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            UrlBarData urlBarData = dataProvider.getUrlBarData();
            Assert.assertEquals(
                    "Display text did not match", displayText, urlBarData.displayText.toString());
            Assert.assertEquals("Editing text did not match", editText, urlBarData.editingText);
        });
    }

    /**
     * @param activity A reference to {@link ChromeTabbedActivity} to pull
     *            {@link android.view.View} data from.
     * @return The id of the current {@link Tab} as far as the {@link ToolbarModel} sees it.
     */
    public static int getCurrentTabId(final ChromeTabbedActivity activity) {
        ToolbarLayout toolbar = (ToolbarLayout) activity.findViewById(R.id.toolbar);
        Assert.assertNotNull("Toolbar is null", toolbar);

        ToolbarDataProvider dataProvider = toolbar.getToolbarDataProvider();
        Tab tab = dataProvider.getTab();
        return tab != null ? tab.getId() : Tab.INVALID_TAB_ID;
    }

    private class TestToolbarModel extends ToolbarModel {
        private String mDisplayUrl;
        private String mFullUrl;
        private String mUrl;

        public TestToolbarModel() {
            super(ContextUtils.getApplicationContext(), null /* bottomSheet */,
                    false /* useModernDesign */);
            initializeWithNative();

            Tab tab = new Tab(0, false, null) {
                @Override
                public boolean isInitialized() {
                    return true;
                }

                @Override
                public boolean isFrozen() {
                    return false;
                }
            };
            setTab(tab, false);
        }

        @Override
        public String getCurrentUrl() {
            return mUrl == null ? super.getCurrentUrl() : mUrl;
        }

        @Override
        public String getFormattedFullUrl() {
            return mFullUrl == null ? super.getFormattedFullUrl() : mFullUrl;
        }

        @Override
        public String getUrlForDisplay() {
            return mDisplayUrl == null ? super.getUrlForDisplay() : mDisplayUrl;
        }
    }
}
