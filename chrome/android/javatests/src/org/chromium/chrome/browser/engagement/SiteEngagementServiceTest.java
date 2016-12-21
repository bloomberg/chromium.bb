// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.engagement;

import android.support.test.filters.SmallTest;
import android.test.UiThreadTest;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;

/**
 * Test for the Site Engagement Service Java binding.
 */
public class SiteEngagementServiceTest extends ChromeActivityTestCaseBase<ChromeActivity> {

    public SiteEngagementServiceTest() {
        super(ChromeActivity.class);
    }

    /**
     * Verify that setting the engagement score for a URL and reading it back it works.
     */
    @SmallTest
    @UiThreadTest
    @Feature({"Engagement"})
    public void testSettingAndRetrievingScore() {
        final String url = "https://www.google.com";
        SiteEngagementService service = SiteEngagementService.getForProfile(
                getActivity().getActivityTab().getProfile());

        assertEquals(0.0, service.getScore(url));
        service.resetScoreForUrl(url, 5.0);
        assertEquals(5.0, service.getScore(url));

        service.resetScoreForUrl(url, 2.0);
        assertEquals(2.0, service.getScore(url));
    }

    /**
     * Verify that repeatedly fetching and throwing away the SiteEngagementService works.
     */
    @SmallTest
    @UiThreadTest
    @Feature({"Engagement"})
    public void testRepeatedlyGettingService() {
        final String url = "https://www.google.com";
        Profile profile = getActivity().getActivityTab().getProfile();

        assertEquals(0.0, SiteEngagementService.getForProfile(profile).getScore(url));
        SiteEngagementService.getForProfile(profile).resetScoreForUrl(url, 5.0);
        assertEquals(5.0, SiteEngagementService.getForProfile(profile).getScore(url));

        SiteEngagementService.getForProfile(profile).resetScoreForUrl(url, 2.0);
        assertEquals(2.0, SiteEngagementService.getForProfile(profile).getScore(url));
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }
}
