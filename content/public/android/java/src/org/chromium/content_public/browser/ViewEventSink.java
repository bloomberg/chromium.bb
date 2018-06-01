// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.content.res.Configuration;

import org.chromium.content.browser.ViewEventSinkImpl;

/**
 * Interface for updating content with view events.
 */
public interface ViewEventSink {
    /**
     * @return {@link ViewEventSink} instance for a given {@link WebContents}.
     */
    public static ViewEventSink from(WebContents webContents) {
        return ViewEventSinkImpl.from(webContents);
    }

    /**
     * @see View#onAttachedToWindow()
     */
    void onAttachedToWindow();

    /**
     * @see View#onDetachedFromWindow()
     */
    void onDetachedFromWindow();

    /**
     * @see View#onWindowFocusChanged(boolean)
     */
    void onWindowFocusChanged(boolean hasWindowFocus);

    /**
     * Called when view-level focus for the container view has changed.
     * @param gainFocus {@code true} if the focus is gained, otherwise {@code false}.
     */
    void onViewFocusChanged(boolean gainFocus);

    /**
     * Sets whether the keyboard should be hidden when losing input focus.
     * @param hideKeyboardOnBlur {@code true} if we should hide soft keyboard when losing focus.
     */
    void setHideKeyboardOnBlur(boolean hideKeyboardOnBlur);

    /**
     * @see View#onConfigurationChanged(Configuration)
     */
    void onConfigurationChanged(Configuration newConfig);

    void onPauseForTesting();
    void onResumeForTesting();
}
