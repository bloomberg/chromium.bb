// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import android.app.Instrumentation;
import android.content.Context;
import android.support.test.InstrumentationRegistry;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.test.MultiActivityTestCommon.MultiActivityTestCommonCallback;
import org.chromium.chrome.test.util.browser.tabmodel.document.MockStorageDelegate;

/** Custom TestRule for MultiActivity Tests. */
public class MultiActivityTestRule implements TestRule, MultiActivityTestCommonCallback {
    private final MultiActivityTestCommon mTestCommon;

    public MultiActivityTestRule() {
        mTestCommon = new MultiActivityTestCommon(this);
    }

    public MockStorageDelegate getStorageDelegate() {
        return mTestCommon.mStorageDelegate;
    }

    public Context getContext() {
        return mTestCommon.mContext;
    }

    public void waitForFullLoad(final ChromeActivity activity, final String expectedTitle) {
        mTestCommon.waitForFullLoad(activity, expectedTitle);
    }

    public void waitForFullLoad(
            final ChromeActivity activity, final String expectedTitle, boolean waitLongerForLoad) {
        mTestCommon.waitForFullLoad(activity, expectedTitle, waitLongerForLoad);
    }

    @Override
    public Statement apply(final Statement base, Description desc) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                mTestCommon.setUp();
                base.evaluate();
                mTestCommon.tearDown();
            }
        };
    }

    @Override
    public Instrumentation getInstrumentation() {
        return InstrumentationRegistry.getInstrumentation();
    }
}
