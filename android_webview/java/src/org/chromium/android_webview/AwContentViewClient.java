// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.view.KeyEvent;
import android.view.View;
import android.webkit.URLUtil;
import android.webkit.WebChromeClient;
import android.widget.FrameLayout;

import org.chromium.content.browser.ContentVideoViewClient;
import org.chromium.content.browser.ContentViewClient;
import org.chromium.content.browser.SelectActionMode;
import org.chromium.content.browser.SelectActionModeCallback.ActionHandler;

/**
 * ContentViewClient implementation for WebView
 */
public class AwContentViewClient extends ContentViewClient implements ContentVideoViewClient {
    private final AwContentsClient mAwContentsClient;
    private final AwSettings mAwSettings;
    private final AwContents mAwContents;
    private final Context mContext;
    private FrameLayout mCustomView;

    public AwContentViewClient(AwContentsClient awContentsClient, AwSettings awSettings,
            AwContents awContents, Context context) {
        mAwContentsClient = awContentsClient;
        mAwSettings = awSettings;
        mAwContents = awContents;
        mContext = context;
    }

    @Override
    public void onBackgroundColorChanged(int color) {
        mAwContentsClient.onBackgroundColorChanged(color);
    }

    @Override
    public void onStartContentIntent(Context context, String contentUrl) {
        if (mAwContentsClient.hasWebViewClient()) {
            //  Callback when detecting a click on a content link.
            mAwContentsClient.shouldOverrideUrlLoading(contentUrl);
            return;
        }

        AwContentsClient.sendBrowsingIntent(context, contentUrl);
    }

    @Override
    public void onUpdateTitle(String title) {
        mAwContentsClient.updateTitle(title, true);
    }

    @Override
    public boolean shouldOverrideKeyEvent(KeyEvent event) {
        if (mAwContentsClient.hasWebViewClient()) {
            // The check below is reflecting Chrome's behavior and is a workaround for
            // http://b/7697782.
            if (!ContentViewClient.shouldPropagateKey(event.getKeyCode())) return true;
            return mAwContentsClient.shouldOverrideKeyEvent(event);
        }

        return super.shouldOverrideKeyEvent(event);
    }

    @Override
    public SelectActionMode startActionMode(
            View view, ActionHandler actionHandler, boolean floating) {
        return mAwContentsClient.startActionMode(view, actionHandler, floating);
    }

    @Override
    public boolean supportsFloatingActionMode() {
        return mAwContentsClient.supportsFloatingActionMode();
    }

    @Override
    public final ContentVideoViewClient getContentVideoViewClient() {
        return this;
    }

    @Override
    public boolean shouldBlockMediaRequest(String url) {
        return mAwSettings != null
                ? mAwSettings.getBlockNetworkLoads() && URLUtil.isNetworkUrl(url) : true;
    }

    @Override
    public void enterFullscreenVideo(View videoView) {
        if (mCustomView == null) {
            // enterFullscreenVideo will only be called after enterFullscreen, but
            // in this case exitFullscreen has been invoked in between them.
            // TODO(igsolla): Fix http://crbug/425926 and replace with assert.
            return;
        }
        mCustomView.addView(videoView, 0);
    }

    @Override
    public void exitFullscreenVideo() {
        // Intentional no-op
    }

    @Override
    public View getVideoLoadingProgressView() {
        return mAwContentsClient.getVideoLoadingProgressView();
    }

    @Override
    public void setSystemUiVisibility(boolean enterFullscreen) {
    }

    /**
     * Called to show the web contents in fullscreen mode.
     *
     * <p>If entering fullscreen on a video element the web contents will contain just
     * the html5 video controls. {@link #enterFullscreenVideo(View)} will be called later
     * once the ContentVideoView, which contains the hardware accelerated fullscreen video,
     * is ready to be shown.
     */
    public void enterFullscreen() {
        if (mAwContents.isFullScreen()) {
            return;
        }
        View fullscreenView = mAwContents.enterFullScreen();
        if (fullscreenView == null) {
            return;
        }
        WebChromeClient.CustomViewCallback cb = new WebChromeClient.CustomViewCallback() {
            @Override
            public void onCustomViewHidden() {
                if (mCustomView != null) {
                    mAwContents.requestExitFullscreen();
                }
            }
        };
        mCustomView = new FrameLayout(mContext);
        mCustomView.addView(fullscreenView);
        mAwContentsClient.onShowCustomView(mCustomView, cb);
    }

    /**
     * Called to show the web contents in embedded mode.
     */
    public void exitFullscreen() {
        if (mCustomView != null) {
            mCustomView = null;
            mAwContents.exitFullScreen();
            mAwContentsClient.onHideCustomView();
        }
    }

    @Override
    public boolean isExternalScrollActive() {
        return mAwContents.isSmoothScrollingActive();
    }
}
