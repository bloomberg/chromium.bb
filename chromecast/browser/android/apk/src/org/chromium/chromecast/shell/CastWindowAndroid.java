// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.support.v4.content.LocalBroadcastManager;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Log;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.content.browser.ContentView;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.ContentViewRenderView;
import org.chromium.content.browser.WebContentsObserverAndroid;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Container for the various UI components that make up a shell window.
 */
@JNINamespace("chromecast::shell")
public class CastWindowAndroid extends LinearLayout {
    public static final String TAG = "CastWindowAndroid";

    public static final String ACTION_PAGE_LOADED = "castPageLoaded";
    public static final String ACTION_ENABLE_DEV_TOOLS = "castEnableDevTools";
    public static final String ACTION_DISABLE_DEV_TOOLS = "castDisableDevTools";

    private ContentViewCore mContentViewCore;
    private ContentViewRenderView mContentViewRenderView;
    private NavigationController mNavigationController;
    private WebContents mWebContents;
    private WebContentsObserverAndroid mWebContentsObserver;
    private WindowAndroid mWindow;

    /**
     * Constructor for inflating via XML.
     */
    public CastWindowAndroid(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Set the SurfaceView being renderered to as soon as it is available.
     */
    public void setContentViewRenderView(ContentViewRenderView contentViewRenderView) {
        FrameLayout contentViewHolder = (FrameLayout) findViewById(R.id.contentview_holder);
        if (contentViewRenderView == null) {
            if (mContentViewRenderView != null) {
                contentViewHolder.removeView(mContentViewRenderView);
            }
        } else {
            contentViewHolder.addView(contentViewRenderView,
                    new FrameLayout.LayoutParams(
                            FrameLayout.LayoutParams.MATCH_PARENT,
                            FrameLayout.LayoutParams.MATCH_PARENT));
        }
        mContentViewRenderView = contentViewRenderView;
    }

    /**
     * @param window The owning window for this shell.
     */
    public void setWindow(WindowAndroid window) {
        mWindow = window;
    }

    /**
     * Loads an URL.  This will perform minimal amounts of sanitizing of the URL to attempt to
     * make it valid.
     *
     * @param url The URL to be loaded by the shell.
     */
    public void loadUrl(String url) {
        if (url == null) return;

        if (TextUtils.equals(url, mWebContents.getUrl())) {
            mNavigationController.reload(true);
        } else {
            mNavigationController.loadUrl(new LoadUrlParams(normalizeUrl(url)));
        }

        // TODO(aurimas): Remove this when crbug.com/174541 is fixed.
        mContentViewCore.getContainerView().clearFocus();
        mContentViewCore.getContainerView().requestFocus();
    }

    /**
     * Given a URI String, performs minimal normalization to attempt to build a usable URL from it.
     * @param uriString The passed-in path to be normalized.
     * @return The normalized URL, as a string.
     */
    private static String normalizeUrl(String uriString) {
        if (uriString == null) return uriString;
        Uri uri = Uri.parse(uriString);
        if (uri.getScheme() == null) {
            uri = Uri.parse("http://" + uriString);
        }
        return uri.toString();
    }

    /**
     * Initializes the ContentView based on the native tab contents pointer passed in.
     * @param nativeWebContents The pointer to the native tab contents object.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void initFromNativeWebContents(long nativeWebContents) {
        Context context = getContext();
        mContentViewCore = new ContentViewCore(context);
        ContentView view = ContentView.newInstance(context, mContentViewCore);
        mContentViewCore.initialize(view, view, nativeWebContents, mWindow);
        mWebContents = mContentViewCore.getWebContents();
        mNavigationController = mWebContents.getNavigationController();

        if (getParent() != null) mContentViewCore.onShow();
        ((FrameLayout) findViewById(R.id.contentview_holder)).addView(view,
                new FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.MATCH_PARENT));
        view.requestFocus();
        mContentViewRenderView.setCurrentContentViewCore(mContentViewCore);

        mWebContentsObserver = new WebContentsObserverAndroid(mWebContents) {
            @Override
            public void didStopLoading(String url) {
                Uri intentUri = Uri.parse(mNavigationController
                        .getOriginalUrlForVisibleNavigationEntry());
                Log.v(TAG, "Broadcast ACTION_PAGE_LOADED: scheme=" + intentUri.getScheme()
                        + ", host=" + intentUri.getHost());
                LocalBroadcastManager.getInstance(getContext()).sendBroadcast(
                        new Intent(ACTION_PAGE_LOADED, intentUri));
            }
        };
    }

    /**
     * @return The {@link ViewGroup} currently shown by this Shell.
     */
    public ViewGroup getContentView() {
        return mContentViewCore.getContainerView();
    }

    /**
     * @return The {@link ContentViewCore} currently managing the view shown by this Shell.
     */
    public ContentViewCore getContentViewCore() {
        return mContentViewCore;
    }
}
