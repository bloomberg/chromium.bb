// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.document;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.TabState;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabUma.TabCreationState;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.ActivityDelegate;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * A Tab child class with Chrome documents specific functionality.
 */
public class DocumentTab extends Tab {
    /**
     * Standard constructor for the document tab.
     * @param activity The document activity that will hold on to this tab.
     * @param incognito Whether the tab is incognito.
     * @param windowAndroid The window that this tab should be using.
     * @param url The url to load on creation.
     * @param parentTabId The id of the parent tab.
     * @param initiallyHidden Whether or not the {@link WebContents} should be initially hidden.
     */
    private DocumentTab(DocumentActivity activity, boolean incognito, WindowAndroid windowAndroid,
            String url, int parentTabId, boolean initiallyHidden, TabCreationState creationState) {
        super(ActivityDelegate.getTabIdFromIntent(activity.getIntent()), parentTabId, incognito,
                activity, windowAndroid, TabLaunchType.FROM_EXTERNAL_APP, creationState, null);
        initialize(url, null, activity.getTabContentManager(), false, initiallyHidden);
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
            WindowAndroid windowAndroid, String url, TabState tabState, int parentTabId,
            TabCreationState creationState) {
        super(ActivityDelegate.getTabIdFromIntent(activity.getIntent()), parentTabId, incognito,
                activity, windowAndroid, TabLaunchType.FROM_RESTORE,  creationState, tabState);
        initialize(url, null, activity.getTabContentManager(), true, false);
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
            WindowAndroid windowAndroid, String url, int parentTabId, WebContents webContents,
            TabCreationState creationState) {
        super(ActivityDelegate.getTabIdFromIntent(activity.getIntent()), parentTabId, incognito,
                activity, windowAndroid, TabLaunchType.FROM_LONGPRESS_FOREGROUND,
                creationState, null);
        initialize(url, webContents, activity.getTabContentManager(), false, false);
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
     * @param initiallyHidden Whether or not the {@link WebContents} should be initially hidden.
     */
    private void initialize(String url, WebContents webContents,
            TabContentManager tabContentManager, boolean unfreeze, boolean initiallyHidden) {
        if (!unfreeze && webContents == null) {
            webContents = WarmupManager.getInstance().hasPrerenderedUrl(url)
                    ? WarmupManager.getInstance().takePrerenderedWebContents()
                    : WebContentsFactory.createWebContents(isIncognito(), initiallyHidden);
        }
        initialize(webContents, tabContentManager, new TabDelegateFactory() {
            @Override
            public TabWebContentsDelegateAndroid createWebContentsDelegate(
                    Tab tab, ChromeActivity activity) {
                return new DocumentTabWebContentsDelegateAndroid(DocumentTab.this, mActivity);
            }
        }, initiallyHidden);
        if (unfreeze) unfreezeContents();

        getView().requestFocus();
    }

    /**
     * A web contents delegate for handling opening new windows in Document mode.
     */
    public class DocumentTabWebContentsDelegateAndroid extends TabWebContentsDelegateAndroid {
        public DocumentTabWebContentsDelegateAndroid(Tab tab, ChromeActivity activity) {
            super(tab, activity);
        }

        /**
         * TODO(dfalcantara): Remove this when DocumentActivity.getTabModelSelector()
         *                    can return a TabModelSelector that activateContents() can use.
         */
        @Override
        protected TabModel getTabModel() {
            return ChromeApplication.getDocumentTabModelSelector().getModel(isIncognito());
        }
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
     * @param initiallyHidden Whether or not the {@link WebContents} should be initially hidden.
     */
    static DocumentTab create(DocumentActivity activity, boolean incognito, WindowAndroid window,
            String url, WebContents webContents, TabState tabState, boolean initiallyHidden,
            TabCreationState creationState) {
        int parentTabId = activity.getIntent().getIntExtra(
                IntentHandler.EXTRA_PARENT_TAB_ID, Tab.INVALID_TAB_ID);
        if (webContents != null) {
            DocumentTab tab = new DocumentTab(
                    activity, incognito, window, url, parentTabId, webContents, creationState);
            webContents.resumeLoadingCreatedWebContents();
            return tab;
        }

        if (tabState == null) {
            return new DocumentTab(
                    activity, incognito, window, url, parentTabId, initiallyHidden, creationState);
        } else {
            return new DocumentTab(
                    activity, incognito, window, "", tabState, parentTabId, creationState);
        }
    }
}
