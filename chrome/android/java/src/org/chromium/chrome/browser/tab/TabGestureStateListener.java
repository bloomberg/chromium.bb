// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.base.UserData;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.WebContents;

/**
 * {@link GestureStateListener} implementation for a {@link Tab}. Associated with an active
 * {@link WebContents} via its {@link GestureListenerManager}. The listener is managed as
 * UserData and added to (or removed from) the active WebContents.
 */
public final class TabGestureStateListener
        extends EmptyTabObserver implements GestureStateListener, UserData {
    private static final Class<TabGestureStateListener> USER_DATA_KEY =
            TabGestureStateListener.class;

    private final Tab mTab;
    private WebContents mWebContents;

    /**
     * Initializes (creates if not present) TabGestureStateListener by adding it to
     * {@link GestureListenerManager} of the active {@link WebContents}.
     * @param tab Tab instance that the active WebContents instance gets loaded in.
     */
    public static void init(Tab tab) {
        TabGestureStateListener listener = tab.getUserDataHost().getUserData(USER_DATA_KEY);
        if (listener == null) {
            listener = tab.getUserDataHost().setUserData(
                    USER_DATA_KEY, new TabGestureStateListener(tab));
        }
        listener.init();
    }

    private TabGestureStateListener(Tab tab) {
        mTab = tab;
    }

    private void init() {
        WebContents newWebContents = mTab.getWebContents();
        if (newWebContents == mWebContents) return;
        if (mWebContents != null) removeFromManager();
        if (newWebContents != null) {
            GestureListenerManager.fromWebContents(newWebContents).addListener(this);
        }
        mWebContents = newWebContents;
    }

    // TabObserver

    @Override
    public void onWebContentsSwapped(Tab tab, boolean didStartLoad, boolean didFinishLoad) {
        init();
    }

    @Override
    public void onDestroyed(Tab tab) {
        removeFromManager();
    }

    private void removeFromManager() {
        GestureListenerManager manager = GestureListenerManager.fromWebContents(mWebContents);
        if (manager != null) manager.removeListener(this);
    }

    // GestureStateListener

    @Override
    public void onFlingStartGesture(int scrollOffsetY, int scrollExtentY) {
        onScrollingStateChanged();
    }

    @Override
    public void onFlingEndGesture(int scrollOffsetY, int scrollExtentY) {
        onScrollingStateChanged();
    }

    @Override
    public void onScrollStarted(int scrollOffsetY, int scrollExtentY) {
        onScrollingStateChanged();
    }

    @Override
    public void onScrollEnded(int scrollOffsetY, int scrollExtentY) {
        onScrollingStateChanged();
    }

    private void onScrollingStateChanged() {
        FullscreenManager fullscreenManager = mTab.getFullscreenManager();
        if (fullscreenManager == null) return;
        fullscreenManager.onContentViewScrollingStateChanged(isScrollInProgress());
    }

    private boolean isScrollInProgress() {
        GestureListenerManager manager = GestureListenerManager.fromWebContents(mWebContents);
        return manager != null ? manager.isScrollInProgress() : false;
    }
}
