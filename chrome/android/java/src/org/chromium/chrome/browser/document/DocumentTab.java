// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.document;

import android.graphics.Bitmap;

import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.EmptyTabObserver;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.TabObserver;
import org.chromium.chrome.browser.TabState;
import org.chromium.chrome.browser.TabUma.TabCreationState;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.tab.ChromeTab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.ActivityDelegate;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * A Tab child class with Chrome documents specific functionality.
 */
public class DocumentTab extends ChromeTab {
    private static final int DESIRED_ICON_SIZE_DP = 32;

    /**
     * Observer class with extra calls specific to Chrome Documents
     */
    public static class DocumentTabObserver extends EmptyTabObserver {
        /**
         * Called when a Favicon is received for the current document.
         * @param image The favicon image that was received.
         */
        protected void onFaviconReceived(Bitmap image) { }

        /**
         * Called when a tab is set to be covered or uncovered by child activity.
         */
        protected void onSetCoveredByChildActivity() { }
    }

    private int mDesiredIconSizePx;
    private boolean mDidRestoreState;

    // Whether this document tab was constructed from passed-in web contents pointer.
    private boolean mCreatedFromWebContents;

    private final DocumentActivity mActivity;

    // Whether this tab is covered by its child activity.
    private boolean mIsCoveredByChildActivity;

    /**
     * Standard constructor for the document tab.
     * @param activity The document activity that will hold on to this tab.
     * @param incognito Whether the tab is incognito.
     * @param windowAndroid The window that this tab should be using.
     * @param url The url to load on creation.
     * @param parentTabId The id of the parent tab.
     */
    private DocumentTab(DocumentActivity activity, boolean incognito,
            WindowAndroid windowAndroid, String url, int parentTabId) {
        super(ActivityDelegate.getTabIdFromIntent(activity.getIntent()), activity,
                incognito, windowAndroid, TabLaunchType.FROM_EXTERNAL_APP, parentTabId, null, null);
        mActivity = activity;
        initialize(url, null, activity.getTabContentManager(), false);
    }

    /**
     * Constructor for document tab from a frozen state.
     * @param activity The document activity that will hold on to this tab.
     * @param incognito Whether the tab is incognito.
     * @param windowAndroid The window that this tab should be using.
     * @param url The url to load on creation.
     * @param tabState The {@link TabState} the tab will be recreated from.
     * @param parentTabId The id of the parent tab.
     */
    private DocumentTab(DocumentActivity activity, boolean incognito,
            WindowAndroid windowAndroid, String url, TabState tabState, int parentTabId) {
        super(ActivityDelegate.getTabIdFromIntent(activity.getIntent()), activity,
                incognito, windowAndroid, TabLaunchType.FROM_RESTORE, parentTabId,
                TabCreationState.FROZEN_ON_RESTORE, tabState);
        mActivity = activity;
        initialize(url, null, activity.getTabContentManager(), true);
    }

    /**
     * Constructor for tab opened via JS.
     * @param activity The document activity that will hold on to this tab.
     * @param incognito Whether the tab is incognito.
     * @param windowAndroid The window that this tab should be using.
     * @param url The url to load on creation.
     * @param parentTabId The id of the parent tab.
     * @param webContents An optional {@link WebContents} object to use.
     */
    private DocumentTab(DocumentActivity activity, boolean incognito,
            WindowAndroid windowAndroid, String url, int parentTabId, WebContents webContents) {
        super(ActivityDelegate.getTabIdFromIntent(activity.getIntent()), activity,
                incognito, windowAndroid, TabLaunchType.FROM_LONGPRESS_FOREGROUND,
                parentTabId, null, null);
        mActivity = activity;
        initialize(url, webContents, activity.getTabContentManager(), false);
        mCreatedFromWebContents = true;
    }

    @Override
    protected void initContentViewCore(WebContents webContents) {
        super.initContentViewCore(webContents);
        getContentViewCore().setFullscreenRequiredForOrientationLock(false);
    }

    /**
     * Initializes the tab with native web contents.
     * @param url The url to use for looking up potentially pre-rendered web contents.
     * @param webContents Optionally, a pre-created web contents.
     * @param unfreeze Whether we want to initialize the tab from tab state.
     */
    private void initialize(String url, WebContents webContents,
            TabContentManager tabContentManager, boolean unfreeze) {
        mDesiredIconSizePx = (int) (DESIRED_ICON_SIZE_DP
                * mActivity.getResources().getDisplayMetrics().density);

        if (!unfreeze && webContents == null) {
            webContents = WarmupManager.getInstance().hasPrerenderedUrl(url)
                    ? WarmupManager.getInstance().takePrerenderedWebContents()
                    : WebContentsFactory.createWebContents(isIncognito(), false);
        }
        initialize(webContents, tabContentManager, false);
        if (unfreeze) mDidRestoreState = unfreezeContents();

        getView().requestFocus();
    }

    @Override
    public void onFaviconAvailable(Bitmap image) {
        super.onFaviconAvailable(image);
        if (image == null) return;
        if (image.getWidth() < mDesiredIconSizePx || image.getHeight() < mDesiredIconSizePx) {
            return;
        }
        RewindableIterator<TabObserver> observers = getTabObservers();
        while (observers.hasNext()) {
            TabObserver observer = observers.next();
            if (observer instanceof DocumentTabObserver) {
                ((DocumentTabObserver) observer).onFaviconReceived(image);
            }
        }
    }

    /**
     * A web contents delegate for handling opening new windows in Document mode.
     */
    public class DocumentTabChromeWebContentsDelegateAndroidImpl
            extends TabChromeWebContentsDelegateAndroidImpl {
        /**
         * TODO(dfalcantara): Remove this when DocumentActivity.getTabModelSelector()
         *                    can return a TabModelSelector that activateContents() can use.
         */
        @Override
        protected TabModel getTabModel() {
            return ChromeApplication.getDocumentTabModelSelector().getModel(isIncognito());
        }
    }

    @Override
    protected TabChromeWebContentsDelegateAndroid createWebContentsDelegate() {
        return new DocumentTabChromeWebContentsDelegateAndroidImpl();
    }

    /**
     * @return Whether or not the tab's state was restored.
     */
    public boolean didRestoreState() {
        return mDidRestoreState;
    }

    /**
     * @return Whether this tab was created using web contents passed to it.
     */
    public boolean isCreatedWithWebContents() {
        return mCreatedFromWebContents;
    }

    /**
     * Create a DocumentTab.
     * @param activity The activity the tab will be residing in.
     * @param incognito Whether the tab is incognito.
     * @param window The window the activity is using.
     * @param url The url that should be displayed by the tab.
     * @param webContents A {@link WebContents} object.
     * @param tabState State that was previously persisted to disk for the Tab.
     * @return The created {@link DocumentTab}.
     */
    static DocumentTab create(DocumentActivity activity, boolean incognito, WindowAndroid window,
            String url, WebContents webContents, boolean webContentsPaused, TabState tabState) {
        int parentTabId = activity.getIntent().getIntExtra(
                IntentHandler.EXTRA_PARENT_TAB_ID, Tab.INVALID_TAB_ID);
        if (webContents != null) {
            DocumentTab tab = new DocumentTab(
                    activity, incognito, window, url, parentTabId, webContents);
            if (webContentsPaused) webContents.resumeLoadingCreatedWebContents();
            return tab;
        }

        if (tabState == null) {
            return new DocumentTab(activity, incognito, window, url, parentTabId);
        } else {
            return new DocumentTab(activity, incognito, window, "", tabState, parentTabId);
        }
    }

    @Override
    public boolean isCoveredByChildActivity() {
        return mIsCoveredByChildActivity;
    }

    @Override
    @VisibleForTesting
    public void setCoveredByChildActivity(boolean isCoveredByChildActivity) {
        mIsCoveredByChildActivity = isCoveredByChildActivity;
        RewindableIterator<TabObserver> observers = getTabObservers();
        while (observers.hasNext()) {
            TabObserver observer = observers.next();
            if (observer instanceof DocumentTabObserver) {
                ((DocumentTabObserver) observer).onSetCoveredByChildActivity();
            }
        }
    }
}
