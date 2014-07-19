// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.chrome.browser.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * This class allows Java code to read and modify preferences related to the NTP
 */
public class NewTabPagePrefs {
    private long mNativeNewTabPagePrefs;

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
        mNativeNewTabPagePrefs = 0;
    }

    /**
     * Sets whether the list of currently open tabs is collapsed (vs expanded) on the Recent Tabs
     * page.
     * @param isCollapsed Whether we want the currently open tabs list to be collapsed.
     */
    public void setCurrentlyOpenTabsCollapsed(boolean isCollapsed) {
        nativeSetCurrentlyOpenTabsCollapsed(mNativeNewTabPagePrefs, isCollapsed);
    }

    /**
     * Gets whether the list of currently open tabs is collapsed (vs expanded) on Recent Tabs page.
     * @return Whether the list of currently open tabs is collapsed (vs expanded) on
     *         the Recent Tabs page.
     */
    public boolean getCurrentlyOpenTabsCollapsed() {
        return nativeGetCurrentlyOpenTabsCollapsed(mNativeNewTabPagePrefs);
    }

    /**
     * Sets whether the list of snapshot documents is collapsed (vs expanded) on the Recent Tabs
     * page.
     * @param isCollapsed Whether we want the snapshot documents list to be collapsed.
     */
    public void setSnapshotDocumentCollapsed(boolean isCollapsed) {
        nativeSetSnapshotDocumentCollapsed(mNativeNewTabPagePrefs, isCollapsed);
    }

    /**
     * Gets whether the list of snapshot documents is collapsed (vs expanded) on
     * the Recent Tabs page.
     * @return Whether the list of snapshot documents is collapsed (vs expanded) on
     *         the Recent Tabs page.
     */
    public boolean getSnapshotDocumentCollapsed() {
        return nativeGetSnapshotDocumentCollapsed(mNativeNewTabPagePrefs);
    }

    /**
     * Sets whether the list of recently closed tabs is collapsed (vs expanded) on the Recent Tabs
     * page.
     * @param isCollapsed Whether we want the recently closed tabs list to be collapsed.
     */
    public void setRecentlyClosedTabsCollapsed(boolean isCollapsed) {
        nativeSetRecentlyClosedTabsCollapsed(mNativeNewTabPagePrefs, isCollapsed);
    }

    /**
     * Gets whether the list of recently closed tabs is collapsed (vs expanded) on
     * the Recent Tabs page.
     * @return Whether the list of recently closed tabs is collapsed (vs expanded) on
     *         the Recent Tabs page.
     */
    public boolean getRecentlyClosedTabsCollapsed() {
        return nativeGetRecentlyClosedTabsCollapsed(mNativeNewTabPagePrefs);
    }

    /**
     * Sets whether sync promo is collapsed (vs expanded) on the Recent Tabs page.
     * @param isCollapsed Whether we want the sync promo to be collapsed.
     */
    public void setSyncPromoCollapsed(boolean isCollapsed) {
        nativeSetSyncPromoCollapsed(mNativeNewTabPagePrefs, isCollapsed);
    }

    /**
     * Gets whether sync promo is collapsed (vs expanded) on the Recent Tabs page.
     * @return Whether the sync promo is collapsed (vs expanded) on the Recent Tabs page.
     */
    public boolean getSyncPromoCollapsed() {
        return nativeGetSyncPromoCollapsed(mNativeNewTabPagePrefs);
    }

    /**
     * Sets whether the given foreign session is collapsed (vs expanded) on the Recent Tabs page.
     * @param session Session to set collapsed or expanded.
     * @param isCollapsed Whether we want the foreign session to be collapsed.
     */
    public void setForeignSessionCollapsed(ForeignSession session, boolean isCollapsed) {
        nativeSetForeignSessionCollapsed(mNativeNewTabPagePrefs, session.tag, isCollapsed);
    }

    /**
     * Gets whether the given foreign session is collapsed (vs expanded) on the Recent Tabs page.
     * @param  session Session to fetch collapsed state.
     * @return Whether the given foreign session is collapsed (vs expanded) on the Recent Tabs page.
     */
    public boolean getForeignSessionCollapsed(ForeignSession session) {
        return nativeGetForeignSessionCollapsed(mNativeNewTabPagePrefs, session.tag);
    }

    private static native long nativeInit(Profile profile);
    private static native void nativeDestroy(long nativeNewTabPagePrefs);
    private static native void nativeSetCurrentlyOpenTabsCollapsed(
            long nativeNewTabPagePrefs, boolean isCollapsed);
    private static native boolean nativeGetCurrentlyOpenTabsCollapsed(
            long nativeNewTabPagePrefs);
    private static native void nativeSetSnapshotDocumentCollapsed(
            long nativeNewTabPagePrefs, boolean isCollapsed);
    private static native boolean nativeGetSnapshotDocumentCollapsed(
            long nativeNewTabPagePrefs);
    private static native void nativeSetRecentlyClosedTabsCollapsed(
            long nativeNewTabPagePrefs, boolean isCollapsed);
    private static native boolean nativeGetRecentlyClosedTabsCollapsed(
            long nativeNewTabPagePrefs);
    private static native void nativeSetSyncPromoCollapsed(long nativeNewTabPagePrefs,
            boolean isCollapsed);
    private static native boolean nativeGetSyncPromoCollapsed(long nativeNewTabPagePrefs);
    private static native void nativeSetForeignSessionCollapsed(
            long nativeNewTabPagePrefs, String sessionTag, boolean isCollapsed);
    private static native boolean nativeGetForeignSessionCollapsed(
            long nativeNewTabPagePrefs, String sessionTag);
}
