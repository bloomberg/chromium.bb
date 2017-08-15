// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

/**
 * Describes a portion of UI responsible for rendering a group of sites. It abstracts general tasks
 * related to initialising and updating this UI.
 */
interface SiteSectionView {
    /** Initialise the view, letting it know the data it will have to display. */
    void bindDataSource(TileGroup tileGroup, TileRenderer tileRenderer);

    /** Clears the current data and displays the current state of the model. */
    void refreshData();

    /** Updates the icon shown for the provided tile. */
    void updateIconView(Tile tile);

    /** Updates the offline badge for the provided tile. */
    void updateOfflineBadge(Tile tile);
}
