// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.ui.resources.dynamics.DynamicResourceLoader;
import org.chromium.ui.resources.dynamics.ViewResourceInflater;

/**
 * A helper class for inflating Overlay Panel Views.
 */
public abstract class OverlayPanelInflater extends ViewResourceInflater {

    /**
     * The panel delegate used to get information about the panel layout.
     */
    protected OverlayPanel mOverlayPanel;

    /**
     * Object Replacement Character that is used in place of HTML objects that cannot be represented
     * as text (e.g. images). Overlay panel should not be displaying such characters as
     * they get shown as [obj] character.
     */
    private static final String OBJ_CHARACTER = "\uFFFC";

    /**
     * @param panel             The panel.
     * @param layoutId          The XML Layout that declares the View.
     * @param viewId            The id of the root View of the Layout.
     * @param context           The Android Context used to inflate the View.
     * @param container         The container View used to inflate the View.
     * @param resourceLoader    The resource loader that will handle the snapshot capturing.
     */
    public OverlayPanelInflater(OverlayPanel panel,
                                    int layoutId,
                                    int viewId,
                                    Context context,
                                    ViewGroup container,
                                    DynamicResourceLoader resourceLoader) {
        super(layoutId, viewId, context, container, resourceLoader);

        mOverlayPanel = panel;
    }

    @Override
    public void destroy() {
        super.destroy();

        mOverlayPanel = null;
    }

    @Override
    protected void onFinishInflate() {
        if (!mOverlayPanel.isFullWidthSizePanel()) {
            setWidth(mOverlayPanel.getMaximumWidthPx());
        }
    }

    @Override
    protected int getWidthMeasureSpec() {
        return View.MeasureSpec.makeMeasureSpec(
                mOverlayPanel.getMaximumWidthPx(), View.MeasureSpec.EXACTLY);
    }

    /**
     * @param width The width of the view to be inforced.
     */
    private void setWidth(int width) {
        // When the view is attached, we need to force the layout to have a specific width
        // because the container is "full-width" (as wide as a tab). When not attached,
        // ViewResourceInflater#layout() will properly resize the view offscreen.
        if (shouldAttachView()) {
            View view = getView();
            if (view != null) {
                view.getLayoutParams().width = width;
                view.requestLayout();
            }
        }
    }

    /**
     * Sanitizes a string to be displayed on the Overlay Panel Bar.
     * @param text The text to be sanitized.
     * @return The sanitized text.
     */
    public static String sanitizeText(String text) {
        if (text == null) return null;
        return text.replace(OBJ_CHARACTER, " ").trim();
    }

}
