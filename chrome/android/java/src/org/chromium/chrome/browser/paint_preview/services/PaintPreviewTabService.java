// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview.services;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.components.paintpreview.browser.NativePaintPreviewServiceProvider;
import org.chromium.content_public.browser.WebContents;

/**
 * The Java-side implementations of paint_preview_tab_service.cc. The C++ side owns and controls
 * the lifecycle of the Java implementation.
 * This class provides the required functionalities for capturing the Paint Preview representation
 * of a tab.
 */
@JNINamespace("paint_preview")
public class PaintPreviewTabService implements NativePaintPreviewServiceProvider {
    private long mNativePaintPreviewTabService;
    private TabModelSelectorTabObserver mTabModelSelectorTabObserver;

    private class PaintPreviewTabServiceTabModelSelectorTabObserver
            extends TabModelSelectorTabObserver {
        private PaintPreviewTabService mTabService;

        private PaintPreviewTabServiceTabModelSelectorTabObserver(
                PaintPreviewTabService tabService, TabModelSelector tabModelSelector) {
            super(tabModelSelector);
            mTabService = tabService;
        }

        @Override
        public void onHidden(Tab tab, @TabHidingType int reason) {
            if (qualifiesForCapture(tab)) {
                mTabService.captureTab(tab, success -> {
                    if (!success) {
                        // Treat the tab as if it was closed to cleanup any partial capture data.
                        mTabService.tabClosed(tab);
                    }
                });
            }
        }

        @Override
        public void onTabUnregistered(Tab tab) {
            mTabService.tabClosed(tab);
        }

        private boolean qualifiesForCapture(Tab tab) {
            return !tab.isIncognito() && !tab.isNativePage() && !tab.isShowingErrorPage()
                    && tab.getWebContents() != null;
        }
    }

    @CalledByNative
    private PaintPreviewTabService(long nativePaintPreviewTabService) {
        mNativePaintPreviewTabService = nativePaintPreviewTabService;
    }

    @CalledByNative
    private void onNativeDestroyed() {
        mNativePaintPreviewTabService = 0;
    }

    @Override
    public long getNativeService() {
        return mNativePaintPreviewTabService;
    }

    /**
     * Returns whether there exists a capture for the tab ID.
     * @param tabId the id for the tab requested.
     * @return Will be true if there is a capture for the tab.
     */
    public boolean hasCaptureForTab(int tabId) {
        if (mNativePaintPreviewTabService == 0) return false;

        return PaintPreviewTabServiceJni.get().hasCaptureForTabAndroid(
                mNativePaintPreviewTabService, tabId);
    }

    /**
     * Should be called when all tabs are restored. Registers a {@link TabModelSelectorTabObserver}
     * for the regular to capture and delete paint previews as needed. Audits restored tabs to
     * remove any failed deletions.
     * @param tabModelSelector the TabModelSelector for the activity.
     * @param runAudit whether to delete tabs not in the tabModelSelector.
     */
    public void onRestoreCompleted(TabModelSelector tabModelSelector, boolean runAudit) {
        mTabModelSelectorTabObserver =
                new PaintPreviewTabServiceTabModelSelectorTabObserver(this, tabModelSelector);
        TabModel regularTabModel = tabModelSelector.getModel(/*incognito*/ false);

        int tabCount = regularTabModel.getCount();
        int[] tabIds = new int[tabCount];
        for (int i = 0; i < tabCount; i++) {
            Tab tab = regularTabModel.getTabAt(i);
            tabIds[i] = tab.getId();
        }

        if (runAudit) auditArtifacts(tabIds);
    }

    private void captureTab(Tab tab, Callback<Boolean> successCallback) {
        if (mNativePaintPreviewTabService == 0) {
            successCallback.onResult(false);
            return;
        }

        PaintPreviewTabServiceJni.get().captureTabAndroid(
                mNativePaintPreviewTabService, tab.getId(), tab.getWebContents(), successCallback);
    }

    private void tabClosed(Tab tab) {
        if (mNativePaintPreviewTabService == 0) return;

        PaintPreviewTabServiceJni.get().tabClosedAndroid(
                mNativePaintPreviewTabService, tab.getId());
    }

    private void auditArtifacts(int[] activeTabIds) {
        if (mNativePaintPreviewTabService == 0) return;

        PaintPreviewTabServiceJni.get().auditArtifactsAndroid(
                mNativePaintPreviewTabService, activeTabIds);
    }

    @NativeMethods
    interface Natives {
        void captureTabAndroid(long nativePaintPreviewTabService, int tabId,
                WebContents webContents, Callback<Boolean> successCallback);
        void tabClosedAndroid(long nativePaintPreviewTabService, int tabId);
        boolean hasCaptureForTabAndroid(long nativePaintPreviewTabService, int tabId);
        void auditArtifactsAndroid(long nativePaintPreviewTabService, int[] activeTabIds);
    }
}
