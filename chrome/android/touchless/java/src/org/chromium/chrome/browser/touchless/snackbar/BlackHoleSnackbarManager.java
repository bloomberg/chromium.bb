// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless.snackbar;

import android.app.Activity;
import android.view.ViewGroup;

import org.chromium.chrome.browser.snackbar.Snackbar;
import org.chromium.chrome.browser.snackbar.SnackbarManager;

/** A snackbar manager that consumes all incoming requests. */
public class BlackHoleSnackbarManager extends SnackbarManager {
    /**
     * @param activity The embedding activity.
     */
    public BlackHoleSnackbarManager(Activity activity) {
        super(activity, new ViewGroup(activity) {
            @Override
            protected void onLayout(boolean changed, int left, int top, int right, int bottom) {}
        });
    }

    @Override
    public void showSnackbar(Snackbar snackbar) {
        // Intentional noop.
    }
}
