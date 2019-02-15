// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha.inline;

import com.google.android.play.core.appupdate.AppUpdateManager;
import com.google.android.play.core.appupdate.AppUpdateManagerFactory;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.omaha.inline.FakeAppUpdateManagerWrapper.Type;

/** A factory that creates an {@link AppUpdateManager} instance. */
public class InlineAppUpdateManagerFactory {
    private static final boolean sTest = true;

    /** @return A new {@link AppUpdateManager} to use to interact with Play for inline updates. */
    public static AppUpdateManager create() {
        // TODO(me): Check finch configuration to figure out what kind of test scenario to run.
        if (sTest) {
            return new FakeAppUpdateManagerWrapper(Type.NONE);
        } else {
            return AppUpdateManagerFactory.create(ContextUtils.getApplicationContext());
        }
    }
}