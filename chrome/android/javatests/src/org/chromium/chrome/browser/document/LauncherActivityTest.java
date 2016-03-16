// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.document;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.concurrent.Callable;

/**
 * Tests for launching Chrome.
 */
public class LauncherActivityTest extends ChromeActivityTestCaseBase<ChromeActivity> {

    private Context mContext;

    public LauncherActivityTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mContext = getInstrumentation().getTargetContext();
    }

    @SmallTest
    public void testLaunchWithUrlNoScheme() throws Exception {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("www.google.com"));
        intent.setClassName(mContext.getPackageName(), ChromeLauncherActivity.class.getName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mContext.startActivity(intent);

        // Check that Chrome launched successfully
        CriteriaHelper.pollInstrumentationThread(
                Criteria.equals(ApplicationState.HAS_RUNNING_ACTIVITIES, new Callable<Integer>() {
                    @Override
                    public Integer call() {
                        return ApplicationStatus.getStateForApplication();
                    }
                }));
    }

    @Override
    public void startMainActivity() throws InterruptedException {
    }

}
