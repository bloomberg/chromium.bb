// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.support.test.filters.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;

/**
 * Class responsible for testing the ContextualSearchRequest.
 */
public class ContextualSearchRequestTest extends ChromeTabbedActivityTestBase {
    ContextualSearchRequest mRequest;
    ContextualSearchRequest mNormalPriorityOnlyRequest;

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                mRequest = new ContextualSearchRequest("barack obama", "barack", "", true);
                mNormalPriorityOnlyRequest =
                        new ContextualSearchRequest("woody allen", "allen", "", false);
            }
        });
    }

    @SmallTest
    @Feature({"ContextualSearch"})
    public void testIsUsingLowPriority() {
        assertTrue(mRequest.isUsingLowPriority());
        assertFalse(mNormalPriorityOnlyRequest.isUsingLowPriority());
    }

    @SmallTest
    @Feature({"ContextualSearch"})
    @RetryOnFailure
    public void testHasFailed() {
        assertFalse(mRequest.getHasFailed());
        mRequest.setHasFailed();
        assertTrue(mRequest.getHasFailed());
        assertFalse(mNormalPriorityOnlyRequest.getHasFailed());
        mNormalPriorityOnlyRequest.setHasFailed();
        assertTrue(mNormalPriorityOnlyRequest.getHasFailed());
    }

    @SmallTest
    @Feature({"ContextualSearch"})
    public void testSetNormalPriority() {
        assertTrue(mRequest.isUsingLowPriority());
        mRequest.setNormalPriority();
        assertFalse(mRequest.isUsingLowPriority());
        assertFalse(mNormalPriorityOnlyRequest.isUsingLowPriority());
    }
}
