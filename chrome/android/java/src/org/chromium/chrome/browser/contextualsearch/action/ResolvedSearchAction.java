// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch.action;

import org.chromium.chrome.browser.contextualsearch.gesture.SearchGestureHost;

/**
 * Represents a Search Action that will gather and examine surrounding text in order to
 * "resolve" what to search for.
 * Right now this class is just being used to extract surrounding context for a tap
 * action before issuing the resolution request to determine if we want to suppress
 * the tap. Longer term it will actually do the resolution request & search request.
 * This is part of the 2016-refactoring (crbug.com/624609, go/cs-refactoring-2016).
 */
public class ResolvedSearchAction extends SearchAction {
    // ============================================================================================
    // Constructor
    // ============================================================================================

    /**
     * Constructs a Search Action that extracts context for a Tap, using the given listener and
     * host. Current implementation is limited to gathering the text surrounding a Tap gesture in
     * order to determine whether a search should be done or not.
     * @param listener The object to notify when the {@link SearchAction} state changes.
     * @param host The host that provides environment data to the {@link SearchAction}.
     */
    public ResolvedSearchAction(SearchActionListener listener, SearchGestureHost host) {
        super(listener, host);
    }

    // ============================================================================================
    // Abstract implementations
    // ============================================================================================

    @Override
    public void extractContext() {
        requestSurroundingText();
    }

    // ============================================================================================
    // State handling
    // ============================================================================================

    @Override
    protected void onSurroundingTextReady() {
        super.onSurroundingTextReady();

        notifyContextReady();
    }
}
