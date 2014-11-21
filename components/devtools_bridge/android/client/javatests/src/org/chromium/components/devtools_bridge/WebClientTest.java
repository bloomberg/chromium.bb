// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.devtools_bridge;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content.browser.BrowserStartupController;

import java.util.concurrent.Callable;

/**
 * Tests for {@link WebClient}. WebClient is not intended to run on Android but
 * it can. It is useful for tests: we can test it against the server in the
 * same process (without dependency on network and cloud services).
 */
public class WebClientTest extends InstrumentationTestCase {
    private Profile mProfile;

    protected void startChromeBrowserProcessSyncOnUIThread() throws Exception {
        BrowserStartupController.get(getInstrumentation().getTargetContext())
                .startBrowserProcessesSync(false);
        mProfile = Profile.getLastUsedProfile();
    }

    @SmallTest
    public void testCreationWebClient() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Callable<Void>() {
            @Override
            public Void call() throws Exception {
                startChromeBrowserProcessSyncOnUIThread();
                assert mProfile != null;

                new WebClient(mProfile).dispose();
                return null;
            }
        });
    }
}
