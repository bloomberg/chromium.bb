// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.graphics.Rect;
import android.view.ViewTreeObserver;

/**
 * A CursorController instance can be used to control a cursor in the text.
 */
abstract class CursorController implements ViewTreeObserver.OnTouchModeChangeListener {

    private Rect mVisibleClippingRectangle;

    /**
     * Hide the cursor controller from screen.
     */
    abstract void hide();

    /**
     * @return true if the CursorController is currently visible
     */
    abstract boolean isShowing();

    /**
     * Called when the handle is about to start updating its position.
     * @param handle
     */
    abstract void beforeStartUpdatingPosition(HandleView handle);

    /**
     * Update the controller's position.
     */
    abstract void updatePosition(HandleView handle, int x, int y);

    /**
     * Called when the view is detached from window. Perform house keeping task, such as
     * stopping Runnable thread that would otherwise keep a reference on the context, thus
     * preventing the activity to be recycled.
     */
    abstract void onDetached();

    /**
     * Sets the visible rectangle for text input elements as supplied by Blink.
     */
    public void setVisibleClippingRectangle(int left, int top, int right, int bottom) {
        if (mVisibleClippingRectangle == null) {
            mVisibleClippingRectangle = new Rect(left, top, right, bottom);
        } else {
            mVisibleClippingRectangle.set(left,top,right,bottom);
        }
    }

    Rect getVisibleClippingRectangle() {
        return mVisibleClippingRectangle;
    }

}
