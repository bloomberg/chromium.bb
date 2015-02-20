// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.shell;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;

import org.chromium.chrome.browser.ServiceTabLauncher;
import org.chromium.ui.WindowOpenDisposition;

/**
 * Service Tab Launcher implementation for ChromeShell. Provides the ability for Chromium to
 * launch tabs from background services, e.g. a Service Worker.
 *
 * This class is used as the Chrome Shell implementation of the ServiceTabLauncher, and is
 * referred to per a meta-data section in the manifest file.
 */
public class ChromeShellServiceTabLauncher extends ServiceTabLauncher {
    @Override
    public void launchTab(Context context, int requestId, boolean incognito, String url,
                          int disposition, String referrerUrl, int referrerPolicy,
                          String extraHeaders, byte[] postData) {
        Intent intent = new Intent(context, ChromeShellActivity.class);
        intent.setAction(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setData(Uri.parse(url));

        switch (disposition) {
            case WindowOpenDisposition.NEW_WINDOW:
            case WindowOpenDisposition.NEW_POPUP:
            case WindowOpenDisposition.NEW_FOREGROUND_TAB:
            case WindowOpenDisposition.NEW_BACKGROUND_TAB:
                // The browser should attempt to create a new tab.
                intent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
                break;
            default:
                // The browser should attempt to re-use an existing tab.
                break;
        }

        intent.putExtra(ServiceTabLauncher.LAUNCH_REQUEST_ID_EXTRA, requestId);

        // TODO(peter): Support |incognito| when ChromeShell supports that.
        // TODO(peter): Support the referrer information, extra headers and post data if
        // ChromeShell gets support for those properties from intent extras.

        context.startActivity(intent);
    }
}
