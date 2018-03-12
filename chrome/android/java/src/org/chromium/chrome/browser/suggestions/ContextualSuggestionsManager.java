// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;

/**
 * Manages showing contextual suggestions in the {@link BottomSheet}.
 */
public class ContextualSuggestionsManager {
    SuggestionsBottomSheetContent mBottomSheetContent;

    public ContextualSuggestionsManager(final ChromeActivity activity, final BottomSheet sheet,
            TabModelSelector tabModelSelector, SnackbarManager snackbarManager) {
        // TODO(twellington): Replace with bottom sheet content that only shows contextual
        // suggestions.
        mBottomSheetContent = new SuggestionsBottomSheetContent(
                activity, sheet, tabModelSelector, snackbarManager);
        sheet.showContent(mBottomSheetContent);
    }
}
