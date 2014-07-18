// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.content.Context;
import android.content.Intent;
import android.preference.PreferenceActivity;

import org.chromium.content_public.browser.WebContents;

/**
 * Generates an InfoBar for the data reduction proxy that contains a message and a link to the
 * data reduction proxy settings menu.
 */
public class DataReductionProxyInfoBar extends ConfirmInfoBar {
    private static String sSettingsClassName;
    private static String sDataReductionProxySettingsClassName;
    private static String sTitle;
    private static String sLinkText;

    /**
     * Launch a data reduction proxy {@link InfoBar} with the specified title and link
     * text. Clicking the link will open the specified settings page.
     * @param webContents The {@link WebContents} in which to open the {@link InfoBar}.
     * @param settingsClassName. The settings class to open.
     * @param drpSettingsClassName The {@link PreferenceActivity} fragment to show.
     * @param title The text to display in the {@link InfoBar}.
     * @param linkText The text to display in the link in the {@link InfoBar}.
     */
    public static void launch(WebContents webContents,
            String settingsClassName,
            String drpSettingsClassName,
            String title,
            String linkText) {
        sSettingsClassName = settingsClassName;
        sDataReductionProxySettingsClassName = drpSettingsClassName;
        sTitle = title;
        sLinkText = linkText;
        DataReductionProxyInfoBarDelegate.launch(webContents);
    }

    DataReductionProxyInfoBar(long nativeInfoBar, int iconDrawableId) {
        super(nativeInfoBar, null, iconDrawableId, sTitle,
                sLinkText, null, null);
    }

    @Override
    public void onLinkClicked() {
        Context context = getContext();
        if (context == null) return;
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setClassName(context, sSettingsClassName);
        intent.setFlags(
                Intent.FLAG_ACTIVITY_REORDER_TO_FRONT | Intent.FLAG_ACTIVITY_SINGLE_TOP);
        intent.putExtra(PreferenceActivity.EXTRA_SHOW_FRAGMENT,
                sDataReductionProxySettingsClassName);
        context.startActivity(intent);
    }
}
