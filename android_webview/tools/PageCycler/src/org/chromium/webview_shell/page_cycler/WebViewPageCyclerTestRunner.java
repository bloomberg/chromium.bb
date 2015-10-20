// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell.page_cycler;

import android.test.AndroidTestRunner;
import android.test.InstrumentationTestRunner;

import org.chromium.test.reporter.TestStatusListener;

/**
 * Customized test runner for running instrumentation tests in WebViewBrowserTests.
 */
public class WebViewPageCyclerTestRunner extends InstrumentationTestRunner {

    @Override
    protected AndroidTestRunner getAndroidTestRunner() {
        AndroidTestRunner runner = super.getAndroidTestRunner();
        runner.addTestListener(new TestStatusListener(getContext()));
        return runner;
    }
}
