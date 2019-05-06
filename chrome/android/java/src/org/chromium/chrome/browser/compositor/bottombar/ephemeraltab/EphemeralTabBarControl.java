// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.ephemeraltab;

import android.content.Context;
import android.view.ViewGroup;

import org.chromium.chrome.R;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

/**
 * Top control used for ephemeral tab.
 */
public class EphemeralTabBarControl {
    /** Full opacity -- fully visible. */
    private static final float SOLID_OPAQUE = 1.0f;

    /** Transparent opacity -- completely transparent (not visible). */
    private static final float SOLID_TRANSPARENT = 0.0f;

    private final EphemeralTabTitleControl mTitle;

    // Dimensions used for laying out the controls in the bar.
    private final float mTextLayerMinHeight;

    /**
     * @param panel The panel.
     * @param context The Android Context used to inflate the View.
     * @param container The container View used to inflate the View.
     * @param loader The resource loader that will handle the snapshot capturing.
     */
    public EphemeralTabBarControl(EphemeralTabPanel panel, Context context, ViewGroup container,
            DynamicResourceLoader loader) {
        mTitle = new EphemeralTabTitleControl(panel, context, container, loader);
        mTextLayerMinHeight = context.getResources().getDimension(
                R.dimen.contextual_search_text_layer_min_height);
    }

    /**
     * Returns the minimum height that the text layer (containing the title and the caption)
     * should be.
     */
    public float getTextLayerMinHeight() {
        return mTextLayerMinHeight;
    }

    /**
     * Set the text in the panel.
     * @param text The string to set the text to.
     */
    public void setBarText(String text) {
        mTitle.setBarText(text);
    }

    /**
     * @return {@link EphemeralTabTitleControl} object.
     */
    public EphemeralTabTitleControl getTitleControl() {
        return mTitle;
    }

    /**
     * Removes the bottom bar views from the parent container.
     */
    public void destroy() {
        mTitle.destroy();
    }
}
