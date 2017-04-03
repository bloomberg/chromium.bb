// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;

/** Implementation of the {@link LocationBarLayout} that is displayed for widget searches. */
class SearchLocationBarLayout extends LocationBarLayout {
    /** Delegates calls out to the containing Activity. */
    public static interface Delegate {
        /** Load a URL in the associated tab. */
        void loadUrl(String url);

        /** The user hit the back button. */
        void backKeyPressed();
    }

    private Delegate mDelegate;

    public SearchLocationBarLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        findViewById(R.id.google_g_container).setVisibility(View.GONE);

        // TODO(dfalcantara|yusufo): Remove once upstream and the bar is inflated from XML.
        onFinishInflate();
    }

    /** Set the {@link Delegate}. */
    void setDelegate(Delegate delegate) {
        mDelegate = delegate;
    }

    @Override
    protected void loadUrl(String url, int transition) {
        mDelegate.loadUrl(url);
    }

    @Override
    protected void backKeyPressed() {
        mDelegate.backKeyPressed();
    }

    @Override
    public boolean mustQueryUrlBarLocationForSuggestions() {
        return true;
    }

    @Override
    public void setUrlToPageUrl() {
        // Explicitly do nothing.  The tab is invisible, so showing its URL would be confusing.
    }
}
