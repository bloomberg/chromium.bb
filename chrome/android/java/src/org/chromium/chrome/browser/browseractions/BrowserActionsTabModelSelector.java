// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browseractions;

import android.os.AsyncTask;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.browseractions.BrowserActionsTabCreatorManager.BrowserActionsTabCreator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModel;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelDelegate;
import org.chromium.chrome.browser.tabmodel.TabModelImpl;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelOrderController;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabPersistentStoreObserver;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.ArrayList;
import java.util.List;

/**
 * The tab model selector creates Tabs for Browser Actions. Tabs created by it are not shown in the
 * foreground and don't need {@link TabContentMananger}.
 */
public class BrowserActionsTabModelSelector
        extends TabModelSelectorBase implements TabModelDelegate {
    /** The singleton reference. */
    private static BrowserActionsTabModelSelector sInstance;
    private static final Object sInstanceLock = new Object();

    private final TabCreatorManager mTabCreatorManager;

    private final TabPersistentStore mTabSaver;

    private final TabModelOrderController mOrderController;

    private final List<LoadUrlParams> mPendingUrls = new ArrayList<>();

    private Runnable mTabCreationRunnable;

    /**
     * Builds a {@link BrowserActionsTabModelSelector} instance.
     */
    private BrowserActionsTabModelSelector() {
        super();
        mTabCreatorManager = new BrowserActionsTabCreatorManager();
        final TabPersistentStoreObserver persistentStoreObserver =
                new TabPersistentStoreObserver() {
                    @Override
                    public void onStateLoaded() {
                        markTabStateInitialized();
                    }
                };
        BrowserActionsTabPersistencePolicy persistencePolicy =
                new BrowserActionsTabPersistencePolicy();
        mTabSaver = new TabPersistentStore(
                persistencePolicy, this, mTabCreatorManager, persistentStoreObserver);
        mOrderController = new TabModelOrderController(this);
    }

    /**
     * @return The singleton instance of {@link BrowserActionsTabModelSelector}.
     */
    public static BrowserActionsTabModelSelector getInstance() {
        synchronized (sInstanceLock) {
            ThreadUtils.assertOnUiThread();
            if (sInstance == null) {
                sInstance = new BrowserActionsTabModelSelector();
                sInstance.initializeSelector();
            }
            return sInstance;
        }
    }

    /**
     * @return Whether {@link BrowserActionsTabModelSelector} has been initialized.
     */
    public static boolean isInitialized() {
        return sInstance != null;
    }

    /**
     * Initializes the selectors for the {@link BrowserActionsTabModelSelector}.
     */
    public void initializeSelector() {
        BrowserActionsTabCreator regularTabCreator =
                (BrowserActionsTabCreator) mTabCreatorManager.getTabCreator(false);
        TabModelImpl normalModel = new TabModelImpl(false, false, regularTabCreator, null, null,
                mOrderController, null, mTabSaver, this, false);
        TabModel incognitoModel = EmptyTabModel.getInstance();
        initialize(false, normalModel, incognitoModel);
        regularTabCreator.setTabModel(getCurrentModel());
    }

    @Override
    public void markTabStateInitialized() {
        super.markTabStateInitialized();
        // Add observer when tab model is restored to prevent restored tabs updating tab model.
        TabModelObserver tabModelObserver = new EmptyTabModelObserver() {
            @Override
            public void didAddTab(Tab tab, TabLaunchType type) {
                if (tab != null) {
                    mTabSaver.addTabToSaveQueue(tab);
                }
            }

            @Override
            public void tabRemoved(Tab tab) {
                // TODO(ltian): save the tab list when all the tabs are removed.
                mTabSaver.saveTabListAsynchronously();
            }
        };
        getModel(false).addObserver(tabModelObserver);
        if (mTabCreationRunnable != null) {
            ThreadUtils.runOnUiThread(mTabCreationRunnable);
        }
        mTabCreationRunnable = null;
    }

    /**
     * Creates a new Tab with given url in Browser Actions tab model.
     * @param loadUrlParams The url params to be opened.
     */
    public void openNewTab(LoadUrlParams loadUrlParams) {
        // If tab model is restored, directly create a new tab.
        if (isTabStateInitialized()) {
            createNewTab(loadUrlParams);
            return;
        }
        if (mTabCreationRunnable == null) {
            mTabCreationRunnable = new Runnable() {
                @Override
                public void run() {
                    for (int i = 0; i < mPendingUrls.size(); i++) {
                        createNewTab(mPendingUrls.get(i));
                    }
                    mPendingUrls.clear();
                }
            };
            new AsyncTask<Void, Void, Void>() {
                @Override
                protected Void doInBackground(Void... params) {
                    mTabSaver.loadState(true);
                    mTabSaver.restoreTabs(false);
                    return null;
                }
            }
                    .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        }
        mPendingUrls.add(loadUrlParams);
    }

    private void createNewTab(LoadUrlParams params) {
        openNewTab(params, TabLaunchType.FROM_BROWSER_ACTIONS, null, false);
    }

    @Override
    public void requestToShowTab(Tab tab, TabSelectionType type) {}

    @Override
    public boolean closeAllTabsRequest(boolean incognito) {
        return false;
    }

    @Override
    public boolean isInOverviewMode() {
        return false;
    }

    @Override
    public boolean isSessionRestoreInProgress() {
        return false;
    }

    /**
     * @return Return the current tab model. For Browser Actions, it always returns the normal tab
     * model.
     */
    public TabModelImpl getModel() {
        return (TabModelImpl) getModel(false);
    }

    /**
     * Save the whole tab list to the disk.
     */
    public void saveState() {
        commitAllTabClosures();
        mTabSaver.saveState();
    }

    @Override
    public Tab openNewTab(
            LoadUrlParams loadUrlParams, TabLaunchType type, Tab parent, boolean incognito) {
        if (!isTabStateInitialized()) {
            assert type == TabLaunchType.FROM_RESTORE;
        }
        return mTabCreatorManager.getTabCreator(incognito).createNewTab(
                loadUrlParams, type, parent);
    }
}
