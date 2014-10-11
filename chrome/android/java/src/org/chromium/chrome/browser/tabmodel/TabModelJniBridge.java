// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.base.CalledByNative;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Bridges between the C++ and Java {@link TabModel} interfaces.
 */
public abstract class TabModelJniBridge implements TabModel {
    private final boolean mIsIncognito;

    /** Native TabModelJniBridge pointer, which will be set by {@link #setNativePtr(long)}. */
    private long mNativeTabModelJniBridge;

    public TabModelJniBridge(boolean isIncognito) {
        mIsIncognito = isIncognito;
    }

    @CalledByNative
    private void clearNativePtr() {
        assert mNativeTabModelJniBridge != 0;
        mNativeTabModelJniBridge = 0;
    }

    @CalledByNative
    private void setNativePtr(long nativePointer) {
        assert mNativeTabModelJniBridge == 0;
        mNativeTabModelJniBridge = nativePointer;
    }

    @Override
    public void destroy() {
        if (mNativeTabModelJniBridge != 0) {
            // This will invalidate all other native references to this object in child classes.
            nativeDestroy(mNativeTabModelJniBridge);
            mNativeTabModelJniBridge = 0;
        }
    }

    @Override
    public boolean isIncognito() {
        return mIsIncognito;
    }

    @Override
    public Profile getProfile() {
        return nativeGetProfileAndroid(mNativeTabModelJniBridge);
    }

    /** Broadcast a native-side notification that all tabs are now loaded from storage. */
    public void broadcastSessionRestoreComplete() {
        assert mNativeTabModelJniBridge != 0;
        nativeBroadcastSessionRestoreComplete(mNativeTabModelJniBridge);
    }

    /**
     * Called by subclasses when a Tab is added to the TabModel.
     * @param tab Tab being added to the model.
     */
    protected void tabAddedToModel(Tab tab) {
        if (mNativeTabModelJniBridge != 0) nativeTabAddedToModel(mNativeTabModelJniBridge, tab);
    }

    /**
     * Sets the TabModel's index.
     * @param index Index of the Tab to select.
     */
    @CalledByNative
    private void setIndex(int index) {
        TabModelUtils.setIndex(this, index);
    }

    @Override
    @CalledByNative
    public abstract Tab getTabAt(int index);

    /**
     * Closes the Tab at a particular index.
     * @param index Index of the tab to close.
     * @return Whether the was successfully closed.
     */
    @CalledByNative
    protected abstract boolean closeTabAt(int index);

    /**
     * Creates a Tab with the given WebContents.
     * @param incognito Whether or not the tab is incognito.
     * @param nativeWebContents Pointer to the native WebContents.
     * @param parentId ID of the parent.
     */
    @CalledByNative
    protected abstract Tab createTabWithNativeContents(
            boolean incognito, long nativeWebContents, int parentId);

    /**
     * Creates a Tab with the given WebContents for DevTools.
     * @param url URL to show.
     */
    @CalledByNative
    protected abstract Tab createNewTabForDevTools(String url);

    @Override
    @CalledByNative
    public abstract int getCount();

    @Override
    @CalledByNative
    public abstract int index();

    /** @return Whether or not a sync session is currently being restored. */
    @CalledByNative
    protected abstract boolean isSessionRestoreInProgress();

    private native Profile nativeGetProfileAndroid(long nativeTabModelJniBridge);
    private native void nativeBroadcastSessionRestoreComplete(long nativeTabModelJniBridge);
    private native void nativeDestroy(long nativeTabModelJniBridge);
    private native void nativeTabAddedToModel(long nativeTabModelJniBridge, Tab tab);
}