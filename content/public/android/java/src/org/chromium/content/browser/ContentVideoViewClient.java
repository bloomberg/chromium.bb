// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.view.View;

/**
 *  Main callback class used by {@link ContentVideoView}.
 *
 *  This contains the superset of callbacks that must be implemented by the embedder
 *  to support fullscreen video.
 *
 *  {@link #enterFullscreenVideo(View)} and {@link #exitFullscreenVideo()} must be implemented,
 *  {@link #getVideoLoadingProgressView()} is optional, and may return null if not required.
 *
 *  The implementer is responsible for displaying the Android view when
 *  {@link #enterFullscreenVideo(View)} is called.
 */
public interface ContentVideoViewClient {
    /**
     * Called when the {@link ContentVideoView}, which contains the fullscreen video,
     * is ready to be shown. Must be implemented.
     * @param view The view containing the fullscreen video that embedders must show.
     */
    public void enterFullscreenVideo(View view);

    /**
     * Called to exit fullscreen video. Embedders must stop showing the view given in
     * {@link #enterFullscreenVideo(View)}. Must be implemented.
     */
    public void exitFullscreenVideo();

    /**
     * Allows the embedder to replace the view indicating that the video is loading.
     * If null is returned, the default video loading view is used.
     */
    public View getVideoLoadingProgressView();

    /**
     * Sets the system ui visibility after entering or exiting fullscreen.
     * @param enterFullscreen True if video is going fullscreen, or false otherwise.
     */
    public void setSystemUiVisibility(boolean enterFullscreen);
}
