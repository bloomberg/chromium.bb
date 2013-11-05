// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ForeignSessionHelper.ForeignSession;

/**
 * This class allows Java code to read and modify preferences related to the NTP
 */
public class NewTabPagePrefs {
    private int mNativeNewTabPagePrefs;

    /**
     * Initialize this class with the given profile.
     * @param profile Profile that will be used for syncing.
     */
    public NewTabPagePrefs(Profile profile) {
        mNativeNewTabPagePrefs = nativeInit(profile);
    }

    /**
     * Clean up the C++ side of this class. After the call, this class instance shouldn't be used.
     */
    public void destroy() {
        assert mNativeNewTabPagePrefs != 0;
        nativeDestroy(mNativeNewTabPagePrefs);
    }

    /**
     * Set snapshot document list collapsed or uncollapsed state in preferences.
     * @param isCollapsed {@code True} Whether we want the snapshot documents list to be collapsed.
     */
    public void setSnapshotDocumentCollapsed(boolean isCollapsed) {
        nativeSetSnapshotDocumentCollapsed(mNativeNewTabPagePrefs, isCollapsed);
    }

    /**
     * Get the snapshot document list collapsed or uncollapsed state in preferences.
     * @return {@code True} Whether the snapshot documnets list is collapsed.
     */
    public boolean getSnapshotDocumentCollapsed() {
        return nativeGetSnapshotDocumentCollapsed(mNativeNewTabPagePrefs);
    }

    /**
     * Set recently closed tabs list collapsed or uncollapsed state in preferences.
     * @param isCollapsed {@code True} Whether we want the recently closed tabs list to be
     * collapsed.
     */
    public void setRecentlyClosedTabsCollapsed(boolean isCollapsed) {
        nativeSetRecentlyClosedTabsCollapsed(mNativeNewTabPagePrefs, isCollapsed);
    }

    /**
     * Get the recently closed document list collapsed or uncollapsed state in preferences.
     * @return {@code True} Whether the recently closed list is collapsed.
     */
    public boolean getRecentlyClosedTabsCollapsed() {
        return nativeGetRecentlyClosedTabsCollapsed(mNativeNewTabPagePrefs);
    }

    /**
     * Set sync promo collapsed or uncollapsed state in preferences.
     * @param isCollapsed {@code True} Whether we want the sync promo to be collapsed.
     */
    public void setSyncPromoCollapsed(boolean isCollapsed) {
        nativeSetSyncPromoCollapsed(mNativeNewTabPagePrefs, isCollapsed);
    }

    /**
     * Get the sync promo collapsed or uncollapsed state in preferences.
     * @return {@code True} Whether the snapshot documnets list is collapsed.
     */
    public boolean getSyncPromoCollapsed() {
        return nativeGetSyncPromoCollapsed(mNativeNewTabPagePrefs);
    }

    /**
     * Set the given session collapsed or uncollapsed in preferences.
     * @param session     Session to set collapsed or uncollapsed.
     * @param isCollapsed {@code True} iff we want the session to be collapsed.
     */
    public void setForeignSessionCollapsed(ForeignSession session, boolean isCollapsed) {
        nativeSetForeignSessionCollapsed(mNativeNewTabPagePrefs, session.tag, isCollapsed);
    }

    /**
     * Get the given session collapsed or uncollapsed state in preferences.
     * @param  session Session to fetch collapsed state.
     * @return         {@code True} if the session is collapsed, false if expanded.
     */
    public boolean getForeignSessionCollapsed(ForeignSession session) {
        return nativeGetForeignSessionCollapsed(mNativeNewTabPagePrefs, session.tag);
    }

    private static native int nativeInit(Profile profile);
    private static native void nativeDestroy(int nativeNewTabPagePrefs);
    private static native void nativeSetSnapshotDocumentCollapsed(
            int nativeNewTabPagePrefs, boolean isCollapsed);
    private static native boolean nativeGetSnapshotDocumentCollapsed(
            int nativeNewTabPagePrefs);
    private static native void nativeSetRecentlyClosedTabsCollapsed(
            int nativeNewTabPagePrefs, boolean isCollapsed);
    private static native boolean nativeGetRecentlyClosedTabsCollapsed(
            int nativeNewTabPagePrefs);
    private static native void nativeSetSyncPromoCollapsed(int nativeNewTabPagePrefs,
            boolean isCollapsed);
    private static native boolean nativeGetSyncPromoCollapsed(int nativeNewTabPagePrefs);
    private static native void nativeSetForeignSessionCollapsed(
            int nativeNewTabPagePrefs, String sessionTag, boolean isCollapsed);
    private static native boolean nativeGetForeignSessionCollapsed(
            int nativeNewTabPagePrefs, String sessionTag);
}
