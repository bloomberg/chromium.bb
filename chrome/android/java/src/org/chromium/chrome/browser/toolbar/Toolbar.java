// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.graphics.Bitmap;
import android.graphics.Rect;
import android.view.View;
import android.view.View.OnClickListener;

import org.chromium.chrome.browser.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.compositor.Invalidator;
import org.chromium.chrome.browser.omnibox.LocationBar;

/**
 * An interface for other classes to interact with Toolbar Layout. Other than for testing purposes
 * this interface should be used rather than {@link ToolbarLayout} and extending classes.
 */
public interface Toolbar {

    /**
     * Initialize the external dependencies required for view interaction.
     * @param toolbarDataProvider The provider for toolbar data.
     * @param tabController       The controller that handles interactions with the tab.
     * @param appMenuButtonHelper The helper for managing menu button interactions.
     */
    void initialize(ToolbarDataProvider toolbarDataProvider,
            ToolbarTabController tabController, AppMenuButtonHelper appMenuButtonHelper);

    /**
     * Sets the {@link Invalidator} that will be called when the toolbar attempts to invalidate the
     * drawing surface.  This will give the object that registers as the host for the
     * {@link Invalidator} a chance to defer the actual invalidate to sync drawing.
     * @param invalidator An {@link Invalidator} instance.
     */
    void setPaintInvalidator(Invalidator invalidator);

    /**
     * Adds a custom action button to the {@link Toolbar} if it is supported.
     * @param buttonSource The {@link Bitmap} resource to use as the source for the button.
     * @param listener The {@link OnClickListener} to use for clicks to the button.
     */
    void addCustomActionButton(Bitmap buttonSource, OnClickListener listener);

    /**
     * Sets the OnClickListener that will be notified when the TabSwitcher button is pressed.
     * @param listener The callback that will be notified when the TabSwitcher button is pressed.
     */
    void setOnTabSwitcherClickHandler(OnClickListener listener);

    /**
     * Sets the OnClickListener that will be notified when the New Tab button is pressed.
     * @param listener The callback that will be notified when the New Tab button is pressed.
     */
    void setOnNewTabClickHandler(OnClickListener listener);

    /**
     * Sets the OnClickListener that will be notified when the bookmark button is pressed.
     * @param listener The callback that will be notified when the bookmark button is pressed.
     */
    void setBookmarkClickHandler(OnClickListener listener);

    /**
     * Calculates the {@link Rect} that represents the content area of the location bar.  This
     * rect will be relative to the toolbar.
     * @param outRect The Rect that represents the content area of the location bar.
     */
    void getLocationBarContentRect(Rect outRect);

    /**
     * @return Whether any swipe gestures should be ignored for the current Toolbar state.
     */
    boolean shouldIgnoreSwipeGesture();

    /**
     * Returns the elapsed realtime in ms of the time at which first draw for the toolbar occurred.
     */
    long getFirstDrawTime();

    /**
     * Finish any toolbar animations.
     */
    void finishAnimations();

    /**
     * @return {@link LocationBar} object this {@link Toolbar} contains.
     */
    LocationBar getLocationBar();

    // TODO(yusufo): Move the below calls to a separate interface about texture capture.
    /**
     * Calculate the relative position wrt to the given container view.
     * @param containerView The container view to be used.
     * @param position The position array to be used for returning the calculated position.
     */
    void getPositionRelativeToContainer(View containerView, int[] position);

    /**
     * Sets whether or not the toolbar should draw as if it's being captured for a snapshot
     * texture.  In this mode it will only draw the toolbar in it's normal state (no TabSwitcher
     * or animations).
     * @param textureMode Whether or not to be in texture capture mode.
     */
    void setTextureCaptureMode(boolean textureMode);

    /**
     * @return Whether a dirty check for invalidation makes sense at this time.
     */
    boolean isReadyForTextureCapture();
}
