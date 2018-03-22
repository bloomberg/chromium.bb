// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import android.view.View;

import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.BottomSheetContent;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.ContentPriority;

/** A {@link BottomSheetContent} that displays contextual suggestions. */
public class ContextualSuggestionsBottomSheetContent implements BottomSheetContent {
    private final ContentCoordinator mContentCoordinator;
    private final ToolbarCoordinator mToolbarCoordinator;

    /**
     * Construct a new {@link ContextualSuggestionsBottomSheetContent}.
     * @param contentCoordinator The {@link ContentCoordinator} that manages content to be
     *                           displayed.
     * @param toolbarCoordinator The {@link ToolbarCoordinator} that manages the toolbar to be
     *                           displayed.
     */
    ContextualSuggestionsBottomSheetContent(
            ContentCoordinator contentCoordinator, ToolbarCoordinator toolbarCoordinator) {
        mContentCoordinator = contentCoordinator;
        mToolbarCoordinator = toolbarCoordinator;
    }

    @Override
    public View getContentView() {
        return mContentCoordinator.getView();
    }

    @Override
    public View getToolbarView() {
        return mToolbarCoordinator.getView();
    }

    @Override
    public int getVerticalScrollOffset() {
        return mContentCoordinator.getVerticalScrollOffset();
    }

    @Override
    public void destroy() {}

    @Override
    public boolean applyDefaultTopPadding() {
        return false;
    }

    @Override
    public @ContentPriority int getPriority() {
        return ContentPriority.LOW;
    }
}
