// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.subresource_filter.TestSubresourceFilterPublisher;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.safe_browsing.SafeBrowsingApiBridge;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * End to end tests of SubresourceFilter ad filtering on Android.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=SubresourceFilter<SB,SubresourceFilterExperimentalUI",
        "force-fieldtrials=SB/Enabled",
        "force-fieldtrial-params=SB.Enabled:enable_presets/liverun_on_better_ads_violating_sites"})

public final class SubresourceFilterTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);
    private EmbeddedTestServer mTestServer;

    private static final String PAGE_WITH_JPG =
            "/chrome/test/data/android/subresource_filter/page-with-img.html";
    private static final String METADATA_FOR_ENFORCEMENT =
            "{\"matches\":[{\"threat_type\":\"13\",\"sf_bas\":\"\"}]}";
    private static final String METADATA_FOR_WARNING =
            "{\"matches\":[{\"threat_type\":\"13\",\"sf_bas\":\"warn\"}]}";

    private void createAndPublishRulesetDisallowingSuffix(String suffix) {
        TestSubresourceFilterPublisher publisher = new TestSubresourceFilterPublisher();
        ThreadUtils.runOnUiThreadBlocking(
                (Runnable) ()
                        -> publisher.createAndPublishRulesetDisallowingSuffixForTesting(suffix));
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return publisher.isPublished();
            }
        });
    }

    @Before
    public void setUp() throws Exception {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());

        // Create a new temporary instance to ensure the Class is loaded. Otherwise we will get a
        // ClassNotFoundException when trying to instantiate during startup.
        SafeBrowsingApiBridge.setSafeBrowsingHandlerType(
                new MockSafeBrowsingApiHandler().getClass());
        mActivityTestRule.startMainActivityOnBlankPage();

        // Disallow all jpgs.
        createAndPublishRulesetDisallowingSuffix(".jpg");
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        MockSafeBrowsingApiHandler.clearMockResponses();
    }

    @Test
    @MediumTest
    public void resourceNotFiltered() throws Exception {
        String url = mTestServer.getURL(PAGE_WITH_JPG);
        mActivityTestRule.loadUrl(url);

        String loaded = mActivityTestRule.runJavaScriptCodeInCurrentTab("imgLoaded");
        Assert.assertEquals("true", loaded);
    }

    @Test
    @MediumTest
    public void resourceFiltered() throws Exception {
        String url = mTestServer.getURL(PAGE_WITH_JPG);
        MockSafeBrowsingApiHandler.addMockResponse(url, METADATA_FOR_ENFORCEMENT);
        mActivityTestRule.loadUrl(url);

        String loaded = mActivityTestRule.runJavaScriptCodeInCurrentTab("imgLoaded");
        Assert.assertEquals("false", loaded);
    }

    @Test
    @MediumTest
    public void resourceNotFilteredWithWarning() throws Exception {
        String url = mTestServer.getURL(PAGE_WITH_JPG);
        MockSafeBrowsingApiHandler.addMockResponse(url, METADATA_FOR_WARNING);
        mActivityTestRule.loadUrl(url);

        String loaded = mActivityTestRule.runJavaScriptCodeInCurrentTab("imgLoaded");
        Assert.assertEquals("true", loaded);
    }
}
