// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.os.AsyncTask;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.prerender.ExternalPrerenderHandler;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;

import java.net.InetAddress;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.UnknownHostException;

/**
 * This class is a singleton that holds utilities for warming up Chrome and prerendering urls
 * without creating the Activity.
 */
public final class WarmupManager {
    private static WarmupManager sWarmupManager;

    private WebContents mPrerenderedWebContents;
    private boolean mPrerendered;
    private ViewGroup mMainView;
    private ExternalPrerenderHandler mExternalPrerenderHandler;

    /**
     * @return The singleton instance for the WarmupManager, creating one if necessary.
     */
    public static WarmupManager getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sWarmupManager == null) sWarmupManager = new WarmupManager();
        return sWarmupManager;
    }

    private WarmupManager() {
    }

    /**
     * Check whether prerender manager has the given url prerendered. This also works with
     * redirected urls.
     *
     * Uses the last used profile.
     *
     * @param url The url to check.
     * @return Whether the given url has been prerendered.
     */
    public boolean hasPrerenderedUrl(String url) {
        return hasAnyPrerenderedUrl() && ExternalPrerenderHandler.hasPrerenderedUrl(
                Profile.getLastUsedProfile(), url, mPrerenderedWebContents);
    }

    /**
     * @return Whether any url has been prerendered.
     */
    public boolean hasAnyPrerenderedUrl() {
        return mPrerendered;
    }

    /**
     * @return The prerendered {@link WebContents} clearing out the reference WarmupManager owns.
     */
    public WebContents takePrerenderedWebContents() {
        WebContents prerenderedWebContents = mPrerenderedWebContents;
        assert (mPrerenderedWebContents != null);
        mPrerenderedWebContents = null;
        return prerenderedWebContents;
    }

    /**
     * Prerenders the given url using the prerender_manager.
     *
     * Uses the last used profile.
     *
     * @param url The url to prerender.
     * @param referrer The referrer url to be used while prerendering
     * @param widthPix The width in pixels to which the page should be prerendered.
     * @param heightPix The height in pixels to which the page should be prerendered.
     */
    public void prerenderUrl(final String url, final String referrer,
            final int widthPix, final int heightPix) {
        clearWebContentsIfNecessary();
        if (mExternalPrerenderHandler == null) {
            mExternalPrerenderHandler = new ExternalPrerenderHandler();
        }

        mPrerenderedWebContents = mExternalPrerenderHandler.addPrerender(
                Profile.getLastUsedProfile(), url, referrer, widthPix, heightPix);
        if (mPrerenderedWebContents != null) mPrerendered = true;
    }

    /**
     * Inflates and constructs the view hierarchy that the app will use.
     * @param baseContext The base context to use for creating the ContextWrapper.
     * @param themeId Id of the main theme to use in the inflated views.
     * @param layoutId Id of the layout to inflate.
     */
    public void initializeViewHierarchy(Context baseContext, int themeId, int layoutId) {
        ContextThemeWrapper context = new ContextThemeWrapper(baseContext, themeId);
        FrameLayout contentHolder = new FrameLayout(context);
        mMainView = (ViewGroup) LayoutInflater.from(context).inflate(layoutId, contentHolder);
    }

    /**
     * Transfers all the children in the view hierarchy to the giving ViewGroup as child.
     * @param contentView The parent ViewGroup to use for the transfer.
     */
    public void transferViewHierarchyTo(ViewGroup contentView) {
        ViewGroup viewHierarchy = takeMainView();
        if (viewHierarchy == null) return;
        while (viewHierarchy.getChildCount() > 0) {
            View currentChild = viewHierarchy.getChildAt(0);
            viewHierarchy.removeView(currentChild);
            contentView.addView(currentChild);
        }
    }

    /**
     * Destroys the native WebContents instance the WarmupManager currently holds onto.
     */
    public void clearWebContentsIfNecessary() {
        mPrerendered = false;
        if (mPrerenderedWebContents == null) return;

        mPrerenderedWebContents.destroy();
        mPrerenderedWebContents = null;
    }

    /**
     * Cancel the current prerender.
     */
    public void cancelCurrentPrerender() {
        clearWebContentsIfNecessary();
        if (mExternalPrerenderHandler == null) return;

        mExternalPrerenderHandler.cancelCurrentPrerender();
    }

    /**
     * @return Whether the view hierarchy has been prebuilt.
     */
    public boolean hasBuiltViewHierarchy() {
        return mMainView != null;
    }

    /**
     * @return The prebuilt view hierarchy and clears the reference WarmupManager owns.
     */
    private ViewGroup takeMainView() {
        ViewGroup mainView = mMainView;
        mMainView = null;
        return mainView;
    }

    /**
     * Launches a background DNS query for a given URL.
     *
     * @param urlString URL from which the domain to query is extracted.
     */
    private static void prefetchDnsForUrlInBackground(String urlString) {
        new AsyncTask<String, Void, Void>() {
            @Override
            protected Void doInBackground(String... params) {
                try {
                    URL url = new URL(params[0]);
                    InetAddress.getByName(url.getHost());
                } catch (MalformedURLException e) {
                    // We don't do anything with the result of the resolution,
                    // it is only here to warm the DNS cache. So ignoring all
                    // exceptions is fine.
                } catch (UnknownHostException e) {
                    // Idem
                }
                return null;
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR, urlString);
    }

    /** Launches a background DNS query for a given URL if the data reduction proxy is not in use.
     *
     * @param context The Application context.
     * @param url URL from which the domain to query is extracted.
     */
    public static void maybePrefetchDnsForUrlInBackground(Context context, String url) {
        if (!DataReductionProxySettings.isEnabledBeforeNativeLoad(context)) {
            prefetchDnsForUrlInBackground(url);
        }
    }
}
