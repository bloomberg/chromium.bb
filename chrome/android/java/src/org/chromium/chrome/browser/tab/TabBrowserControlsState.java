// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.UserData;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.BrowserControlsState;

/**
 * Manages the state of tab browser controls. Instantiation is done by
 * {@link TabDelegateFactory#createBrowserControlsState()}.
 */
public class TabBrowserControlsState implements UserData {
    private static final Class<TabBrowserControlsState> USER_DATA_KEY =
            TabBrowserControlsState.class;

    private final Tab mTab;
    private final BrowserControlsVisibilityDelegate mVisibilityDelegate;
    private final long mNativeTabBrowserControlsState;

    /** The current browser controls constraints. -1 if not set. */
    private @BrowserControlsState int mConstraints = -1;

    public static void create(Tab tab, BrowserControlsVisibilityDelegate delegate) {
        tab.getUserDataHost().setUserData(
                USER_DATA_KEY, new TabBrowserControlsState(tab, delegate));
    }

    public static TabBrowserControlsState get(Tab tab) {
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    /**
     * Returns the current visibility constraints for the display of browser controls.
     * {@link BrowserControlsState} defines the valid return options.
     * @param tab Tab whose browser constrol state is looked into.
     * @return The current visibility constraints.
     */
    @BrowserControlsState
    public static int getConstraints(Tab tab) {
        if (tab == null || get(tab) == null) return BrowserControlsState.BOTH;
        return get(tab).getConstraints();
    }

    /**
     * Updates the browser controls state for this tab.  As these values are set at the renderer
     * level, there is potential for this impacting other tabs that might share the same
     * process.
     *
     * @param tab Tab whose browser constrol state is looked into.
     * @param current The desired current state for the controls.  Pass
     *         {@link BrowserControlsState#BOTH} to preserve the current position.
     * @param animate Whether the controls should animate to the specified ending condition or
     *         should jump immediately.
     */
    public static void update(Tab tab, int current, boolean animate) {
        if (tab == null || get(tab) == null) return;
        get(tab).update(current, animate);
    }

    /** Constructor */
    private TabBrowserControlsState(Tab tab, BrowserControlsVisibilityDelegate delegate) {
        mTab = tab;
        mVisibilityDelegate = delegate;
        mNativeTabBrowserControlsState = nativeInit();
    }

    @Override
    public void destroy() {
        nativeOnDestroyed(mNativeTabBrowserControlsState);
    }

    private void update(int current, boolean animate) {
        int constraints = getConstraints();

        // Do nothing if current and constraints conflict to avoid error in renderer.
        if ((constraints == BrowserControlsState.HIDDEN && current == BrowserControlsState.SHOWN)
                || (constraints == BrowserControlsState.SHOWN
                        && current == BrowserControlsState.HIDDEN)) {
            return;
        }
        if (mNativeTabBrowserControlsState != 0) {
            nativeUpdateState(mNativeTabBrowserControlsState, mTab.getWebContents(), constraints,
                    current, animate);
        }
        if (constraints == mConstraints) return;

        mConstraints = constraints;
        RewindableIterator<TabObserver> observers = mTab.getTabObservers();
        while (observers.hasNext()) {
            observers.next().onBrowserControlsConstraintsUpdated(mTab, constraints);
        }
    }

    /**
     * @return Whether hiding browser controls is enabled or not.
     */
    private boolean canAutoHide() {
        return mVisibilityDelegate.canAutoHideBrowserControls();
    }

    /**
     * @return Whether showing browser controls is enabled or not.
     */
    public boolean canShow() {
        return mVisibilityDelegate.canShowBrowserControls();
    }

    @BrowserControlsState
    private int getConstraints() {
        int constraints = BrowserControlsState.BOTH;
        if (!canShow()) {
            constraints = BrowserControlsState.HIDDEN;
        } else if (!canAutoHide()) {
            constraints = BrowserControlsState.SHOWN;
        }
        return constraints;
    }

    private native long nativeInit();
    private native void nativeOnDestroyed(long nativeTabBrowserControlsState);
    private native void nativeUpdateState(long nativeTabBrowserControlsState,
            WebContents webContents, int contraints, int current, boolean animate);
}
