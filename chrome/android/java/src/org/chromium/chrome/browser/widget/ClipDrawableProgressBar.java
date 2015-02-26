// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.ImageView;

/**
 * An alternative progress bar implemented using ClipDrawable. We're experimenting with this for
 * faster performance. http://crbug.com/455891
 */
public class ClipDrawableProgressBar extends ImageView {
    private static final long PROGRESS_CLEARING_DELAY_MS = 200;

    private final Runnable mClearLoadProgressRunnable = new Runnable() {
        @Override
        public void run() {
            setProgress(0);
        }
    };

    /**
     * Constructor for inflating from XML.
     */
    public ClipDrawableProgressBar(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Set the current progress to the specified value.
     * @param progress The new progress, between 0 and 10000.
     */
    public void setProgress(int progress) {
        getDrawable().setLevel(progress);

        removeCallbacks(mClearLoadProgressRunnable);
        if (progress == 10000) postDelayed(mClearLoadProgressRunnable, PROGRESS_CLEARING_DELAY_MS);
    }

    /**
     * Get the progress bar's current level of progress.
     * @return The current progress, between 0 and getMax().
     */
    public int getProgress() {
        return getDrawable().getLevel();
    }
}
