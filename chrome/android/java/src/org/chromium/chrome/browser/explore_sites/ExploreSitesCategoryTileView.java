// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.compositor.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.ntp.TitleUtil;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.widget.ViewHighlighter;
import org.chromium.chrome.browser.widget.textbubble.TextBubble;
import org.chromium.chrome.browser.widget.tile.TileWithTextView;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.widget.ViewRectProvider;

/**
 * A category tile for ExploreSites, containing an icon that is a composition of sites' favicons
 * within the category.  Alternatively, a MORE button.
 */
public class ExploreSitesCategoryTileView extends TileWithTextView {
    private static final int FADE_ANIMATION_TIME_MS = 300;
    private static final int TITLE_LINES = 1;
    private static final boolean SUPPORTED_OFFLINE = false;

    /** The data currently associated to this tile. */
    private ExploreSitesCategory mCategory;

    /**
     * Constructor for inflating from XML.
     */
    public ExploreSitesCategoryTileView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Initializes the view using the data held by {@code tile}. This should be called immediately
     * after inflation.
     * @param category The object that holds the data to populate this view.
     */
    public void initialize(ExploreSitesCategory category, Profile profile) {
        super.initialize(TitleUtil.getTitleForDisplay(category.getTitle(), category.getUrl()),
                SUPPORTED_OFFLINE, category.getDrawable(), TITLE_LINES);
        mCategory = category;

        // Correct the properties of the icon for categories, it should be the entire size of the
        // icon background now.
        mIconView.setScaleType(ImageView.ScaleType.CENTER);
        MarginLayoutParams layoutParams = (MarginLayoutParams) mIconView.getLayoutParams();
        int tileViewIconSize =
                getContext().getResources().getDimensionPixelSize(R.dimen.tile_view_icon_size);
        layoutParams.width = tileViewIconSize;
        layoutParams.height = tileViewIconSize;
        layoutParams.topMargin = getContext().getResources().getDimensionPixelSize(
                R.dimen.tile_view_icon_background_margin_top_modern);
        mIconView.setLayoutParams(layoutParams);
        Context context = getContext();

        if (mCategory.getType() == ExploreSitesCategory.CategoryType.MORE_BUTTON
                && context instanceof ChromeActivity) {
            ChromeActivity activity = (ChromeActivity) context;
            if (isAttachedToWindow()) {
                initializeIPH(profile, activity);
            } else {
                addOnAttachStateChangeListener(new OnAttachStateChangeListener() {
                    @Override
                    public void onViewAttachedToWindow(View v) {
                        initializeIPH(profile, activity);
                    }

                    @Override
                    public void onViewDetachedFromWindow(View v) {}
                });
            }
        }
    }

    private void initializeIPH(Profile profile, ChromeActivity activity) {
        // Activity was destroyed; don't show IPH.
        if (activity.isActivityFinishingOrDestroyed()) return;

        final String contentString = getContext().getString(R.string.explore_sites_iph);
        assert (contentString.length() > 0);

        final String accessibilityString =
                getContext().getString(R.string.explore_sites_iph_accessibility);
        assert (accessibilityString.length() > 0);

        final Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        if (!tracker.shouldTriggerHelpUI(FeatureConstants.EXPLORE_SITES_TILE_FEATURE)) return;

        ViewRectProvider rectProvider = new ViewRectProvider(this);

        TextBubble textBubble = new TextBubble(
                this.getContext(), this, contentString, accessibilityString, true, rectProvider);
        textBubble.setDismissOnTouchInteraction(true);
        View foregroundView = findViewById(R.id.tile_view_highlight);
        ViewHighlighter.turnOnHighlight(foregroundView, true);
        textBubble.addOnDismissListener(() -> {
            ViewHighlighter.turnOffHighlight(foregroundView);

            tracker.dismissed(FeatureConstants.EXPLORE_SITES_TILE_FEATURE);
        });
        textBubble.show();
    }

    /** Retrieves url associated with this view. */
    public String getUrl() {
        return mCategory.getUrl();
    }

    public ExploreSitesCategory getCategory() {
        return mCategory;
    }

    /** Renders icon based on tile data.  */
    public void renderIcon(ExploreSitesCategory category) {
        mCategory = category;
        // If the category is a placeholder, just instantly render it, as we can assume there has
        // been no appreciable delay since the NTP was initialized.
        if (mCategory.isPlaceholder()) {
            setIconDrawable(category.getDrawable());
            return;
        }
        fadeThumbnailIn(category.getDrawable());
    }

    private void fadeThumbnailIn(Drawable thumbnail) {
        int duration = FADE_ANIMATION_TIME_MS;

        // If animations are disabled, just show the thumbnail.
        if (CompositorAnimationHandler.isInTestingMode()) {
            setIconDrawable(thumbnail);
            return;
        }

        // We have some transition time, but no existing icon.  TransitionDrawable requires two or
        // more drawables to crossfade, so manually fade in here.
        if (mIconView.getDrawable() == null) {
            mIconView.setImageDrawable(thumbnail);
            mIconView.setAlpha(0.0f);
            mIconView.animate().alpha(1.0f).setDuration(duration).start();
            return;
        }

        mIconView.animate()
                .alpha(0.0f)
                .setDuration(duration / 2)
                .setListener(new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mIconView.setImageDrawable(thumbnail);
                        mIconView.animate().alpha(1.0f).setDuration(duration / 2).start();
                    }
                })
                .start();
    }
}
