// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell.util;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.vr_shell.TestVrShellDelegate;

import java.util.concurrent.atomic.AtomicReference;

/**
 * Class containing utility functions for interacting with VrShellDelegate
 * during tests.
 */
public class VrShellDelegateUtils {
    /**
     * Retrieves the current VrShellDelegate instance from the UI thread.
     * This is necessary in case acquiring the instance causes the delegate
     * to be constructed, which must happen on the UI thread.
     */
    public static TestVrShellDelegate getDelegateInstance() {
        final AtomicReference<TestVrShellDelegate> delegate =
                new AtomicReference<TestVrShellDelegate>();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                delegate.set(TestVrShellDelegate.getInstance());
            }
        });
        return delegate.get();
    }
}
