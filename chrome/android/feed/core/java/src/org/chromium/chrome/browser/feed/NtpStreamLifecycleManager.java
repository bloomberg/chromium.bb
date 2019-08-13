// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.app.Activity;
import android.support.annotation.Nullable;

import com.google.android.libraries.feed.api.client.stream.Stream;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;

/**
 * Manages the lifecycle of a {@link Stream} associated with a Tab in an Activity.
 */
final class NtpStreamLifecycleManager extends StreamLifecycleManager {
    /** Key for the Stream instance state that may be stored in a navigation entry. */
    private static final String STREAM_SAVED_INSTANCE_STATE_KEY = "StreamSavedInstanceState";

    /** The {@link Tab} that {@link #mStream} is attached to. */
    private final Tab mTab;

    /**
     * The {@link TabObserver} that observes tab state changes and notifies the {@link Stream}
     * accordingly.
     */
    private final TabObserver mTabObserver;

    /**
     * @param stream The {@link Stream} that this class manages.
     * @param activity The {@link Activity} that the {@link Stream} is attached to.
     * @param tab The {@link Tab} that the {@link Stream} is attached to.
     */
    NtpStreamLifecycleManager(Stream stream, Activity activity, Tab tab) {
        super(stream, activity);

        // Set mTab before 'start' since 'restoreInstanceState' will use it.
        mTab = tab;
        start();

        // We don't need to handle mStream#onDestroy here since this class will be destroyed when
        // the associated FeedNewTabPage is destroyed.
        mTabObserver = new EmptyTabObserver() {
            @Override
            public void onInteractabilityChanged(boolean isInteractable) {
                if (isInteractable) {
                    activate();
                } else {
                    deactivate();
                }
            }

            @Override
            public void onShown(Tab tab, @TabSelectionType int type) {
                show();
            }

            @Override
            public void onHidden(Tab tab, @TabHidingType int type) {
                hide();
            }

            @Override
            public void onPageLoadStarted(Tab tab, String url) {
                saveInstanceState();
            }
        };
        mTab.addObserver(mTabObserver);
    }

    /** @return Whether the {@link Stream} can be shown. */
    @Override
    protected boolean canShow() {
        return super.canShow() && !mTab.isHidden();
    }

    /** @return Whether the {@link Stream} can be activated. */
    @Override
    protected boolean canActivate() {
        return super.canActivate() && mTab.isUserInteractable();
    }

    /**
     * Clears any dependencies and calls {@link Stream#onDestroy()} when this class is not needed
     * anymore.
     */
    @Override
    protected void destroy() {
        if (mStreamState == StreamState.DESTROYED) return;

        super.destroy();
        mTab.removeObserver(mTabObserver);
    }

    /** Save the Stream instance state to the navigation entry if necessary. */
    @Override
    protected void saveInstanceState() {
        if (mTab.getWebContents() == null) return;

        NavigationController controller = mTab.getWebContents().getNavigationController();
        int index = controller.getLastCommittedEntryIndex();
        NavigationEntry entry = controller.getEntryAtIndex(index);
        if (entry == null) return;

        // At least under test conditions this method may be called initially for the load of the
        // NTP itself, at which point the last committed entry is not for the NTP yet. This method
        // will then be called a second time when the user navigates away, at which point the last
        // committed entry is for the NTP. The extra data must only be set in the latter case.
        if (!NewTabPage.isNTPUrl(entry.getUrl())) return;

        controller.setEntryExtraData(
                index, STREAM_SAVED_INSTANCE_STATE_KEY, mStream.getSavedInstanceStateString());
    }

    /**
     * @return The Stream instance state saved in navigation entry, or null if it is not previously
     *         saved.
     */
    @Override
    @Nullable
    protected String restoreInstanceState() {
        if (mTab.getWebContents() == null) return null;

        NavigationController controller = mTab.getWebContents().getNavigationController();
        int index = controller.getLastCommittedEntryIndex();
        return controller.getEntryExtraData(index, STREAM_SAVED_INSTANCE_STATE_KEY);
    }

    @VisibleForTesting
    TabObserver getTabObserverForTesting() {
        return mTabObserver;
    }
}
