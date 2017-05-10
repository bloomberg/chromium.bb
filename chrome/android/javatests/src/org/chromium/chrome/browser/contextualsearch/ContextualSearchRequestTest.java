// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/**
 * Class responsible for testing the ContextualSearchRequest.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeActivityTestRule.DISABLE_NETWORK_PREDICTION_FLAG,
})
public class ContextualSearchRequestTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    ContextualSearchRequest mRequest;
    ContextualSearchRequest mNormalPriorityOnlyRequest;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mRequest = new ContextualSearchRequest("barack obama", "barack", "", true);
                mNormalPriorityOnlyRequest =
                        new ContextualSearchRequest("woody allen", "allen", "", false);
            }
        });
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testIsUsingLowPriority() {
        Assert.assertTrue(mRequest.isUsingLowPriority());
        Assert.assertFalse(mNormalPriorityOnlyRequest.isUsingLowPriority());
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @RetryOnFailure
    public void testHasFailed() {
        Assert.assertFalse(mRequest.getHasFailed());
        mRequest.setHasFailed();
        Assert.assertTrue(mRequest.getHasFailed());
        Assert.assertFalse(mNormalPriorityOnlyRequest.getHasFailed());
        mNormalPriorityOnlyRequest.setHasFailed();
        Assert.assertTrue(mNormalPriorityOnlyRequest.getHasFailed());
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testSetNormalPriority() {
        Assert.assertTrue(mRequest.isUsingLowPriority());
        mRequest.setNormalPriority();
        Assert.assertFalse(mRequest.isUsingLowPriority());
        Assert.assertFalse(mNormalPriorityOnlyRequest.isUsingLowPriority());
    }
}
