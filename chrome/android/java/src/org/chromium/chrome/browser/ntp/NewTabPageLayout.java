// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.content.Context;
import android.content.res.Resources;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;

import org.chromium.chrome.R;

/**
 * Layout for the new tab page. This positions the page elements in the correct vertical positions.
 * There are no separate phone and tablet UIs; this layout adapts based on the available space.
 */
public class NewTabPageLayout extends LinearLayout {

    // Space permitting, the spacers will grow from 0dp to the heights given below. If there is
    // additional space, it will be distributed evenly between the top and bottom spacers.
    private static final float TOP_SPACER_HEIGHT_DP = 44f;
    private static final float MIDDLE_SPACER_HEIGHT_DP = 24f;
    private static final float BOTTOM_SPACER_HEIGHT_DP = 44f;
    private static final float TOTAL_SPACER_HEIGHT_DP = TOP_SPACER_HEIGHT_DP
            + MIDDLE_SPACER_HEIGHT_DP + BOTTOM_SPACER_HEIGHT_DP;

    private final int mTopSpacerIdealHeight;
    private final int mMiddleSpacerIdealHeight;
    private final int mBottomSpacerIdealHeight;
    private final int mTotalSpacerIdealHeight;
    private final int mMostVisitedLayoutBleed;
    private final int mPeekingCardHeight;
    private final int mTabStripHeight;

    private int mParentViewportHeight;
    private int mSearchboxViewShadowWidth;

    private boolean mCardsUiEnabled;
    private View mTopSpacer;  // Spacer above search logo.
    private View mMiddleSpacer;  // Spacer between toolbar and Most Likely.
    private View mBottomSpacer;  // Spacer below Most Likely.

    // Separate spacer below Most Likely to add enough space so the user can scroll with Most Likely
    // at the top of the screen.
    private View mScrollCompensationSpacer;

    private LogoView mSearchProviderLogoView;
    private View mSearchBoxView;
    private MostVisitedLayout mMostVisitedLayout;
    /**
     * Constructor for inflating from XML.
     */
    public NewTabPageLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        Resources res = getResources();
        float density = res.getDisplayMetrics().density;
        mTopSpacerIdealHeight = Math.round(density * TOP_SPACER_HEIGHT_DP);
        mMiddleSpacerIdealHeight = Math.round(density * MIDDLE_SPACER_HEIGHT_DP);
        mBottomSpacerIdealHeight = Math.round(density * BOTTOM_SPACER_HEIGHT_DP);
        mTotalSpacerIdealHeight = Math.round(density * TOTAL_SPACER_HEIGHT_DP);
        mMostVisitedLayoutBleed = res.getDimensionPixelSize(R.dimen.most_visited_layout_bleed);

        mPeekingCardHeight = getResources()
                .getDimensionPixelSize(R.dimen.snippets_padding_and_peeking_card_height);
        mTabStripHeight = getResources().getDimensionPixelSize(R.dimen.tab_strip_height);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTopSpacer = findViewById(R.id.ntp_top_spacer);
        mMiddleSpacer = findViewById(R.id.ntp_middle_spacer);
        mBottomSpacer = findViewById(R.id.ntp_bottom_spacer);
        mScrollCompensationSpacer = findViewById(R.id.ntp_scroll_spacer);
        mSearchProviderLogoView = (LogoView) findViewById(R.id.search_provider_logo);
        mSearchBoxView = findViewById(R.id.search_box);
        mMostVisitedLayout = (MostVisitedLayout) findViewById(R.id.most_visited_layout);
        setSearchBoxStyle();
    }

    /**
     * Specifies the height of the parent's viewport for the container view of this View.
     *
     * As this is required in onMeasure, we can not rely on the parent having the proper
     * size set yet and thus must be told explicitly of this size.
     *
     * This View takes into account the presence of the tab strip height for tablets.
     */
    public void setParentViewportHeight(int height) {
        mParentViewportHeight = height;
    }

    /**
     * Sets whether the cards UI is enabled.
     * This view assumes that a peeking card will always be present when the cards UI is used.
     */
    public void setUseCardsUiEnabled(boolean useCardsUi) {
        mCardsUiEnabled = useCardsUi;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        mScrollCompensationSpacer.setVisibility(View.GONE);
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        if (getMeasuredHeight() > mParentViewportHeight) {
            // This layout is bigger than its parent's viewport, so the user will need to scroll
            // to see all of it. Extra spacing should be added at the bottom so the user can scroll
            // until Most Visited is at the top.

            // All the spacers have height 0 since they use weights to set height.
            assert mTopSpacer.getMeasuredHeight() == 0;

            final int topOfMostVisited = calculateTopOfMostVisited();
            final int belowTheFoldHeight = getMeasuredHeight() - mParentViewportHeight;
            if (belowTheFoldHeight < topOfMostVisited) {
                mScrollCompensationSpacer.getLayoutParams().height =
                        topOfMostVisited - belowTheFoldHeight;

                if (mCardsUiEnabled) {
                    // If we have a peeking card, allow that to show at the bottom of the screen.
                    mScrollCompensationSpacer.getLayoutParams().height -= mPeekingCardHeight;
                }

                mScrollCompensationSpacer.setVisibility(View.INVISIBLE);
                super.onMeasure(widthMeasureSpec, heightMeasureSpec);
            }
        } else {
            // This layout is smaller than it's parent viewport, redistribute the extra space.
            if (mCardsUiEnabled) {
                getLayoutParams().height = Math.max(getMeasuredHeight(),
                        mParentViewportHeight - mPeekingCardHeight - mTabStripHeight);
                // Call onMeasure to update mTopScaper's height.
                super.onMeasure(widthMeasureSpec, heightMeasureSpec);
            }
            distributeExtraSpace(mTopSpacer.getMeasuredHeight());
        }

        // Make the search box and logo as wide as the most visited items.
        if (mMostVisitedLayout.getVisibility() != GONE) {
            final int width = mMostVisitedLayout.getMeasuredWidth() - mMostVisitedLayoutBleed;
            setMeasure(mSearchBoxView, width + mSearchboxViewShadowWidth,
                    mSearchBoxView.getMeasuredHeight());
            setMeasure(mSearchProviderLogoView, width, mSearchProviderLogoView.getMeasuredHeight());
        }
    }

    /**
     * Calculate the vertical position of Most Visited.
     * This method does not use mMostVisitedLayout.getTop(), so can be called in onMeasure.
     */
    private int calculateTopOfMostVisited() {
        // Manually add the heights (and margins) of all children above Most Visited.
        int top = 0;
        int mostVisitedIndex = indexOfChild(mMostVisitedLayout);
        for (int i = 0; i < mostVisitedIndex; i++) {
            View child = getChildAt(i);
            MarginLayoutParams params = (MarginLayoutParams) child.getLayoutParams();

            if (child.getVisibility() != View.GONE) {
                top += params.topMargin + child.getMeasuredHeight() + params.bottomMargin;
            }
        }
        top += ((MarginLayoutParams) mMostVisitedLayout.getLayoutParams()).topMargin;
        return top;
    }

    /**
     * Set the search box style, adding a shadow if required.
     */
    private void setSearchBoxStyle() {
        if (!NtpColorUtils.shouldUseMaterialColors()) return;

        Resources resources = getContext().getResources();

        // Adjust the margins to account for the bigger size due to the shadow.
        MarginLayoutParams layoutParams = (MarginLayoutParams) mSearchBoxView.getLayoutParams();
        layoutParams.setMargins(
                resources.getDimensionPixelSize(R.dimen.ntp_search_box_material_margin_left),
                resources.getDimensionPixelSize(R.dimen.ntp_search_box_material_margin_top),
                resources.getDimensionPixelSize(R.dimen.ntp_search_box_material_margin_right),
                resources.getDimensionPixelSize(R.dimen.ntp_search_box_material_margin_bottom));
        layoutParams.height = resources
                .getDimensionPixelSize(R.dimen.ntp_search_box_material_height);
        // Width will be adjusted in onMeasure();
        mSearchboxViewShadowWidth = resources
                .getDimensionPixelOffset(R.dimen.ntp_search_box_material_extra_width);

        mSearchBoxView.setBackgroundResource(R.drawable.textbox);
        mSearchBoxView.setPadding(
                resources.getDimensionPixelSize(R.dimen.ntp_search_box_material_padding_left),
                resources.getDimensionPixelSize(R.dimen.ntp_search_box_material_padding_top),
                resources.getDimensionPixelSize(R.dimen.ntp_search_box_material_padding_right),
                resources.getDimensionPixelSize(R.dimen.ntp_search_box_material_padding_bottom));
    }

    /**
     * Distribute extra vertical space between the three spacer views.
     * @param extraHeight The amount of extra space, in pixels.
     */
    private void distributeExtraSpace(int extraHeight) {
        int topSpacerHeight;
        int middleSpacerHeight;
        int bottomSpacerHeight;

        if (extraHeight < mTotalSpacerIdealHeight) {
            // The spacers will be less than their ideal height, shrink them proportionally.
            topSpacerHeight =
                    Math.round(extraHeight * (TOP_SPACER_HEIGHT_DP / TOTAL_SPACER_HEIGHT_DP));
            middleSpacerHeight =
                    Math.round(extraHeight * (MIDDLE_SPACER_HEIGHT_DP / TOTAL_SPACER_HEIGHT_DP));
            bottomSpacerHeight = extraHeight - topSpacerHeight - middleSpacerHeight;
        } else {
            // Distribute remaining space evenly between the top and bottom spacers.
            extraHeight -= mTotalSpacerIdealHeight;
            topSpacerHeight = mTopSpacerIdealHeight + extraHeight / 2;
            middleSpacerHeight = mMiddleSpacerIdealHeight;
            bottomSpacerHeight = mBottomSpacerIdealHeight + extraHeight / 2;
        }

        setMeasure(mTopSpacer, 0, topSpacerHeight);
        setMeasure(mMiddleSpacer, 0, middleSpacerHeight);
        setMeasure(mBottomSpacer, 0, bottomSpacerHeight);
    }


    /**
     * Convenience method to call |measure| on the given View with MeasureSpecs converted from the
     * given dimensions (in pixels) with MeasureSpec.EXACTLY.
     */
    private void setMeasure(View view, int widthPx, int heightPx) {
        view.measure(MeasureSpec.makeMeasureSpec(widthPx, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(heightPx, MeasureSpec.EXACTLY));
    }
}
