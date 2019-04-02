// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.view.KeyEvent;

import org.chromium.ui.modelutil.PropertyModel;

/** A controller for touchless UI. */
public interface TouchlessUiController {
    /**
     * Add a model to the queue to be shown.
     * @param model The model to add.
     */
    default void addModelToQueue(PropertyModel model) {}

    /**
     * Remove a model from the queue to be shown.
     * @param model The model to remove.
     */
    default void removeModelFromQueue(PropertyModel model) {}

    /**
     * A notification that a key event occurred.
     * @param event The event object.
     * @return Whether the event was consumed.
     */
    boolean onKeyEvent(KeyEvent event);

    /**
     * Clean up anything that needs to be.
     */
    void destroy();
}
