// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelInflater;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

/**
 * Controls the Caption View that is shown at the bottom of the control and used
 * as a dynamic resource.
 */
public class ContextualSearchCaptionControl extends OverlayPanelInflater {
    private static final float CAPTION_OPACITY_OPAQUE = 1.f;
    private static final float CAPTION_OPACITY_TRANSPARENT = 0.f;

    /**
     * The caption View.
     */
    private TextView mCaption;

    /**
     * The caption visibility.
     */
    private boolean mIsVisible;

    /**
     * The caption opacity.
     */
    private float mOpacity;

    /**
     * @param panel             The panel.
     * @param context           The Android Context used to inflate the View.
     * @param container         The container View used to inflate the View.
     * @param resourceLoader    The resource loader that will handle the snapshot capturing.
     */
    public ContextualSearchCaptionControl(OverlayPanel panel, Context context, ViewGroup container,
            DynamicResourceLoader resourceLoader) {
        super(panel, R.layout.contextual_search_caption_view, R.id.contextual_search_caption_view,
                context, container, resourceLoader);
    }

    /**
     * Sets the caption to display in the bottom of the control.
     * @param caption The string displayed as a caption to help explain results,
     *        e.g. a Quick Answer.
     */
    public void setCaption(String caption) {
        inflate();

        mCaption.setText(sanitizeText(caption));

        invalidate();
        show();
    }

    /**
     * Hides the caption.
     */
    public void hide() {
        mIsVisible = false;
        mOpacity = CAPTION_OPACITY_TRANSPARENT;

        // Snapshot the transparent caption.
        invalidate();
    }

    /**
     * Shows the caption.
     */
    private void show() {
        mIsVisible = true;
        // When the Panel is in transition it will get the right opacity during the
        // state transition update.
        if (mOverlayPanel.isPeeking()) {
            mOpacity = CAPTION_OPACITY_OPAQUE;
        }
    }

    /**
     * @return The current opacity of the Caption.
     */
    public float getOpacity() {
        return mOpacity;
    }

    /**
     * Updates this caption when in transition between closed to peeked states.
     * @param percentage The percentage to the more opened state.
     */
    public void onUpdateFromCloseToPeek(float percentage) {
        if (!mIsVisible) return;

        mOpacity = CAPTION_OPACITY_OPAQUE;
    }

    /**
     * Updates this caption when in transition between peeked to expanded states.
     * @param percentage The percentage to the more opened state.
     */
    public void onUpdateFromPeekToExpand(float percentage) {
        if (!mIsVisible) return;

        float fadingOutPercentage = Math.max(0f, (percentage - 0.5f) * 2);
        mOpacity = MathUtils.interpolate(
                CAPTION_OPACITY_OPAQUE,
                CAPTION_OPACITY_TRANSPARENT,
                fadingOutPercentage);
    }

    /**
     * Updates this caption when in transition between expanded and maximized states.
     * @param percentage The percentage to the more opened state.
     */
    public void onUpdateFromExpandToMaximize(float percentage) {
        if (!mIsVisible) return;

        mOpacity = CAPTION_OPACITY_TRANSPARENT;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        View view = getView();
        mCaption = (TextView) view.findViewById(R.id.contextual_search_caption);
    }
}
