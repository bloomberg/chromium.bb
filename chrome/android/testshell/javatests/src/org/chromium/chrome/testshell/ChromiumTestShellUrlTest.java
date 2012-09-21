// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.testshell;

import org.chromium.base.test.DisabledTest;

public class ChromiumTestShellUrlTest extends ChromiumTestShellTestBase {
    // URL used for base tests.
    private static final String URL = "data:text";

    @DisabledTest
    public void testBaseStartup() throws Exception {
        ChromiumTestShellActivity activity = launchChromiumTestShellWithUrl(URL);

        // Make sure the activity was created as expected.
        assertNotNull(activity);
    }
}