// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import android.content.Context;
import android.support.v4.view.animation.PathInterpolatorCompat;
import android.text.TextUtils;
import android.view.animation.Interpolator;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelAnimation;
import org.chromium.chrome.browser.compositor.layouts.ChromeAnimation;

/**
 * Controls the image shown in the Bar. Owns the {@link ContextualSearchIconSpriteControl} and
 * details about the thumbnail, and handles animating between the two.
 */
public class ContextualSearchImageControl
        implements ChromeAnimation.Animatable<ContextualSearchImageControl.AnimationType> {
    /**
      * Animation properties.
      */
    protected enum AnimationType {
        STATIC_IMAGE_VISIBILITY
    }

    /** The current context. */
    private final Context mContext;

    /** The OverlayPanelAnimation used to add animations. */
    private final OverlayPanelAnimation mOverlayPanelAnimation;

    public ContextualSearchImageControl(OverlayPanelAnimation overlayPanelAnimation,
            Context context) {
        mContext = context;
        mOverlayPanelAnimation = overlayPanelAnimation;
    }

    // ============================================================================================
    // Search Provider Icon Sprite
    // ============================================================================================

    private ContextualSearchIconSpriteControl mIconSpriteControl;

    /**
     * @return The {@link ContextualSearchIconSpriteControl} for the panel.
     */
    public ContextualSearchIconSpriteControl getIconSpriteControl() {
        if (mIconSpriteControl == null) {
            mIconSpriteControl =
                    new ContextualSearchIconSpriteControl(mOverlayPanelAnimation, mContext);
        }
        return mIconSpriteControl;
    }

    /**
     * @param shouldAnimateIconSprite Whether the search provider icon sprite should be animated.
     */
    public void setShouldAnimateIconSprite(boolean shouldAnimateIconSprite) {
        getIconSpriteControl().setShouldAnimateAppearance(shouldAnimateIconSprite);
    }

    // ============================================================================================
    // Static Icon
    // ============================================================================================

    /**
     * The resource id of the static icon to display.
     */
    private int mStaticIconResourceId;

    /**
     * Whether the static icon is visible.
     */
    private boolean mStaticIconVisible;

    /**
     * @param resId The resource id of the static icon to display.
     */
    public void setStaticIconResourceId(int resId) {
        mStaticIconResourceId = resId;
        mStaticIconVisible = true;
        animateStaticImageVisibility(true);
    }

    /**
     * @return The resource id of the static icon to display.
     */
    public int getStaticIconResourceId() {
        return mStaticIconResourceId;
    }

    /**
     * @return Whether the static icon is visible.
     */
    public boolean getStaticIconVisible() {
        return mStaticIconVisible;
    }

    // ============================================================================================
    // Thumbnail
    // ============================================================================================

    /**
     * The URL of the thumbnail to display.
     */
    private String mThumbnailUrl;

    /**
     * Whether the thumbnail is visible.
     */
    private boolean mThumbnailVisible;

    /**
     * @param thumbnailUrl The URL of the thumbnail to display
     */
    public void setThumbnailUrl(String thumbnailUrl) {
        // If a static icon is showing, the thumbnail should not be shown.
        if (mStaticIconVisible) return;

        mThumbnailUrl = thumbnailUrl;
    }

    /**
     * @return The URL used to fetch a thumbnail to display in the Bar. Will return an empty string
     *         if no thumbnail is available.
     */
    public String getThumbnailUrl() {
        return mThumbnailUrl != null ? mThumbnailUrl : "";
    }

    /**
     * @return Whether the thumbnail is visible.
     */
    public boolean getThumbnailVisible() {
        return mThumbnailVisible;
    }

    /**
     * Called when the thumbnail has finished being fetched.
     * @param success Whether fetching the thumbnail was successful.
     */
    public void onThumbnailFetched(boolean success) {
        // Check if the thumbnail URL was cleared before the thumbnail fetch completed. This may
        // occur if the user taps to refine the search.
        mThumbnailVisible = success && !TextUtils.isEmpty(mThumbnailUrl);
        if (!mThumbnailVisible) return;

        // TODO(twellington): if the icon sprite is animating wait to start the thumbnail visibility
        //                    animation.
        animateStaticImageVisibility(true);
    }

    // ============================================================================================
    // Static Image -- either a thumbnail or static icon
    // ============================================================================================

    /**
     * The height and width of the static image.
     */
    private int mStaticImageSize;

    /**
     * The static image visibility percentage, which dictates how and where to draw the static
     * image. The static image is not visible at all at 0.f and completely visible at 1.f.
     */
    private float mStaticImageVisibilityPercentage = 0.f;

    /**
     * Hides the static image if it is visible and makes the icon sprite visible. Also resets the
     * thumbnail URL and static icon resource id.
     * @param animate Whether hiding the thumbnail should be animated.
     */
    public void hideStaticIcon(boolean animate) {
        getIconSpriteControl().setIsVisible(true);
        if ((mThumbnailVisible || mStaticIconVisible) && animate) {
            animateStaticImageVisibility(false);
        } else {
            mOverlayPanelAnimation.cancelAnimation(this, AnimationType.STATIC_IMAGE_VISIBILITY);
            onStaticImageHidden();
        }
    }

    /**
     * @return The height and width of the static image in px.
     */
    public int getStaticImageSize() {
        if (mStaticImageSize == 0) {
            mStaticImageSize = mContext.getResources().getDimensionPixelSize(
                    R.dimen.contextual_search_static_image_size);
        }
        return mStaticImageSize;
    }

    /**
     * @return The static image visibility percentage, which dictates how and where to draw the
     *         static image. The static image is not visible at all at 0.f and completely visible at
     *         1.f. The static image may be either a thumbnail or static icon.
     */
    public float getStaticImageVisibilityPercentage() {
        return mStaticImageVisibilityPercentage;
    }

    /**
     * Called when the static image finishes hiding to reset thumbnail and static icon values.
     */
    private void onStaticImageHidden() {
        mStaticIconResourceId = 0;
        mStaticIconVisible = false;

        mThumbnailUrl = "";
        mThumbnailVisible = false;
        getIconSpriteControl().setIsVisible(true);
        mStaticImageVisibilityPercentage = 0.f;
    }

    // ============================================================================================
    // Thumbnail Animation
    // ============================================================================================

    private Interpolator mStaticImageVisibilityInterpolator;

    private void animateStaticImageVisibility(boolean visible) {
        if (mStaticImageVisibilityInterpolator == null) {
            mStaticImageVisibilityInterpolator =
                    PathInterpolatorCompat.create(0.4f, 0.f, 0.6f, 1.f);
        }

        mOverlayPanelAnimation.cancelAnimation(this, AnimationType.STATIC_IMAGE_VISIBILITY);

        float endValue = visible ? 1.f : 0.f;
        mOverlayPanelAnimation.addToAnimation(this, AnimationType.STATIC_IMAGE_VISIBILITY,
                mStaticImageVisibilityPercentage, endValue,
                OverlayPanelAnimation.BASE_ANIMATION_DURATION_MS, 0, false,
                mStaticImageVisibilityInterpolator);
    }

    @Override
    public void setProperty(AnimationType prop, float val) {
        if (prop == AnimationType.STATIC_IMAGE_VISIBILITY) {
            mStaticImageVisibilityPercentage = val;
        }
    }

    @Override
    public void onPropertyAnimationFinished(AnimationType prop) {
        if (prop == AnimationType.STATIC_IMAGE_VISIBILITY) {
            if (mStaticImageVisibilityPercentage == 0.f) {
                onStaticImageHidden();
            } else {
                getIconSpriteControl().setIsVisible(false);
            }
        }
    }
}
