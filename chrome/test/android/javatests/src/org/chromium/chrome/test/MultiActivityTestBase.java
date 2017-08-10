// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import android.content.Context;
import android.test.InstrumentationTestCase;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.util.browser.tabmodel.document.MockStorageDelegate;

import java.util.concurrent.TimeoutException;

/**
 * Base for testing and interacting with multiple Activities (e.g. Document or Webapp Activities).
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public abstract class MultiActivityTestBase extends InstrumentationTestCase
        implements MultiActivityTestCommon.MultiActivityTestCommonCallback {
    private final MultiActivityTestCommon mTestCommon;

    public MultiActivityTestBase() {
        mTestCommon = new MultiActivityTestCommon(this);
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mTestCommon.setUp();
    }

    @Override
    public void tearDown() throws Exception {
        super.tearDown();
        mTestCommon.tearDown();
    }

    public Context getContext() {
        return mTestCommon.mContext;
    }

    public MockStorageDelegate getStorageDelegate() {
        return mTestCommon.mStorageDelegate;
    }

    /**
     * See {@link #waitForFullLoad(ChromeActivity,String,boolean)}.
     */
    protected void waitForFullLoad(final ChromeActivity activity, final String expectedTitle)
            throws InterruptedException, TimeoutException {
        mTestCommon.waitForFullLoad(activity, expectedTitle);
    }
}
