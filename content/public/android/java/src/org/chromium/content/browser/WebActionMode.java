// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.annotation.TargetApi;
import android.os.Build;
import android.view.ActionMode;
import android.view.View;
import android.view.ViewConfiguration;

import org.chromium.base.Log;

/**
 * An ActionMode for in-page web content selection. This class wraps an ActionMode created
 * by the associated View, providing modified interaction with that ActionMode.
 */
@TargetApi(Build.VERSION_CODES.M)
public class WebActionMode {
    private static final String TAG = "cr.WebActionMode";

    // Default delay for reshowing the {@link ActionMode} after it has been
    // hidden. This avoids flickering issues if there are trailing rect
    // invalidations after the ActionMode is shown. For example, after the user
    // stops dragging a selection handle, in turn showing the ActionMode, the
    // selection change response will be asynchronous. 300ms should accomodate
    // most such trailing, async delays.
    private static final int SHOW_DELAY_MS = 300;

    protected final ActionMode mActionMode;
    private final View mView;
    private boolean mHidden;
    private boolean mPendingInvalidateContentRect;

    // Self-repeating task that repeatedly hides the ActionMode. This is
    // required because ActionMode only exposes a temporary hide routine.
    private final Runnable mRepeatingHideRunnable;

    /**
     * Constructs a SelectActionMode instance wrapping a concrete ActionMode.
     * @param actionMode the wrapped ActionMode.
     * @param view the associated View.
     */
    public WebActionMode(ActionMode actionMode, View view) {
        assert actionMode != null;
        assert view != null;
        mActionMode = actionMode;
        mView = view;
        mRepeatingHideRunnable = new Runnable() {
            @Override
            public void run() {
                assert mHidden;
                final long hideDuration = getDefaultHideDuration();
                // Ensure the next hide call occurs before the ActionMode reappears.
                mView.postDelayed(mRepeatingHideRunnable, hideDuration - 1);
                hideTemporarily(hideDuration);
            }
        };
    }

    /**
     * @see ActionMode#finish()
     */
    public void finish() {
        mActionMode.finish();
    }

    /**
     * @see ActionMode#invalidate()
     * Note that invalidation will also reset visibility state. The caller
     * should account for this when making subsequent visibility updates.
     */
    public void invalidate() {
        if (mHidden) {
            assert canHide();
            mHidden = false;
            mView.removeCallbacks(mRepeatingHideRunnable);
            mPendingInvalidateContentRect = false;
        }

        // Try/catch necessary for framework bug, crbug.com/446717.
        try {
            mActionMode.invalidate();
        } catch (NullPointerException e) {
            Log.w(TAG, "Ignoring NPE from ActionMode.invalidate() as workaround for L", e);
        }
    }

    /**
     * @see ActionMode#invalidateContentRect()
     */
    public void invalidateContentRect() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            if (mHidden) {
                mPendingInvalidateContentRect = true;
            } else {
                mPendingInvalidateContentRect = false;
                mActionMode.invalidateContentRect();
            }
        }
    }

    /**
     * @see ActionMode#onWindowFocusChanged()
     */
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            mActionMode.onWindowFocusChanged(hasWindowFocus);
        }
    }

    /**
     * Hide or reveal the ActionMode. Note that this only has visible
     * side-effects if the underlying ActionMode supports hiding.
     * @param hide whether to hide or show the ActionMode.
     */
    public void hide(boolean hide) {
        if (!canHide()) return;
        if (mHidden == hide) return;
        mHidden = hide;
        if (mHidden) {
            mRepeatingHideRunnable.run();
        } else {
            mHidden = false;
            mView.removeCallbacks(mRepeatingHideRunnable);
            hideTemporarily(SHOW_DELAY_MS);
            if (mPendingInvalidateContentRect) {
                mPendingInvalidateContentRect = false;
                invalidateContentRect();
            }
        }
    }

    /**
     * @see ActionMode#hide(long)
     */
    private void hideTemporarily(long duration) {
        assert canHide();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            mActionMode.hide(duration);
        }
    }

    private boolean canHide() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            return mActionMode.getType() == ActionMode.TYPE_FLOATING;
        }
        return false;
    }

    private long getDefaultHideDuration() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            return ViewConfiguration.getDefaultActionModeHideDuration();
        }
        return 2000;
    }
}
