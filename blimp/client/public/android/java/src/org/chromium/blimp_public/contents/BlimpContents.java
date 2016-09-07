// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp_public.contents;

import android.view.ViewGroup;

/**
 * BlimpContents is the Java representation of a native BlimpContents object.
 *
 * BlimpContents is the core class in //blimp/client/core. It renders web pages from an engine in a
 * rectangular area. It enables callers to control the blimp engine through the use of the
 * navigation controller.
 */
public interface BlimpContents {
    /**
     * Retrieves the {@link BlimpNavigationController} that controls all navigation related
     * to this BlimpContents.
     */
    BlimpNavigationController getNavigationController();

    /**
     * Returns the theme color for Blimp tab.
     */
    int getThemeColor();

    /**
     * Adds an observer to this BlimpContents.
     */
    void addObserver(BlimpContentsObserver observer);

    /**
     * Removes an observer from this BlimpContents.
     */
    void removeObserver(BlimpContentsObserver observer);

    /**
     * Returns a view that represents the content for this BlimpContents.
     */
    ViewGroup getView();

    /**
     * For BlimpContents that are owned by Java, this must be called before this BlimpContents is
     * garbage collected.
     */
    void destroy();

    /**
     * Shows this BlimpContents.
     */
    void show();

    /**
     * Hide this BlimpContents.
     */
    void hide();
}
