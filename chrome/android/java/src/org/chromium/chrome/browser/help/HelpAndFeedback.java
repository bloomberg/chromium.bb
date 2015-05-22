// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.help;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
import android.provider.Browser;
import android.text.TextUtils;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.chrome.browser.ChromiumApplication;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.UrlUtilities;

/**
 * Launches an activity that displays a relevant support page and has an option to provide feedback.
 */
public class HelpAndFeedback {

    // These constants should be in sync with the context names on go/mobilehelprecs.
    // If the context ID cannot be found, the default help page will be shown to the user.
    public static final String CONTEXT_NEW_TAB = "mobile_new_tab";
    public static final String CONTEXT_SEARCH_RESULTS = "mobile_search_results";
    public static final String CONTEXT_WEBPAGE = "mobile_webpage";
    public static final String CONTEXT_SETTINGS = "mobile_settings";
    public static final String CONTEXT_INCOGNITO = "mobile_incognito";
    public static final String CONTEXT_BOOKMARKS = "mobile_bookmarks";
    public static final String CONTEXT_HISTORY = "mobile_history";
    public static final String CONTEXT_ERROR = "mobile_error";  // not used for now
    public static final String CONTEXT_PRIVACY = "mobile_privacy";  // not added for now
    public static final String CONTEXT_TRANSLATE = "mobile_translate";  // not added for now
    public static final String CONTEXT_GENERAL = "mobile_general";

    protected static final String FALLBACK_SUPPORT_URL =
            "https://support.google.com/chrome/topic/6069782";

    private static HelpAndFeedback sInstance;

    /**
     * Returns the singleton instance of HelpAndFeedback, creating it if needed.
     */
    @SuppressFBWarnings("LI_LAZY_INIT_STATIC")
    public static HelpAndFeedback getInstance(Context context) {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = ((ChromiumApplication) context.getApplicationContext())
                    .createHelpAndFeedback();
        }
        return sInstance;
    }

    /**
     * Starts an activity showing a help page for the specified context ID.
     *
     * @param activity The activity to use for starting the help activity and to take a
     *                 screenshot of.
     * @param helpContext One of the CONTEXT_* constants. This should describe the user's current
     *                    context and will be used to show a more relevant help page.
     * @param screenshot A screenshot of the current activity to include in the feedback the
     *                   user sends, if any. If null, this method will take a screenshot of the
     *                   activity (which will show a rendered page as black).
     * @param url The URL of the current tab to include in the feedback the user sends, if any.
     *            This parameter can be null.
     */
    public void show(Activity activity, String helpContext, Bitmap screenshot, String url) {
        launchFallbackSupportUri(activity);
    }

    /**
     * Get help context ID from URL.
     *
     * @param url The URL to be checked.
     * @param isIncognito Whether we are in incognito mode or not.
     * @return Help context ID that matches the URL and incognito mode.
     */
    public static String getHelpContextIdFromUrl(String url, boolean isIncognito) {
        if (TextUtils.isEmpty(url)) {
            return CONTEXT_GENERAL;
        } else if (url.startsWith(UrlConstants.BOOKMARKS_URL)) {
            return CONTEXT_BOOKMARKS;
        } else if (url.equals(UrlConstants.HISTORY_URL)) {
            return CONTEXT_HISTORY;
        // Note: For www.google.com the following function returns false.
        } else if (UrlUtilities.nativeIsGoogleSearchUrl(url)) {
            return CONTEXT_SEARCH_RESULTS;
        // For incognito NTP, we want to show incognito help.
        } else if (isIncognito) {
            return CONTEXT_INCOGNITO;
        } else if (url.equals(UrlConstants.NTP_URL)) {
            return CONTEXT_NEW_TAB;
        }
        return CONTEXT_WEBPAGE;
    }

    protected static void launchFallbackSupportUri(Context context) {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(FALLBACK_SUPPORT_URL));
        // Let Chrome know that this intent is from Chrome, so that it does not close the app when
        // the user presses 'back' button.
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        intent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
        intent.setPackage(context.getPackageName());
        context.startActivity(intent);
    }
}