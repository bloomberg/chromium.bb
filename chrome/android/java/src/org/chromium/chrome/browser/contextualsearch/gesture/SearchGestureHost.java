// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch.gesture;

import org.chromium.content_public.browser.WebContents;

/**
 * Provides the host interface for Search Gestures, which can create Search Actions.
 * When a gesture is recognized that it should trigger a search, this interface is used
 * to communicate back to the host
 * This is part of the 2016-refactoring (crbug.com/624609, go/cs-refactoring-2016).
 */
public interface SearchGestureHost {
    /**
     * @return the {@link WebContents} for the current base tab.
     */
    WebContents getTabWebContents();

    /**
     * Tells the host that the current gesture should be dismissed because processing is complete.
     */
    void dismissGesture();
}
