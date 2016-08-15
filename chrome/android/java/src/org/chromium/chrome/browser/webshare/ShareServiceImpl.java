// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webshare;

import android.content.Context;
import android.content.Intent;
import android.support.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content_public.browser.WebContents;
import org.chromium.mojo.system.MojoException;
import org.chromium.mojom.webshare.ShareService;
import org.chromium.ui.base.WindowAndroid;

/**
 * Android implementation of the ShareService service defined in
 * third_party/WebKit/public/platform/modules/webshare/webshare.mojom.
 */
public class ShareServiceImpl implements ShareService {
    private final WindowAndroid mWindow;
    private final Context mContext;

    public ShareServiceImpl(@Nullable WebContents webContents) {
        mWindow = windowFromWebContents(webContents);
        mContext = contextFromWindow(mWindow);
    }

    @Override
    public void close() {}

    @Override
    public void onConnectionError(MojoException e) {}

    @Override
    public void share(String title, String text, ShareResponse callback) {
        if (mContext == null) {
            callback.call("Share failed");
            return;
        }

        String chooserTitle = mContext.getString(R.string.share_link_chooser_title);
        Intent send = new Intent(Intent.ACTION_SEND);
        send.setType("text/plain");
        send.putExtra(Intent.EXTRA_SUBJECT, title);
        send.putExtra(Intent.EXTRA_TEXT, text);

        Intent chooser = Intent.createChooser(send, chooserTitle);
        if (!mWindow.showIntent(chooser, null, null)) {
            callback.call("Share failed");
            return;
        }

        // Success.
        // TODO(mgiuca): Wait until the user has made a choice, and report failure if they cancel
        // the picker or something else goes wrong.
        callback.call(null);
    }

    @Nullable
    private static WindowAndroid windowFromWebContents(@Nullable WebContents webContents) {
        if (webContents == null) return null;

        ContentViewCore contentViewCore = ContentViewCore.fromWebContents(webContents);
        if (contentViewCore == null) return null;

        return contentViewCore.getWindowAndroid();
    }

    @Nullable
    private static Context contextFromWindow(@Nullable WindowAndroid window) {
        if (window == null) return null;

        return window.getActivity().get();
    }
}
