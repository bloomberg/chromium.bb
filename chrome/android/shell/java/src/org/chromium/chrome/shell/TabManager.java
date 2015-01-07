// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.shell;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.view.inputmethod.InputMethodManager;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.EmptyTabObserver;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.widget.accessibility.AccessibilityTabModelWrapper;
import org.chromium.content.browser.ContentVideoViewClient;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.ContentViewRenderView;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;

/**
 * The TabManager hooks together all of the related {@link View}s that are used to represent
 * a {@link ChromeShellTab}.  It properly builds a {@link ChromeShellTab} and makes sure that the
 * {@link ChromeShellToolbar} and {@link ContentViewRenderView} show the proper content.
 */
public class TabManager extends LinearLayout {
    private static final String DEFAULT_URL = "http://www.google.com";

    private ViewGroup mContentViewHolder;
    private ContentViewRenderView mContentViewRenderView;
    private ChromeShellToolbar mToolbar;

    private ChromeShellTab mCurrentTab;

    private String mStartupUrl = DEFAULT_URL;

    private ChromeShellTabModelSelector mTabModelSelector;
    private AccessibilityTabModelWrapper mTabModelWrapper;

    private final EmptyTabModelObserver mTabModelObserver = new EmptyTabModelObserver() {
        @Override
        public void didSelectTab(Tab tab, TabSelectionType type, int lastId) {
            assert tab instanceof ChromeShellTab;
            setCurrentTab((ChromeShellTab) tab);
            hideTabSwitcher();
        }

        @Override
        public void willCloseTab(Tab tab, boolean animate) {
            if (tab == mCurrentTab) setCurrentTab(null);
            if (mTabModelSelector.getCurrentModel().getCount() == 1) {
                createNewTab();
            }
        }
    };

    /**
     * @param context The Context the view is running in.
     * @param attrs   The attributes of the XML tag that is inflating the view.
     */
    public TabManager(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Initialize the components required for Tab creation.
     * @param window The window used to generate all ContentViews.
     * @param videoViewClient The client to handle interactions from ContentVideoViews.
     */
    public void initialize(WindowAndroid window, ContentVideoViewClient videoViewClient) {
        assert window != null;
        assert videoViewClient != null;

        mContentViewHolder = (ViewGroup) findViewById(R.id.content_container);

        mTabModelSelector = new ChromeShellTabModelSelector(
                window, videoViewClient, mContentViewHolder.getContext(), this);
        mTabModelSelector.getModel(false).addObserver(mTabModelObserver);

        mToolbar = (ChromeShellToolbar) findViewById(R.id.toolbar);
        mToolbar.setTabManager(this);
        mContentViewRenderView = new ContentViewRenderView(getContext()) {
            @Override
            protected void onReadyToRender() {
                if (mCurrentTab == null) createTab(mStartupUrl, TabLaunchType.FROM_RESTORE);
            }
        };
        mContentViewRenderView.onNativeLibraryLoaded(window);
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
     * Ensures that at least one tab exists, by opening a new one if necessary.
     */
    public void ensureTabExists() {
        if (mTabModelSelector.getCurrentModel().getCount() == 0) {
            createNewTab();
        }
    }

    /**
     * Opens a new blank tab.
     */
    public void createNewTab() {
        createTab("about:blank", TabLaunchType.FROM_MENU_OR_OVERVIEW);
    }

    /**
     * Closes all current tabs.
     */
    public void closeAllTabs() {
        mTabModelSelector.getCurrentModel().closeAllTabs();
    }

    @VisibleForTesting
    public void closeTab() {
        mTabModelSelector.getCurrentModel().closeTab(mCurrentTab);
    }

    /**
     * Creates a {@link ChromeShellTab} with a URL specified by {@code url}.
     * @param url The URL the new {@link ChromeShellTab} should start with.
     * @return The newly created tab, or null if the content view is uninitialized.
     */
    public Tab createTab(String url, TabLaunchType type) {
        if (!isContentViewRenderViewInitialized()) return null;

        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
        Tab tab = mTabModelSelector.openNewTab(loadUrlParams, type, null, false);
        tab.addObserver(new EmptyTabObserver() {
            @Override
            public void onToggleFullscreenMode(Tab tab, boolean enable) {
                mToolbar.setVisibility(enable ? GONE : VISIBLE);
            }
        });
        return tab;
    }

    void openNewTab(
            LoadUrlParams params, TabLaunchType launchType, Tab parentTab, boolean incognito) {
        mTabModelSelector.openNewTab(params, launchType, parentTab, incognito);
    }

    private boolean isContentViewRenderViewInitialized() {
        return mContentViewRenderView != null && mContentViewRenderView.isInitialized();
    }

    private void setCurrentTab(ChromeShellTab tab) {
        if (mCurrentTab != null) {
            mContentViewHolder.removeView(mCurrentTab.getView());
        }

        mCurrentTab = tab;

        mToolbar.showTab(mCurrentTab);

        if (mCurrentTab != null) setupContent();
    }

    private void setupContent() {
        View view = mCurrentTab.getView();
        ContentViewCore contentViewCore = mCurrentTab.getContentViewCore();
        mContentViewHolder.addView(view);
        mContentViewRenderView.setCurrentContentViewCore(contentViewCore);
        view.requestFocus();
        contentViewCore.onShow();
    }

    /**
     * Hide the tab switcher.
     */
    public void hideTabSwitcher() {
        if (mTabModelWrapper == null) return;
        if (!isTabSwitcherVisible()) return;
        ViewParent parent = mTabModelWrapper.getParent();
        if (parent != null) {
            assert parent == mContentViewHolder;
            mContentViewHolder.removeView(mTabModelWrapper);
        }
        mToolbar.showAddButton(false);
    }

    /**
     * Shows the tab switcher.
     */
    private void showTabSwitcher() {
        if (mTabModelWrapper == null) {
            mTabModelWrapper = (AccessibilityTabModelWrapper) LayoutInflater.from(
                    mContentViewHolder.getContext()).inflate(
                            R.layout.accessibility_tab_switcher, null);
            mTabModelWrapper.setup(null);
            mTabModelWrapper.setTabModelSelector(mTabModelSelector);
        }

        if (mTabModelWrapper.getParent() == null) {
            mContentViewHolder.addView(mTabModelWrapper);
        }
        mToolbar.showAddButton(true);
        InputMethodManager mImm = (InputMethodManager) getContext().getSystemService(
                Context.INPUT_METHOD_SERVICE);
        mImm.hideSoftInputFromWindow(mContentViewHolder.getWindowToken(), 0);
    }

    /**
     * Returns the visibility status of the tab switcher.
     */
    public boolean isTabSwitcherVisible() {
        return mTabModelWrapper != null && mTabModelWrapper.getParent() == mContentViewHolder;
    }

    /**
     * Toggles the tab switcher visibility.
     */
    public void toggleTabSwitcher() {
        if (!isTabSwitcherVisible()) {
            showTabSwitcher();
        } else {
            hideTabSwitcher();
        }
    }

    /**
     * Opens a URL in the current tab if one exists, or in a new tab otherwise.
     * @param url The URL to open.
     * @return The tab used to open the provided URL.
     */
    public Tab openUrl(String url) {
        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
        loadUrlParams.setTransitionType(PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR);
        Tab tab = mTabModelSelector.getCurrentTab();
        if (tab != null) {
            tab.loadUrl(loadUrlParams);
            return tab;
        }
        return createTab(url, TabLaunchType.FROM_KEYBOARD);
    }

    /**
     * Returns the TabModelSelector containing the tabs.
     */
    public TabModelSelector getTabModelSelector() {
        return mTabModelSelector;
    }
}
