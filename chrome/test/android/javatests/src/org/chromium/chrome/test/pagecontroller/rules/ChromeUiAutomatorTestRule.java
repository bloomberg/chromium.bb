// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.rules;

import android.os.Environment;

import org.junit.rules.TestWatcher;
import org.junit.runner.Description;

import org.chromium.base.Log;
import org.chromium.chrome.test.pagecontroller.utils.UiAutomatorUtils;

import java.io.File;

/**
 * Custom Rule that logs useful information for debugging UiAutomator
 * related issues in the event of a test failure.
 */
public class ChromeUiAutomatorTestRule extends TestWatcher {
    private static final String TAG = "ChromeUiAutomatorTR";

    @Override
    protected void failed(Throwable e, Description description) {
        super.failed(e, description);
        e.printStackTrace();
        UiAutomatorUtils utils = UiAutomatorUtils.getInstance();
        Log.d(TAG, e.getMessage());
        String className = description.getClassName();
        String testName = description.getMethodName();
        File logDir = Environment.getExternalStorageDirectory();
        String filePrefix = className + "_" + testName;

        // TODO(aluo): Get ScreenshotOnFailureStatement in the test runner to work.
        File screenFile = new File(logDir, filePrefix + "_failure_screen.png");
        utils.printWindowHierarchy("Hierarchy for " + screenFile.getAbsolutePath());
        utils.takeScreenShot(screenFile);
        Log.d(TAG, "Screen shot has been saved to " + screenFile.getAbsolutePath());
    }
}
