// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import android.app.Activity;
import android.content.Intent;

import java.net.URISyntaxException;

/**
 * Stores information related to a Contextual Search "quick action."
 */
public class ContextualSearchQuickActionControl {
    private String mQuickActionUri;

    /**
     * @param quickActionUri The URI for the intent associated with the quick action. If the URI is
     *                       the empty string or cannot be parsed no quick action will be available.
     * @param quickActionCategory The category for the quick action.
     */
    public void setQuickAction(String quickActionUri, String quickActionCategory) {
        mQuickActionUri = quickActionUri;

        // TODO(twellington): resolve the intent to determine what caption and icon to use.
    }

    /**
     * Sends the intent associated with the quick action if one is available.
     * @param activity The {@link Activity} used to send the intent.
     */
    public void sendIntent(Activity activity) {
        // TODO(twellington): use resolved intent rather than parsing mQuickActionUri directly.
        Intent intent;
        try {
            intent = Intent.parseUri(mQuickActionUri, 0);
        } catch (URISyntaxException e) {
            // Ignore parsing errors.
            return;
        }
        activity.startActivity(intent);
    }

    /**
     * @return The caption associated with the quick action or null if no quick action is
     *         available.
     */
    public String getCaption() {
        // TODO(twellington): Use quickActionCategory to determine the caption string.
        return null;
    }

    /**
     * @return The resource id for the icon associated with the quick action or 0 if no quick
     *         action is available.
     */
    public int getIconResId() {
        // TODO(twellington): Implement associating a drawable resource or app icon with the
        // the quick action. This icon res id should eventually get set on
        // ContextualSearchImageControl which needs to be refactored to include support for a
        // static icon.
        return 0;
    }

    /**
     * @return Whether there is currently a quick action available.
     */
    public boolean hasQuickAction() {
        // TODO(twellington): Determine if a quick action is available by resolving the intent.
        return false;
    }

    /**
     * Resets quick action data.
     */
    public void reset() {
        mQuickActionUri = "";
    }
}
