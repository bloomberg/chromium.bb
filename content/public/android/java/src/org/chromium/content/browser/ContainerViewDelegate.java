// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.view.View;

/**
 * Interface to add and remove views from the implementing view.
 */
public interface ContainerViewDelegate {

    /**
     * Add the view.
     * @param view The view to be added.
     */
    public void addViewToContainerView(View view);

    /**
     * Remove the view if it is present, otherwise do nothing.
     * @param view The view to be removed.
     */
    public void removeViewFromContainerView(View view);

    /**
     * Used for any calculations that need to place a View near a particular piece of web content.
     * @return The Y offset in pixels to apply to any added views.
     */
    public int getChildViewOffsetYPix();
}