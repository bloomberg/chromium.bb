// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import android.content.Context;
import android.view.ViewGroup;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelAnimation;
import org.chromium.chrome.browser.compositor.layouts.ChromeAnimation;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

/**
 * Controls the Search Bar in the Contextual Search Panel.
 */
public class ContextualSearchBarControl
        implements ChromeAnimation.Animatable<ContextualSearchBarControl.AnimationType> {

    /**
     * Animation properties.
     */
    protected enum AnimationType {
        TEXT_OPACITY,
        DIVIDER_LINE_VISIBILITY
    }

    /**
     * The panel used to get information about the panel layout.
     */
    protected OverlayPanel mOverlayPanel;

    /**
     * The {@link ContextualSearchContextControl} used to control the Search Context View.
     */
    private final ContextualSearchContextControl mContextControl;

    /**
     * The {@link ContextualSearchTermControl} used to control the Search Term View.
     */
    private final ContextualSearchTermControl mSearchTermControl;

    /**
    * The {@link ContextualSearchCaptionControl} used to control the Caption View.
    */
    private final ContextualSearchCaptionControl mCaptionControl;

    /**
     * The {@link ContextualSearchQuickActionControl} used to control quick action behavior.
     */
    private final ContextualSearchQuickActionControl mQuickActionControl;

    /**
     * The {@link ContextualSearchImageControl} for the panel.
     */
    private ContextualSearchImageControl mImageControl;

    /**
     * The opacity of the Bar's Search Context.
     */
    private float mSearchBarContextOpacity = 1.f;

    /**
     * The opacity of the Bar's Search Term.
     */
    private float mSearchBarTermOpacity = 1.f;

    // Dimensions used for laying out the search bar.
    private final float mTextLayerMinHeight;
    private final float mTermCaptionSpacing;

    /**
     * The visibility percentage for the divider line ranging from 0.f to 1.f.
     */
    private float mDividerLineVisibilityPercentage;

    /**
     * The width of the divider line in px.
     */
    private final float mDividerLineWidth;

    /**
     * The height of the divider line in px.
     */
    private final float mDividerLineHeight;

    /**
     * The divider line color.
     */
    private final int mDividerLineColor;

    /**
     * The width of the end button in px.
     */
    private final float mEndButtonWidth;

    /**
     * Constructs a new bottom bar control container by inflating views from XML.
     *
     * @param panel     The panel.
     * @param context   The context used to build this view.
     * @param container The parent view for the bottom bar views.
     * @param loader    The resource loader that will handle the snapshot capturing.
     */
    public ContextualSearchBarControl(OverlayPanel panel,
                                      Context context,
                                      ViewGroup container,
                                      DynamicResourceLoader loader) {
        mOverlayPanel = panel;
        mImageControl = new ContextualSearchImageControl(panel, context);
        mContextControl = new ContextualSearchContextControl(panel, context, container, loader);
        mSearchTermControl = new ContextualSearchTermControl(panel, context, container, loader);
        mCaptionControl = new ContextualSearchCaptionControl(panel, context, container, loader);
        mQuickActionControl = new ContextualSearchQuickActionControl(context, loader);

        mTextLayerMinHeight = context.getResources().getDimension(
                R.dimen.contextual_search_text_layer_min_height);
        mTermCaptionSpacing = context.getResources().getDimension(
                R.dimen.contextual_search_term_caption_spacing);

        // Divider line values.
        mDividerLineWidth = context.getResources().getDimension(
                R.dimen.contextual_search_divider_line_width);
        mDividerLineHeight = context.getResources().getDimension(
                R.dimen.contextual_search_divider_line_height);
        mDividerLineColor = ApiCompatibilityUtils.getColor(context.getResources(),
                R.color.google_grey_500);
        mEndButtonWidth = context.getResources().getDimension(
                R.dimen.contextual_search_end_button_width);
    }

    /**
     * @return The {@link ContextualSearchImageControl} for the panel.
     */
    public ContextualSearchImageControl getImageControl() {
        return mImageControl;
    }

    /**
     * Returns the minimum height that the text layer (containing the Search Context, Term and
     * Caption) should be.
     */
    public float getTextLayerMinHeight() {
        return mTextLayerMinHeight;
    }

    /**
     * Returns the spacing that should be placed between the Search Term and Caption.
     */
    public float getSearchTermCaptionSpacing() {
        return mTermCaptionSpacing;
    }

    /**
     * Removes the bottom bar views from the parent container.
     */
    public void destroy() {
        mContextControl.destroy();
        mSearchTermControl.destroy();
        mCaptionControl.destroy();
    }

    /**
     * Updates this bar when in transition between peeked to expanded states.
     * @param percentage The percentage to the more opened state.
     */
    public void onUpdateFromPeekToExpand(float percentage) {
        // If there is a quick action, the divider line's appearance was animated when the quick
        // action was set.
        if (!getQuickActionControl().hasQuickAction()) {
            mDividerLineVisibilityPercentage = percentage;
        }
        getImageControl().onUpdateFromPeekToExpand(percentage);
        mCaptionControl.onUpdateFromPeekToExpand(percentage);
    }

    /**
     * Sets the search context to display in the control.
     * @param selection The portion of the context that represents the user's selection.
     * @param end The portion of the context after the selection.
     */
    public void setSearchContext(String selection, String end) {
        cancelSearchTermResolutionAnimation();
        hideCaption();
        mQuickActionControl.reset();
        mContextControl.setSearchContext(selection, end);
        resetSearchBarContextOpacity();
        animateDividerLine(false);
    }

    /**
     * Sets the search term to display in the control.
     * @param searchTerm The string that represents the search term.
     */
    public void setSearchTerm(String searchTerm) {
        cancelSearchTermResolutionAnimation();
        hideCaption();
        mQuickActionControl.reset();
        mSearchTermControl.setSearchTerm(searchTerm);
        resetSearchBarTermOpacity();
        animateDividerLine(false);
    }

    /**
     * Sets the caption to display in the control and sets the caption visible.
     * @param caption The caption to display.
     */
    public void setCaption(String caption) {
        mCaptionControl.setCaption(caption);
    }

    /**
     * Gets the current animation percentage for the Caption control, which guides the vertical
     * position and opacity of the caption.
     * @return The animation percentage ranging from 0.0 to 1.0.
     *
     */
    public float getCaptionAnimationPercentage() {
        return mCaptionControl.getAnimationPercentage();
    }

    /**
     * @return Whether the caption control is visible.
     */
    public boolean getCaptionVisible() {
        return mCaptionControl.getIsVisible();
    }

    /**
     * @return The Id of the Search Context View.
     */
    public int getSearchContextViewId() {
        return mContextControl.getViewId();
    }

    /**
     * @return The Id of the Search Term View.
     */
    public int getSearchTermViewId() {
        return mSearchTermControl.getViewId();
    }

    /**
     * @return The Id of the Search Caption View.
     */
    public int getCaptionViewId() {
        return mCaptionControl.getViewId();
    }

    /**
     * @return The opacity of the SearchBar's search context.
     */
    public float getSearchBarContextOpacity() {
        return mSearchBarContextOpacity;
    }

    /**
     * @return The opacity of the SearchBar's search term.
     */
    public float getSearchBarTermOpacity() {
        return mSearchBarTermOpacity;
    }

    /**
     * Sets the quick action if one is available.
     * @param quickActionUri The URI for the intent associated with the quick action.
     * @param quickActionCategory The {@link QuickActionCategory} for the quick action.
     */
    public void setQuickAction(String quickActionUri, int quickActionCategory) {
        mQuickActionControl.setQuickAction(quickActionUri, quickActionCategory);
        if (mQuickActionControl.hasQuickAction()) {
            // TODO(twellington): should the quick action caption be stored separately from the
            // regular caption?
            mCaptionControl.setCaption(mQuickActionControl.getCaption());
            mImageControl.setQuickActionIconResourceId(mQuickActionControl.getIconResId());
            animateDividerLine(true);
        }
    }

    /**
     * @return The {@link ContextualSearchQuickActionControl} for the panel.
     */
    public ContextualSearchQuickActionControl getQuickActionControl() {
        return mQuickActionControl;
    }

    /**
     * Resets the SearchBar text opacity when a new search context is set. The search
     * context is made visible and the search term invisible.
     */
    private void resetSearchBarContextOpacity() {
        mSearchBarContextOpacity = 1.f;
        mSearchBarTermOpacity = 0.f;
    }

    /**
     * Resets the SearchBar text opacity when a new search term is set. The search
     * term is made visible and the search context invisible.
     */
    private void resetSearchBarTermOpacity() {
        mSearchBarContextOpacity = 0.f;
        mSearchBarTermOpacity = 1.f;
    }

    /**
     * Hides the caption so it will not be displayed in the control.
     */
    private void hideCaption() {
        mCaptionControl.hide();
    }

    // ============================================================================================
    // Divider Line
    // ============================================================================================
    /**
     * @return The visibility percentage for the divider line ranging from 0.f to 1.f.
     */
    public float getDividerLineVisibilityPercentage() {
        return mDividerLineVisibilityPercentage;
    }

    /**
     * @return The width of the divider line in px.
     */
    public float getDividerLineWidth() {
        return mDividerLineWidth;
    }

    /**
     * @return The height of the divider line in px.
     */
    public float getDividerLineHeight() {
        return mDividerLineHeight;
    }

    /**
     * @return The divider line color.
     */
    public int getDividerLineColor() {
        return mDividerLineColor;
    }

    /**
     * @return The x-offset for the divider line relative to the x-position of the Bar in px.
     */
    public float getDividerLineXOffset() {
        if (LocalizationUtils.isLayoutRtl()) {
            return mEndButtonWidth;
        } else {
            return mOverlayPanel.getContentViewWidthPx() - mEndButtonWidth - getDividerLineWidth();
        }
    }

    /**
     * Animates the appearance or disappearance of the divider line.
     * @param visible Whether the divider line should be made visible.
     */
    private void animateDividerLine(boolean visible) {
        float endValue = visible ? 1.f : 0.f;
        if (mDividerLineVisibilityPercentage == endValue) return;
        mOverlayPanel.addToAnimation(this, AnimationType.DIVIDER_LINE_VISIBILITY,
                mDividerLineVisibilityPercentage, endValue,
                OverlayPanelAnimation.BASE_ANIMATION_DURATION_MS, 0);
    }

    // ============================================================================================
    // Search Bar Animation
    // ============================================================================================

    /**
     * Animates the search term resolution.
     */
    public void animateSearchTermResolution() {
        mOverlayPanel.addToAnimation(this, AnimationType.TEXT_OPACITY, 0.f, 1.f,
                OverlayPanelAnimation.MAXIMUM_ANIMATION_DURATION_MS, 0);
    }

    /**
     * Cancels the search term resolution animation if it is in progress.
     */
    public void cancelSearchTermResolutionAnimation() {
        if (mOverlayPanel.animationIsRunning()) {
            mOverlayPanel.cancelAnimation(this, AnimationType.TEXT_OPACITY);
        }
    }

    /**
     * Updates the UI state for the SearchBar text. The search context view will fade out
     * while the search term fades in.
     *
     * @param percentage The visibility percentage of the search term view.
     */
    private void updateSearchBarTextOpacity(float percentage) {
        // The search context will start fading out before the search term starts fading in.
        // They will both be partially visible for overlapPercentage of the animation duration.
        float overlapPercentage = .75f;
        float fadingOutPercentage = Math.max(1 - (percentage / overlapPercentage), 0.f);
        float fadingInPercentage =
                Math.max(percentage - (1 - overlapPercentage), 0.f) / overlapPercentage;

        mSearchBarContextOpacity = fadingOutPercentage;
        mSearchBarTermOpacity = fadingInPercentage;
    }

    // ============================================================================================
    // ChromeAnimation.Animatable Implementation
    // ============================================================================================

    @Override
    public void setProperty(AnimationType type, float value) {
        if (type == AnimationType.TEXT_OPACITY) {
            updateSearchBarTextOpacity(value);
        } else if (type == AnimationType.DIVIDER_LINE_VISIBILITY) {
            mDividerLineVisibilityPercentage = value;
        }
    }

    @Override
    public void onPropertyAnimationFinished(AnimationType prop) {}
}
