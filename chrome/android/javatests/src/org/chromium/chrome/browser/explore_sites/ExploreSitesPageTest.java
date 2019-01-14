// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import static android.support.test.espresso.Espresso.onView;

import static org.hamcrest.Matchers.instanceOf;

import android.annotation.TargetApi;
import android.os.Build;
import android.support.test.espresso.contrib.RecyclerViewActions;
import android.support.test.filters.SmallTest;
import android.support.v7.widget.RecyclerView;
import android.view.ViewGroup;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.RenderTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

import java.util.ArrayList;

/**
 * Simple test to demonstrate use of ScreenShooter rule.
 */
// TODO(https://crbug.com/894334): Remove format suppression once formatting bug is fixed.
// clang-format off
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures(ChromeFeatureList.EXPLORE_SITES)
public class ExploreSitesPageTest {
    // clang-format on

    ArrayList<ExploreSitesCategory> getTestingCatalog() {
        final ArrayList<ExploreSitesCategory> categoryList = new ArrayList<>();
        for (int i = 0; i < 5; i++) {
            ExploreSitesCategory category =
                    new ExploreSitesCategory(i, i, "Category #" + Integer.toString(i),
                            /* ntpShownCount = */ 1, /* interactionCount = */ 0);
            // 0th category would be filtered out. Tests that row maximums are obeyed.
            int numSites = 4 * i + 1;
            for (int j = 0; j < numSites; j++) {
                ExploreSitesSite site = new ExploreSitesSite(
                        i * 8 + j, "Site #" + Integer.toString(j), "https://example.com/", false);
                category.addSite(site);
            }
            categoryList.add(category);
        }

        return categoryList;
    }

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public RenderTestRule mRenderTestRule =
            new RenderTestRule("chrome/test/data/android/render_tests");

    private Tab mTab;
    private ViewGroup mRecyclerView;
    private ExploreSitesPage mEsp;

    @Before
    public void setUp() throws Exception {
        ExploreSitesBridge.setCatalogForTesting(getTestingCatalog());
        mActivityTestRule.startMainActivityWithURL("about:blank");

        mActivityTestRule.loadUrl(UrlConstants.EXPLORE_URL);
        mTab = mActivityTestRule.getActivity().getActivityTab();
        waitForEspLoaded(mTab);

        Assert.assertTrue(mTab.getNativePage() instanceof ExploreSitesPage);
        mEsp = (ExploreSitesPage) mTab.getNativePage();
        mRecyclerView = mEsp.getView().findViewById(R.id.explore_sites_category_recycler);
    }

    @After
    public void tearDown() throws Exception {
        ExploreSitesBridge.setCatalogForTesting(null);
    }

    @Test
    @SmallTest
    @Feature({"ExploreSites", "RenderTest"})
    public void testScrolledLayout_withBack() throws Exception {
        onView(instanceOf(RecyclerView.class)).perform(RecyclerViewActions.scrollToPosition(2));

        mRenderTestRule.render(mRecyclerView, "recycler_layout");
        mActivityTestRule.loadUrl("about:blank");
        ThreadUtils.runOnUiThreadBlocking(() -> mActivityTestRule.getActivity().onBackPressed());
        mRenderTestRule.render(mRecyclerView, "recycler_layout_back");
    }

    @Test
    @SmallTest
    @Feature({"ExploreSites", "RenderTest"})
    public void testInitialLayout() throws Exception {
        onView(instanceOf(RecyclerView.class)).perform(RecyclerViewActions.scrollToPosition(0));
        mRenderTestRule.render(mRecyclerView, "initial_layout");
    }

    @Test
    @SmallTest
    @Feature({"ExploreSites", "RenderTest"})
    public void testScrollingFromNTP() throws Exception {
        mActivityTestRule.loadUrl("about:blank");
        ExploreSitesCategory category = getTestingCatalog().get(2);
        mActivityTestRule.loadUrl(category.getUrl());
        waitForEspLoaded(mTab);
        Assert.assertTrue(mTab.getNativePage() instanceof ExploreSitesPage);
        mEsp = (ExploreSitesPage) mTab.getNativePage();
        mRecyclerView = mEsp.getView().findViewById(R.id.explore_sites_category_recycler);
        mRenderTestRule.render(mRecyclerView, "scrolled_to_category_2");
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    public static void waitForEspLoaded(final Tab tab) {
        CriteriaHelper.pollUiThread(new Criteria("ESP never fully loaded") {
            @Override
            public boolean isSatisfied() {
                if (tab.getNativePage() instanceof ExploreSitesPage) {
                    return ((ExploreSitesPage) tab.getNativePage()).isLoadedForTests();
                } else {
                    return false;
                }
            }
        });
    }
}
