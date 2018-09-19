// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.text.method.LinkMovementMethod;
import android.view.Gravity;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.survey.SurveyController;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.TabHidingType;
import org.chromium.chrome.browser.util.AccessibilityUtil;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

/**
 * An {@link InfoBar} that prompts the user to take an optional survey.
 */
public class SurveyInfoBar extends InfoBar {
    // The site to pull a survey from.
    private final String mSiteId;

    // Whether to display the survey as a bottom sheet.
    private final boolean mShowAsBottomSheet;

    // The display logo to be shown on the survey and this infobar.
    private final int mDisplayLogoResId;

    // The delegate to handle what happens when info bar events are triggered.
    private final SurveyInfoBarDelegate mDelegate;

    // Boolean to track if the infobar was clicked to prevent double triggering of the survey.
    private boolean mClicked;

    // Boolean to track if the infobar was closed via survey acceptance or onCloseButtonClicked() to
    // prevent onStartHiding() from being called after.
    private boolean mClosedByInteraction;

    /**
     * Create and show the {@link SurveyInfoBar}.
     * @param webContents The webcontents to create the {@link InfoBar} around.
     * @param siteId The id of the site from where the survey will be downloaded.
     * @param surveyInfoBarDelegate The delegate to customize what the infobar will do.
     */
    public static void showSurveyInfoBar(WebContents webContents, String siteId,
            boolean showAsBottomSheet, int displayLogoResId,
            SurveyInfoBarDelegate surveyInfoBarDelegate) {
        nativeCreate(
                webContents, siteId, showAsBottomSheet, displayLogoResId, surveyInfoBarDelegate);
    }

    /**
     * Default constructor.
     * @param siteId The id of the site from where the survey will be downloaded.
     * @param showAsBottomSheet Whether the survey should be presented as a bottom sheet or not.
     * @param displayLogoResId Optional resource id of the logo to be displayed on the survey.
     *                         Pass 0 for no logo.
     * @param surveyInfoBarDelegate The delegate to customize what happens when different events in
     *                              SurveyInfoBar are triggered.
     */
    private SurveyInfoBar(String siteId, boolean showAsBottomSheet, int displayLogoResId,
            SurveyInfoBarDelegate surveyInfoBarDelegate) {
        super(displayLogoResId, null, null);

        mSiteId = siteId;
        mShowAsBottomSheet = showAsBottomSheet;
        mDisplayLogoResId = displayLogoResId;
        mDelegate = surveyInfoBarDelegate;
    }

    @Override
    protected boolean usesCompactLayout() {
        return true;
    }

    @Override
    protected void createCompactLayoutContent(InfoBarCompactLayout layout) {
        Tab tab = nativeGetTab(getNativeInfoBarPtr());
        tab.addObserver(new EmptyTabObserver() {
            @Override
            public void onHidden(Tab tab, @TabHidingType int type) {
                mDelegate.onSurveyInfoBarTabHidden();
                tab.removeObserver(this);

                // Closes the infobar without calling the {@link SurveyInfoBarDelegate}'s
                // onSurveyInfoBarCloseButtonClicked.
                SurveyInfoBar.super.onCloseButtonClicked();
                // TODO(mdjones): add a proper close method to programatically close the infobar.
            }

            @Override
            public void onInteractabilityChanged(boolean isInteractable) {
                mDelegate.onSurveyInfoBarTabInteractabilityChanged(isInteractable);
            }
        });

        NoUnderlineClickableSpan clickableSpan = new NoUnderlineClickableSpan((widget) -> {
            // Prevent double clicking on the text span.
            if (mClicked) return;
            showSurvey(tab);
            mClosedByInteraction = true;
        });

        CharSequence infoBarText = SpanApplier.applySpans(mDelegate.getSurveyPromptString(),
                new SpanInfo("<LINK>", "</LINK>", clickableSpan));

        TextView prompt = new TextView(getContext());
        prompt.setText(infoBarText);
        prompt.setMovementMethod(LinkMovementMethod.getInstance());
        prompt.setGravity(Gravity.CENTER_VERTICAL);
        ApiCompatibilityUtils.setTextAppearance(prompt, R.style.BlackTitle1);
        addAccessibilityClickListener(prompt, tab);
        layout.addContent(prompt, 1f);
    }

    @Override
    public void onCloseButtonClicked() {
        super.onCloseButtonClicked();
        mDelegate.onSurveyInfoBarClosed(true, true);
        mClosedByInteraction = true;
    }

    @CalledByNative
    private static SurveyInfoBar create(String siteId, boolean showAsBottomSheet,
            int displayLogoResId, SurveyInfoBarDelegate surveyInfoBarDelegate) {
        return new SurveyInfoBar(
                siteId, showAsBottomSheet, displayLogoResId, surveyInfoBarDelegate);
    }

    /**
     * Allows the survey infobar to be triggered when talkback is enabled.
     * @param view The view to attach the listener.
     * @param tab The tab to attach the infobar.
     */
    private void addAccessibilityClickListener(TextView view, Tab tab) {
        view.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mClicked || !AccessibilityUtil.isAccessibilityEnabled()) return;
                showSurvey(tab);
                mClosedByInteraction = true;
            }
        });
    }

    @Override
    protected void onStartedHiding() {
        super.onStartedHiding();
        if (mClosedByInteraction) return;
        if (isFrontInfoBar()) {
            mDelegate.onSurveyInfoBarClosed(false, true);
        } else {
            mDelegate.onSurveyInfoBarClosed(false, false);
        }
    }

    /**
     * Shows the survey and closes the infobar.
     * @param tab The tab on which to show the survey.
     */
    private void showSurvey(Tab tab) {
        mClicked = true;
        mDelegate.onSurveyTriggered();

        SurveyController.getInstance().showSurveyIfAvailable(
                tab.getActivity(), mSiteId, mShowAsBottomSheet, mDisplayLogoResId);
        super.onCloseButtonClicked();
    }

    private static native void nativeCreate(WebContents webContents, String siteId,
            boolean showAsBottomSheet, int displayLogoResId,
            SurveyInfoBarDelegate surveyInfoBarDelegate);
    private native Tab nativeGetTab(long nativeSurveyInfoBar);
}
