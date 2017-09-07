// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.app.Activity;
import android.content.res.Resources;
import android.view.Gravity;
import android.view.View;
import android.view.ViewTreeObserver.OnScrollChangedListener;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;
import android.widget.ScrollView;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.FadingShadow;
import org.chromium.chrome.browser.widget.FadingShadowView;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.BottomSheetContent;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetContentController;

/**
 * Provides content to be displayed inside the Home tab of the bottom sheet in incognito mode.
 */
public class IncognitoBottomSheetContent extends IncognitoNewTabPage implements BottomSheetContent {
    private final ScrollView mScrollView;

    /**
     * Constructs a new IncognitoBottomSheetContent.
     * @param activity The {@link Activity} displaying this bottom sheet content.
     */
    public IncognitoBottomSheetContent(final Activity activity) {
        super(activity);

        final FadingShadowView shadow =
                (FadingShadowView) mIncognitoNewTabPageView.findViewById(R.id.shadow);
        shadow.init(ApiCompatibilityUtils.getColor(
                            mIncognitoNewTabPageView.getResources(), R.color.toolbar_shadow_color),
                FadingShadow.POSITION_TOP);

        initTextViews();

        // Hide the incognito image from the Chrome Home NTP pages.
        ImageView incognitoSplash =
                (ImageView) mIncognitoNewTabPageView.findViewById(R.id.new_tab_incognito_icon);
        incognitoSplash.setVisibility(View.GONE);

        mScrollView = (ScrollView) mIncognitoNewTabPageView.findViewById(R.id.ntp_scrollview);
        mScrollView.setBackgroundColor(ApiCompatibilityUtils.getColor(
                mIncognitoNewTabPageView.getResources(), R.color.incognito_primary_color));
        // Remove some additional padding that's not necessary when using Chrome Home.
        mScrollView.setPaddingRelative(ApiCompatibilityUtils.getPaddingStart(mScrollView),
                mScrollView.getPaddingTop()
                        - ((int) activity.getResources().getDimension(
                                  R.dimen.toolbar_height_no_shadow)),
                ApiCompatibilityUtils.getPaddingEnd(mScrollView), mScrollView.getPaddingBottom());

        mScrollView.getViewTreeObserver().addOnScrollChangedListener(new OnScrollChangedListener() {
            @Override
            public void onScrollChanged() {
                boolean shadowVisible = mScrollView.canScrollVertically(-1);
                shadow.setVisibility(shadowVisible ? View.VISIBLE : View.GONE);
            }
        });
    }

    private void initTextViews() {
        final int locationBarLightHintTextColor = ApiCompatibilityUtils.getColor(
                mIncognitoNewTabPageView.getResources(), R.color.locationbar_light_hint_text);
        final int googleBlueColor = ApiCompatibilityUtils.getColor(
                mIncognitoNewTabPageView.getResources(), R.color.google_blue_300);

        final TextView learnMoreView =
                (TextView) mIncognitoNewTabPageView.findViewById(R.id.learn_more);

        learnMoreView.setTextColor(googleBlueColor);

        if (useMDIncognitoNTP()) {
            final TextView newTabIncognitoTitleView =
                    (TextView) mIncognitoNewTabPageView.findViewById(R.id.new_tab_incognito_title);
            final TextView newTabIncognitoSubtitleView =
                    (TextView) mIncognitoNewTabPageView.findViewById(
                            R.id.new_tab_incognito_subtitle);
            final TextView newTabIncognitoFeaturesView =
                    (TextView) mIncognitoNewTabPageView.findViewById(
                            R.id.new_tab_incognito_features);
            final TextView newTabIncognitoWarningView =
                    (TextView) mIncognitoNewTabPageView.findViewById(
                            R.id.new_tab_incognito_warning);

            newTabIncognitoTitleView.setTextColor(locationBarLightHintTextColor);
            newTabIncognitoSubtitleView.setTextColor(locationBarLightHintTextColor);
            newTabIncognitoFeaturesView.setTextColor(locationBarLightHintTextColor);
            newTabIncognitoWarningView.setTextColor(locationBarLightHintTextColor);
        } else {
            final TextView incognitoNtpHeaderView =
                    (TextView) mIncognitoNewTabPageView.findViewById(R.id.ntp_incognito_header);
            final TextView newTabIncognitoMessageView =
                    (TextView) mIncognitoNewTabPageView.findViewById(
                            R.id.new_tab_incognito_message);
            final LinearLayout newTabIncognitoContainer =
                    (LinearLayout) mIncognitoNewTabPageView.findViewById(
                            R.id.new_tab_incognito_container);

            incognitoNtpHeaderView.setTextColor(locationBarLightHintTextColor);

            Resources resources = getContentView().getResources();
            newTabIncognitoMessageView.setTextColor(locationBarLightHintTextColor);
            newTabIncognitoMessageView.setLineSpacing(
                    resources.getDimensionPixelSize(R.dimen.md_incognito_ntp_line_spacing), 1.0f);
            LayoutParams layoutParams = (LayoutParams) newTabIncognitoMessageView.getLayoutParams();
            layoutParams.bottomMargin = resources.getDimensionPixelSize(
                    R.dimen.chrome_home_incognito_ntp_bottom_margin);

            newTabIncognitoContainer.setPadding(
                    resources.getDimensionPixelSize(R.dimen.md_incognito_ntp_padding_left),
                    newTabIncognitoContainer.getPaddingTop(),
                    newTabIncognitoContainer.getPaddingRight(),
                    newTabIncognitoContainer.getPaddingBottom());
            newTabIncognitoContainer.setGravity(Gravity.START);

            learnMoreView.setPadding(0, 0, 0, 0);
            learnMoreView.setAllCaps(false);
        }
    }

    @Override
    protected int getLayoutResource() {
        return useMDIncognitoNTP() ? R.layout.incognito_bottom_sheet_content_md
                                   : R.layout.incognito_bottom_sheet_content;
    }

    @Override
    public View getContentView() {
        return getView();
    }

    @Override
    public View getToolbarView() {
        return null;
    }

    @Override
    public boolean isUsingLightToolbarTheme() {
        return false;
    }

    @Override
    public boolean isIncognitoThemedContent() {
        return true;
    }

    @Override
    public int getVerticalScrollOffset() {
        return mScrollView.getScrollY();
    }

    @Override
    public void destroy() {}

    @Override
    public int getType() {
        return BottomSheetContentController.TYPE_INCOGNITO_HOME;
    }

    @Override
    public boolean applyDefaultTopPadding() {
        return true;
    }
}
