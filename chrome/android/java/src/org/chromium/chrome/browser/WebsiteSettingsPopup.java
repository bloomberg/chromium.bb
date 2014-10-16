// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.style.ForegroundColorSpan;
import android.text.style.StyleSpan;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import org.chromium.base.CalledByNative;
import org.chromium.base.CommandLine;
import org.chromium.chrome.ChromeSwitches;
import org.chromium.chrome.R;
import org.chromium.content.browser.WebContentsObserver;
import org.chromium.content_public.browser.WebContents;

/**
 * Java side of Android implementation of the website settings UI.
 */
public class WebsiteSettingsPopup {
    private final Context mContext;
    private final WebContents mWebContents;

    // A pointer to the C++ object for this UI.
    private final long mNativeWebsiteSettingsPopup;

    // The outer container, filled with the layout from website_settings.xml.
    private final LinearLayout mContainer;

    // UI elements in the dialog.
    private final TextView mUrlTitle;
    private final TextView mUrlConnectionMessage;

    // The dialog the container is placed in.
    private final Dialog mDialog;

    /**
     * Creates the WebsiteSettingsPopup, but does not display it. Also
     * initializes the corresponding C++ object and saves a pointer to it.
     */
    private WebsiteSettingsPopup(Context context, WebContents webContents) {
        mContext = context;
        mWebContents = webContents;

        // Find the container and all it's important subviews.
        mContainer = (LinearLayout) LayoutInflater.from(mContext).inflate(
                R.layout.website_settings, null);

        mUrlTitle = (TextView) mContainer
                .findViewById(R.id.website_settings_url);
        mUrlConnectionMessage = (TextView) mContainer
                .findViewById(R.id.website_settings_permission_message);

        // Create the dialog.
        mDialog = new Dialog(mContext);
        mDialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        mDialog.setCanceledOnTouchOutside(true);

        // Place the dialog at the top of the screen, and remove its border.
        Window window = mDialog.getWindow();
        window.setGravity(Gravity.TOP);
        window.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));

        // This needs to come after other member initialization.
        mNativeWebsiteSettingsPopup = nativeInit(this, webContents);
        final WebContentsObserver webContentsObserver = new WebContentsObserver(mWebContents) {
            @Override
            public void navigationEntryCommitted() {
                // If a navigation is committed (e.g. from in-page redirect), the data we're
                // showing is stale so dismiss the dialog.
                mDialog.dismiss();
            }
        };
        mDialog.setOnDismissListener(new DialogInterface.OnDismissListener() {
            @Override
            public void onDismiss(DialogInterface dialog) {
                assert mNativeWebsiteSettingsPopup != 0;
                webContentsObserver.detachFromWebContents();
                nativeDestroy(mNativeWebsiteSettingsPopup);
            }
        });
    }

    /**
     * Sets the URL in the title to: (scheme)://(domain)(path). Also colors
     * different parts of the URL depending on connectionType.
     * connectionType should be a valid PageInfoConnectionType.
     */
    @CalledByNative
    private void setURLTitle(String scheme, String domain, String path, int connectionType) {
        boolean makeDomainBold = false;
        int schemeColorId = R.color.website_settings_popup_url_scheme_broken;
        switch (connectionType) {
            case PageInfoConnectionType.CONNECTION_UNKNOWN:
                schemeColorId = R.color.website_settings_popup_url_scheme_http;
                makeDomainBold = true;
                break;
            case PageInfoConnectionType.CONNECTION_ENCRYPTED:
                schemeColorId = R.color.website_settings_popup_url_scheme_https;
                break;
            case PageInfoConnectionType.CONNECTION_MIXED_CONTENT:
                schemeColorId = R.color.website_settings_popup_url_scheme_mixed;
                makeDomainBold = true;
                break;
            case PageInfoConnectionType.CONNECTION_UNENCRYPTED:
                schemeColorId = R.color.website_settings_popup_url_scheme_http;
                makeDomainBold = true;
                break;
            case PageInfoConnectionType.CONNECTION_ENCRYPTED_ERROR:
                schemeColorId = R.color.website_settings_popup_url_scheme_broken;
                makeDomainBold = true;
                break;
            case PageInfoConnectionType.CONNECTION_INTERNAL_PAGE:
                schemeColorId = R.color.website_settings_popup_url_scheme_http;
                break;
            default:
                assert false : "Unexpected connection type: " + connectionType;
        }

        SpannableStringBuilder sb = new SpannableStringBuilder(scheme + "://" + domain + path);

        ForegroundColorSpan schemeColorSpan = new ForegroundColorSpan(
                mContext.getResources().getColor(schemeColorId));
        sb.setSpan(schemeColorSpan, 0, scheme.length(), Spannable.SPAN_INCLUSIVE_INCLUSIVE);

        int domainStartIndex = scheme.length() + 3;
        ForegroundColorSpan domainColorSpan = new ForegroundColorSpan(
                mContext.getResources().getColor(R.color.website_settings_popup_url_domain));
        sb.setSpan(domainColorSpan, domainStartIndex, domainStartIndex + domain.length(),
                Spannable.SPAN_INCLUSIVE_INCLUSIVE);

        if (makeDomainBold) {
            StyleSpan boldStyleSpan = new StyleSpan(android.graphics.Typeface.BOLD);
            sb.setSpan(boldStyleSpan, domainStartIndex, domainStartIndex + domain.length(),
                    Spannable.SPAN_INCLUSIVE_INCLUSIVE);
        }

        mUrlTitle.setText(sb);
    }

    /**
     * Sets the connection message displayed at the top of the dialog to
     * connectionMessage (e.g. "Could not securely connect to this site").
     */
    @CalledByNative
    private void setConnectionMessage(String connectionMessage) {
        mUrlConnectionMessage.setText(connectionMessage);
    }

    /** Displays the WebsiteSettingsPopup. */
    @CalledByNative
    private void showDialog() {
        // Wrap the dialog in a ScrollView in case the content is too long.
        ScrollView scrollView = new ScrollView(mContext);
        scrollView.addView(mContainer);
        mDialog.addContentView(scrollView, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.MATCH_PARENT));

        // Make the dialog fill the width of the screen. This must be called
        // after addContentView, or it won't fully fill to the edge.
        Window window = mDialog.getWindow();
        window.setLayout(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT);

        mDialog.show();
    }

    /**
     * Shows a WebsiteSettings dialog for the provided WebContents. The popup
     * adds itself to the view hierarchy which owns the reference while it's
     * visible.
     *
     * @param context Context which is used for launching a dialog.
     * @param webContents The WebContents for which to show Website information.
     *                    This information is retrieved for the visible entry.
     */
    @SuppressWarnings("unused")
    public static void show(Context context, WebContents webContents) {
        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.ENABLE_NEW_WEBSITE_SETTINGS)) {
            new WebsiteSettingsPopup(context, webContents);
        } else {
            WebsiteSettingsPopupLegacy.show(context, webContents);
        }
    }

    private static native long nativeInit(WebsiteSettingsPopup popup,
            WebContents webContents);

    private native void nativeDestroy(long nativeWebsiteSettingsPopupAndroid);
}
