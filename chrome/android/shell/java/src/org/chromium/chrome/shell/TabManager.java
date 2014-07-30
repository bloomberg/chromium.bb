// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.shell;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import org.chromium.chrome.browser.EmptyTabObserver;
import org.chromium.chrome.browser.Tab;
import org.chromium.content.browser.ContentVideoViewClient;
import org.chromium.content.browser.ContentViewClient;
import org.chromium.content.browser.ContentViewRenderView;
import org.chromium.ui.base.WindowAndroid;

/**
 * The TabManager hooks together all of the related {@link View}s that are used to represent
 * a {@link ChromeShellTab}.  It properly builds a {@link ChromeShellTab} and makes sure that the
 * {@link ChromeShellToolbar} and {@link ContentViewRenderView} show the proper content.
 */
public class TabManager extends LinearLayout {
    private static final String DEFAULT_URL = "http://www.google.com";

    private WindowAndroid mWindow;
    private ContentVideoViewClient mContentVideoViewClient;
    private ViewGroup mContentViewHolder;
    private ContentViewRenderView mContentViewRenderView;
    private ChromeShellToolbar mToolbar;

    private ChromeShellTab mCurrentTab;

    private String mStartupUrl = DEFAULT_URL;
    private View mCurrentView;

    /**
     * @param context The Context the view is running in.
     * @param attrs   The attributes of the XML tag that is inflating the view.
     */
    public TabManager(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
    }

    /**
     * Initialize the components required for Tab creation.
     * @param window The window used to generate all ContentViews.
     * @param videoViewClient The client to handle interactions from ContentVideoViews.
     */
    public void initialize(WindowAndroid window, ContentVideoViewClient videoViewClient) {
        assert window != null;
        mWindow = window;
        assert videoViewClient != null;
        mContentVideoViewClient = videoViewClient;
        mContentViewHolder = (ViewGroup) findViewById(R.id.content_container);
        mToolbar = (ChromeShellToolbar) findViewById(R.id.toolbar);
        mContentViewRenderView = new ContentViewRenderView(getContext()) {
            @Override
            protected void onReadyToRender() {
                if (mCurrentTab == null) createTab(mStartupUrl);
            }
        };
        mContentViewRenderView.onNativeLibraryLoaded(mWindow);
        mContentViewHolder.addView(mContentViewRenderView,
                new FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.MATCH_PARENT));
    }

    /**
     * @param startupUrl The URL that the first tab should navigate to.
     */
    public void setStartupUrl(String startupUrl) {
        mStartupUrl = startupUrl;
    }

    /**
     * Enter or leave overlay video mode.
     * @param enabled Whether overlay mode is enabled.
     */
    public void setOverlayVideoMode(boolean enabled) {
        if (mContentViewRenderView == null) return;
        mContentViewRenderView.setOverlayVideoMode(enabled);
    }

    /**
     * @return The currently visible {@link ChromeShellTab}.
     */
    public ChromeShellTab getCurrentTab() {
        return mCurrentTab;
    }

    /**
     * Creates a {@link ChromeShellTab} with a URL specified by {@code url}.
     * @param url The URL the new {@link ChromeShellTab} should start with.
     */
    public void createTab(String url) {
        if (!isContentViewRenderViewInitialized()) return;

        ContentViewClient client = new ContentViewClient() {
            @Override
            public ContentVideoViewClient getContentVideoViewClient() {
                return mContentVideoViewClient;
            }
        };
        ChromeShellTab tab = new ChromeShellTab(getContext(), url, mWindow, client);
        setCurrentTab(tab);
    }

    private boolean isContentViewRenderViewInitialized() {
        return mContentViewRenderView != null && mContentViewRenderView.isInitialized();
    }

    private void setCurrentTab(ChromeShellTab tab) {
        if (mCurrentTab != null) {
            mContentViewHolder.removeView(mCurrentTab.getView());
            mCurrentTab.destroy();
        }

        mCurrentTab = tab;
        mCurrentView = tab.getView();

        mCurrentTab.addObserver(new EmptyTabObserver() {
            @Override
            public void onContentChanged(Tab tab) {
                mContentViewHolder.removeView(mCurrentView);
                mCurrentView = tab.getView();
                setupContent(tab);
            }
        });

        mToolbar.showTab(mCurrentTab);
        setupContent(mCurrentTab);
    }

    private void setupContent(Tab tab) {
        mContentViewHolder.addView(tab.getView());
        mContentViewRenderView.setCurrentContentViewCore(tab.getContentViewCore());
        tab.getView().requestFocus();
        tab.getContentViewCore().onShow();
    }
}
