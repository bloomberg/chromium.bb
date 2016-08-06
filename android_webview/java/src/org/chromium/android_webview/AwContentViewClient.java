// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.content.Intent;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.URLUtil;
import android.webkit.WebChromeClient;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import org.chromium.content.browser.ContentVideoViewEmbedder;
import org.chromium.content.browser.ContentViewClient;

/**
 * ContentViewClient implementation for WebView
 */
public class AwContentViewClient extends ContentViewClient implements ContentVideoViewEmbedder {
    private final AwContentsClient mAwContentsClient;
    private final AwSettings mAwSettings;
    private final AwContents mAwContents;
    private final Context mContext;
    private FrameLayout mCustomView;
    private View mProgressView;

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
    public void onStartContentIntent(Context context, String contentUrl, boolean isMainFrame) {
        // Comes from WebViewImpl::detectContentOnTouch in Blink, so must be user-initiated, and
        // isn't a redirect.
        mAwContentsClient.shouldIgnoreNavigation(context, contentUrl, isMainFrame, true, false);
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
    public final ContentVideoViewEmbedder getContentVideoViewEmbedder() {
        return this;
    }

    @Override
    public boolean shouldBlockMediaRequest(String url) {
        return mAwSettings != null
                ? mAwSettings.getBlockNetworkLoads() && URLUtil.isNetworkUrl(url) : true;
    }

    @Override
    public void enterFullscreenVideo(View videoView, boolean isVideoLoaded) {
        if (mCustomView == null) {
            // enterFullscreenVideo will only be called after enterFullscreen, but
            // in this case exitFullscreen has been invoked in between them.
            // TODO(igsolla): Fix http://crbug/425926 and replace with assert.
            return;
        }

        mCustomView.addView(videoView, 0);

        if (isVideoLoaded) return;

        mProgressView = mAwContentsClient.getVideoLoadingProgressView();
        if (mProgressView == null) {
            mProgressView = new ProgressView(mContext);
        }
        mCustomView.addView(
                mProgressView, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT,
                                       ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.CENTER));
    }

    @Override
    public void fullscreenVideoLoaded() {
        if (mCustomView == null) return;

        if (mProgressView != null) {
            mCustomView.removeView(mProgressView);
            mProgressView = null;
        }
    }

    @Override
    public void exitFullscreenVideo() {
        // Intentional no-op
    }

    @Override
    public void setSystemUiVisibility(boolean enterFullscreen) {
    }

    @Override
    public boolean doesPerformProcessText() {
        return true;
    }

    @Override
    public void startProcessTextIntent(Intent intent) {
        mAwContents.startProcessTextIntent(intent);
    }

    @Override
    public boolean isSelectActionModeAllowed(int actionModeItem) {
        return mAwContents.isSelectActionModeAllowed(actionModeItem);
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
            mProgressView = null;
            mAwContents.exitFullScreen();
            mAwContentsClient.onHideCustomView();
        }
    }

    @Override
    public String getProductVersion() {
        return AwContentsStatics.getProductVersion();
    }

    private static class ProgressView extends LinearLayout {
        private final ProgressBar mProgressBar;
        private final TextView mTextView;

        public ProgressView(Context context) {
            super(context);
            setOrientation(LinearLayout.VERTICAL);
            setLayoutParams(new LinearLayout.LayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT));
            mProgressBar = new ProgressBar(context, null, android.R.attr.progressBarStyleLarge);
            mTextView = new TextView(context);

            String videoLoadingText = context.getString(
                    org.chromium.android_webview.R.string.media_player_loading_video);

            mTextView.setText(videoLoadingText);
            addView(mProgressBar);
            addView(mTextView);
        }
    }
}
