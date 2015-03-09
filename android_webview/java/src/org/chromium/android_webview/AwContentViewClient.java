// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.provider.Browser;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.webkit.URLUtil;
import android.webkit.WebChromeClient;
import android.widget.FrameLayout;

import org.chromium.content.browser.ContentVideoViewClient;
import org.chromium.content.browser.ContentViewClient;
import org.chromium.content.browser.ContentViewCore;

import java.net.URISyntaxException;

/**
 * ContentViewClient implementation for WebView
 */
public class AwContentViewClient extends ContentViewClient implements ContentVideoViewClient {
    private static final String TAG = "AwContentViewClient";

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

        Intent intent;
        // Perform generic parsing of the URI to turn it into an Intent.
        try {
            intent = Intent.parseUri(contentUrl, Intent.URI_INTENT_SCHEME);
        } catch (URISyntaxException ex) {
            Log.w(TAG, "Bad URI " + contentUrl + ": " + ex.getMessage());
            return;
        }
        // Sanitize the Intent, ensuring web pages can not bypass browser
        // security (only access to BROWSABLE activities).
        intent.addCategory(Intent.CATEGORY_BROWSABLE);
        intent.setComponent(null);
        Intent selector = intent.getSelector();
        if (selector != null) {
            selector.addCategory(Intent.CATEGORY_BROWSABLE);
            selector.setComponent(null);
        }
        // Pass the package name as application ID so that the intent from the
        // same application can be opened in the same tab.
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        if (ContentViewCore.activityFromContext(context) == null) {
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        }

        try {
            context.startActivity(intent);
        } catch (ActivityNotFoundException ex) {
            Log.w(TAG, "No application can handle " + contentUrl);
        }
    }

    @Override
    public void onUpdateTitle(String title) {
        mAwContentsClient.onReceivedTitle(title);
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
    public boolean isJavascriptEnabled() {
        return mAwSettings != null && mAwSettings.getJavaScriptEnabled();
    }

    @Override
    public boolean isExternalFlingActive() {
        return mAwContents.isFlingActive();
    }
}
