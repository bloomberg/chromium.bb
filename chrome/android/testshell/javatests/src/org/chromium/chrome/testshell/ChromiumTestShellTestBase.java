// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.testshell;

import android.content.ComponentName;
import android.content.Intent;
import android.net.Uri;
import android.test.ActivityInstrumentationTestCase2;

/**
 * Base test class for all ChromiumTestShell based tests.
 */
public class ChromiumTestShellTestBase extends
        ActivityInstrumentationTestCase2<ChromiumTestShellActivity> {

    public ChromiumTestShellTestBase() {
        super(ChromiumTestShellActivity.class);
    }

    /**
     * Starts the ChromiumTestShell activity and loads the given URL.
     */
    protected ChromiumTestShellActivity launchChromiumTestShellWithUrl(String url) {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setData(Uri.parse(url));
        intent.setComponent(new ComponentName(getInstrumentation().getTargetContext(),
                ChromiumTestShellActivity.class));
        setActivityIntent(intent);
        return getActivity();
    }
}
