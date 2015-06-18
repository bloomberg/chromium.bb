// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;

import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.document.DocumentMetricIds;
import org.chromium.chrome.browser.document.PendingDocumentData;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.components.service_tab_launcher.ServiceTabLauncher;
import org.chromium.content_public.common.Referrer;
import org.chromium.ui.base.PageTransition;

/**
 * Service Tab Launcher implementation for Chrome. Provides the ability for Android Services
 * running in Chrome to launch tabs, without having access to an activity.
 *
 * This class is referred to from the ServiceTabLauncher implementation in Chromium using a
 * meta-data value in the Android manifest file. The ServiceTabLauncher class has more
 * documentation about why this is necessary.
 *
 * TODO(peter): after upstreaming, merge this with ServiceTabLauncher and remove reflection calls
 *              in ServiceTabLauncher.
 */
public class ChromeServiceTabLauncher extends ServiceTabLauncher {
    @Override
    public void launchTab(Context context, int requestId, boolean incognito, String url,
                          int disposition, String referrerUrl, int referrerPolicy,
                          String extraHeaders, byte[] postData) {
        // TODO(peter): Determine the intent source based on the |disposition| with which the
        // tab is being launched. Right now this is gated by a check in the native implementation.
        int intentSource = DocumentMetricIds.STARTED_BY_WINDOW_OPEN;

        if (FeatureUtilities.isDocumentMode(context)) {
            PendingDocumentData data = new PendingDocumentData();
            data.url = url;
            data.postData = postData;
            data.extraHeaders = extraHeaders;
            data.referrer = new Referrer(referrerUrl, referrerPolicy);
            data.requestId = requestId;

            ChromeLauncherActivity.launchDocumentInstance(null /* activity */, incognito,
                    ChromeLauncherActivity.LAUNCH_MODE_FOREGROUND, url, intentSource,
                    PageTransition.LINK, data);
            return;
        }

        Intent intent = new Intent(context, ChromeLauncherActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setAction(Intent.ACTION_VIEW);

        intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, incognito);
        intent.putExtra(IntentHandler.EXTRA_PAGE_TRANSITION_TYPE, PageTransition.LINK);
        intent.putExtra(IntentHandler.EXTRA_STARTED_BY, intentSource);
        intent.putExtra(IntentHandler.EXTRA_USE_DESKTOP_USER_AGENT, false);

        intent.putExtra(ServiceTabLauncher.LAUNCH_REQUEST_ID_EXTRA, requestId);

        // TODO(peter): We should include the referrer, extra headers and the post data in the
        // new tab request where supported by the tabbed activity.

        intent.setData(Uri.parse(url));

        IntentHandler.addTrustedIntentExtras(intent, context);

        context.startActivity(intent);
    }
}
