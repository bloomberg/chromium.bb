// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.testshell;

import android.util.AttributeSet;
import android.view.ViewGroup;
import android.content.Context;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import org.chromium.chrome.browser.TabBase;
import org.chromium.content.browser.ContentViewRenderView;
import org.chromium.ui.gfx.NativeWindow;

/**
 * The TabManager hooks together all of the related {@link View}s that are used to represent
 * a {@link TabBase}.  It properly builds a {@link TabBase} and makes sure that the {@link Toolbar}
 * and {@link ContentViewRenderView} show the proper content.
 */
public class TabManager extends LinearLayout {
    private static final String DEFAULT_URL = "http://www.google.com";

    private NativeWindow mWindow;
    private ViewGroup mContentViewHolder;
    private ContentViewRenderView mContentViewRenderView;
    private TestShellToolbar mToolbar;

    private TabBase mCurrentTab;

    private String mStartupUrl = DEFAULT_URL;

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

        mContentViewHolder = (ViewGroup) findViewById(R.id.content_container);
        mToolbar = (TestShellToolbar) findViewById(R.id.toolbar);
        mContentViewRenderView = new ContentViewRenderView(getContext()) {
            @Override
            protected void onReadyToRender() {
                if (mCurrentTab == null) createTab(mStartupUrl);
            }
        };
        mContentViewHolder.addView(mContentViewRenderView,
                new FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.MATCH_PARENT));
    }

    /**
     * @param window The window used to generate all ContentViews.
     */
    public void setWindow(NativeWindow window) {
        mWindow = window;
    }

    /**
     * @param startupUrl The URL that the first tab should navigate to.
     */
    public void setStartupUrl(String startupUrl) {
        mStartupUrl = startupUrl;
    }

    /**
     * @return The currently visible {@link TabBase}.
     */
    public TabBase getCurrentTab() {
        return mCurrentTab;
    }

    /**
     * Creates a {@link TabBase} with a URL specified by {@code url}.
     * @param url The URL the new {@link TabBase} should start with.
     */
    public void createTab(String url) {
        if (!isContentViewRenderViewInitialized()) return;

        TabBase tab = new TabBase(getContext(), url, mWindow);
        setCurrentTab(tab);
    }

    private boolean isContentViewRenderViewInitialized() {
        return mContentViewRenderView != null && mContentViewRenderView.isInitialized();
    }

    private void setCurrentTab(TabBase tab) {
        if (mCurrentTab != null) {
            mContentViewHolder.removeView(mCurrentTab.getContentView());
            mCurrentTab.destroy();
        }

        mCurrentTab = tab;

        mToolbar.showTab(mCurrentTab);
        mContentViewHolder.addView(mCurrentTab.getContentView());
        mContentViewRenderView.setCurrentContentView(mCurrentTab.getContentView());
        mCurrentTab.getContentView().requestFocus();
    }
}