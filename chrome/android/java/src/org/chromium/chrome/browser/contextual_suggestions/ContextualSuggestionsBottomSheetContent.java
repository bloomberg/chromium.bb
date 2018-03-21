// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import android.view.View;

import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.BottomSheetContent;

/** A {@link BottomSheetContent} that displays contextual suggestions. */
public class ContextualSuggestionsBottomSheetContent implements BottomSheetContent {
    private ContentCoordinator mContentCoordinator;

    /**
     * Construct a new {@link ContextualSuggestionsBottomSheetContent}.
     * @param contentCoordinator The {@link ContentCoordinator} that manages content to be
     *                           displayed.
     */
    ContextualSuggestionsBottomSheetContent(ContentCoordinator contentCoordinator) {
        mContentCoordinator = contentCoordinator;
    }

    @Override
    public View getContentView() {
        return mContentCoordinator.getView();
    }

    @Override
    public View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return mContentCoordinator.getVerticalScrollOffset();
    }

    @Override
    public void destroy() {
        mContentCoordinator = null;
    }

    @Override
    public boolean applyDefaultTopPadding() {
        return false;
    }
}
