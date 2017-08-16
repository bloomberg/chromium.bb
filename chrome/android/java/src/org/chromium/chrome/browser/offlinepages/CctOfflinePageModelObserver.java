// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.content.Context;
import android.content.Intent;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.AppHooks;

/**
 * Java-side handler for Offline page model changes.
 *
 * Will send a broadcast intent to the originating app if a page related to it has changed
 * and the app is part of the whitelist set of apps.
 */
public class CctOfflinePageModelObserver {
    private static final String TAG = "CctModelObserver";
    private static final String ACTION_OFFLINE_PAGES_UPDATED_SUFFIX = ".OFFLINE_PAGES_CHANGED";

    @CalledByNative
    private static void onPageChanged(String originString) {
        OfflinePageOrigin origin = new OfflinePageOrigin(originString);
        if (!origin.isChrome()) compareSignaturesAndFireIntent(origin);
    }

    private static void compareSignaturesAndFireIntent(OfflinePageOrigin origin) {
        if (!isInWhitelist(origin.getAppName())) {
            Log.w(TAG, "Non-whitelisted app");
            return;
        }
        Context context = ContextUtils.getApplicationContext();
        if (!origin.doesSignatureMatch(context)) {
            Log.w(TAG, "Signature hashes are different");
            return;
        }
        // Create broadcast if signatures match.
        Intent intent = new Intent();
        intent.setAction(ACTION_OFFLINE_PAGES_UPDATED_SUFFIX);
        intent.setPackage(origin.getAppName());
        context.sendBroadcast(intent);
    }

    private static boolean isInWhitelist(String appName) {
        return AppHooks.get().getOfflinePagesCctWhitelist().contains(appName);
    }
}
