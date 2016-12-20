// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;

/**
 * Provides functionality needed for content suggestion notifications.
 *
 * Exposes helper functions to native C++ code.
 */
public class ContentSuggestionsNotificationHelper {
    private ContentSuggestionsNotificationHelper() {} // Prevent instantiation

    @CalledByNative
    private static void openUrl(String url) {
        Context context = ContextUtils.getApplicationContext();
        Intent intent = new Intent();
        intent.setAction(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(url));
        intent.setClass(context, ChromeLauncherActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        intent.putExtra(ShortcutHelper.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
        IntentHandler.addTrustedIntentExtras(intent, context);
        context.startActivity(intent);
    }
}
