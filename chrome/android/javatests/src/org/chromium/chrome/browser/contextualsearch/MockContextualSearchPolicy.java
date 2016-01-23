// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.content.Context;

import org.chromium.chrome.browser.contextualsearch.ContextualSearchSelectionController.SelectionType;

/**
 * A mock ContextualSearchPolicy class that excludes any business logic.
 * TODO(mdjones): Allow the return values of these function to be set.
 */
public class MockContextualSearchPolicy extends ContextualSearchPolicy {
    public MockContextualSearchPolicy(Context context) {
        super(context);
    }

    @Override
    public boolean shouldPrefetchSearchResult(boolean isTapTriggered) {
        return isTapTriggered;
    }

    @Override
    public boolean maySendBasePageUrl() {
        return false;
    }

    @Override
    public boolean isPeekPromoConditionSatisfied(
            ContextualSearchSelectionController controller) {
        return false;
    }

    @Override
    public boolean shouldAnimateSearchProviderIcon(SelectionType selectionType,
            boolean isShowing) {
        return false;
    }

    @Override
    public boolean isPromoAvailable() {
        return false;
    }
}
