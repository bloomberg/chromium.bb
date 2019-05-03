// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.app.Activity;
import android.graphics.Bitmap;

import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.webapps.AddToHomescreenDialog;

/**
 * Add to homescreen manager specifically for touchless devices.
 */
class TouchlessAddToHomescreenManager implements AddToHomescreenDialog.Delegate {
    protected final Activity mActivity;
    private final String mUrl;
    private final String mTitle;
    private final Bitmap mIconBitmap;

    protected AddToHomescreenDialog mDialog;

    public TouchlessAddToHomescreenManager(
            Activity activity, String url, String title, Bitmap iconBitmap) {
        mActivity = activity;
        mUrl = url;
        mTitle = title;
        mIconBitmap = iconBitmap;
    }

    // Starts the process of showing the dialog and adding the shortcut.
    public void start() {
        mDialog = new AddToHomescreenDialog(mActivity, this);
        mDialog.show();
        mDialog.onUserTitleAvailable(mTitle, mUrl, false);
        mDialog.onIconAvailable(mIconBitmap);
    }

    @Override
    public void addToHomescreen(String title) {
        ShortcutHelper.addShortcut(mUrl, mUrl, title, mIconBitmap, false, 0);
    }

    @Override
    public void onNativeAppDetailsRequested() {
        return;
    }

    @Override
    public void onDialogDismissed() {
        mDialog = null;
    }
}
