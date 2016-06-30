// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.AsyncTask;
import android.os.StrictMode;
import android.os.SystemClock;
import android.support.annotation.Nullable;
import android.support.annotation.WorkerThread;
import android.support.v4.util.AtomicFile;
import android.text.TextUtils;
import android.util.Pair;
import android.util.SparseArray;
import android.util.SparseIntArray;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.StreamUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.TabState;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.customtabs.CustomTabDelegateFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabIdManager;
import org.chromium.content_public.browser.LoadUrlParams;

import java.io.BufferedInputStream;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Deque;
import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Executor;
import java.util.concurrent.TimeUnit;

/**
 * This class handles saving and loading tab state from the persistent storage.
 */
public class TabPersistentStore extends TabPersister {
    private static final String TAG = "tabmodel";

    /**
     * The current version of the saved state file.
     * Version 4: In addition to the tab's ID, save the tab's last URL.
     * Version 5: In addition to the total tab count, save the incognito tab count.
     */
    private static final int SAVED_STATE_VERSION = 5;

    private static final String BASE_STATE_FOLDER = "tabs";

    /** The name of the file where the state is saved. */
    @VisibleForTesting
    public static final String SAVED_STATE_FILE = "tab_state";

    /** The name of the directory where the state is saved. */
    @VisibleForTesting
    static final String SAVED_STATE_DIRECTORY = "0";

    @VisibleForTesting
    static final String PREF_ACTIVE_TAB_ID =
            "org.chromium.chrome.browser.tabmodel.TabPersistentStore.ACTIVE_TAB_ID";

    @VisibleForTesting
    static final String PREF_HAS_RUN_FILE_MIGRATION =
            "org.chromium.chrome.browser.tabmodel.TabPersistentStore.HAS_RUN_FILE_MIGRATION";

    @VisibleForTesting
    static final String PREF_HAS_RUN_MULTI_INSTANCE_FILE_MIGRATION =
            "org.chromium.chrome.browser.tabmodel.TabPersistentStore."
            + "HAS_RUN_MULTI_INSTANCE_FILE_MIGRATION";

    private static final String PREF_HAS_COMPUTED_MAX_ID =
            "org.chromium.chrome.browser.tabmodel.TabPersistentStore.HAS_COMPUTED_MAX_ID";

    /** Prevents two copies of the Migration task from being created. */
    private static final Object MIGRATION_LOCK = new Object();

    /** Prevents two TabPersistentStores from saving the same file simultaneously. */
    private static final Object SAVE_LIST_LOCK = new Object();

    /**
     * Prevents two clean up tasks from getting created simultaneously. Also protects against
     * incorrectly interleaving create/run/cancel on the task.
     */
    private static final Object CLEAN_UP_TASK_LOCK = new Object();

    /** Prevents two state directories from getting created simultaneously */
    private static final Object DIR_CREATION_LOCK = new Object();

    /**
     * Callback interface to use while reading the persisted TabModelSelector info from disk.
     */
    public static interface OnTabStateReadCallback {
        /**
         * To be called as the details about a persisted Tab are read from the TabModelSelector's
         * persisted data.
         * @param index                  The index out of all tabs for the current tab read.
         * @param id                     The id for the current tab read.
         * @param url                    The url for the current tab read.
         * @param isIncognito            Whether the Tab is definitely Incognito, or null if it
         *                               couldn't be determined because we didn't know how many
         *                               Incognito tabs were saved out.
         * @param isStandardActiveIndex  Whether the current tab read is the normal active tab.
         * @param isIncognitoActiveIndex Whether the current tab read is the incognito active tab.
         */
        void onDetailsRead(int index, int id, String url, Boolean isIncognito,
                boolean isStandardActiveIndex, boolean isIncognitoActiveIndex);
    }

    /**
     * Alerted at various stages of operation.
     */
    public static interface TabPersistentStoreObserver {
        /**
         * To be called when the file containing the initial information about the TabModels has
         * been loaded.
         * @param tabCountAtStartup How many tabs there are in the TabModels.
         */
        void onInitialized(int tabCountAtStartup);

        /**
         * Called when details about a Tab are read from the metadata file.
         */
        void onDetailsRead(int index, int id, String url,
                boolean isStandardActiveIndex, boolean isIncognitoActiveIndex);

        /**
         * To be called when the TabStates have all been loaded.
         * @param context Context used by the TabPersistentStore.
         */
        void onStateLoaded(Context context);

        /**
         * Called when the metadata file has been saved out asynchronously.
         * This currently does not get called when the metadata file is saved out on the UI thread.
         */
        void onMetadataSavedAsynchronously();
    }

    /** Stores information about a TabModel. */
    public static class TabModelMetadata {
        public final int index;
        public final List<Integer> ids;
        public final List<String> urls;

        TabModelMetadata(int selectedIndex) {
            index = selectedIndex;
            ids = new ArrayList<Integer>();
            urls = new ArrayList<String>();
        }
    }

    private static class BaseStateDirectoryHolder {
        // Not final for tests.
        private static File sDirectory = getOrCreateBaseStateDirectory();
    }

    private static AsyncTask<Void, Void, Void> sMigrationTask = null;
    private static AsyncTask<Void, Void, CleanUpTabStateDataInfo> sCleanupTask = null;
    private static File sStateDirectory;

    private final TabModelSelector mTabModelSelector;
    private final TabCreatorManager mTabCreatorManager;
    private final Context mContext;
    private final int mSelectorIndex;
    private final TabPersistentStoreObserver mObserver;

    private TabContentManager mTabContentManager;

    private final Deque<Tab> mTabsToSave;
    private final Deque<TabRestoreDetails> mTabsToRestore;

    private LoadTabTask mLoadTabTask;
    private SaveTabTask mSaveTabTask;
    private SaveListTask mSaveListTask;

    private boolean mDestroyed;
    private boolean mCancelNormalTabLoads = false;
    private boolean mCancelIncognitoTabLoads = false;

    // Keys are the original tab indexes, values are the tab ids.
    private SparseIntArray mNormalTabsRestored;
    private SparseIntArray mIncognitoTabsRestored;

    private SharedPreferences mPreferences;
    private AsyncTask<Void, Void, DataInputStream> mPrefetchTabListTask;
    private byte[] mLastSavedMetadata;

    @VisibleForTesting
    AsyncTask<Void, Void, TabState> mPrefetchActiveTabTask;

    /**
     * Creates an instance of a TabPersistentStore.
     * @param modelSelector The {@link TabModelSelector} to restore to and save from.
     * @param selectorIndex The index that represents which state file to pull and save state to.
     *                      This is used when there can be more than one TabModelSelector.
     * @param context       A Context instance.
     * @param tabCreatorManager The {@link TabCreatorManager} to use.
     * @param observer      Notified when the TabPersistentStore has completed tasks.
     */
    public TabPersistentStore(TabModelSelector modelSelector, int selectorIndex, Context context,
            TabCreatorManager tabCreatorManager, TabPersistentStoreObserver observer) {
        mTabModelSelector = modelSelector;
        mContext = context;
        mTabCreatorManager = tabCreatorManager;
        mTabsToSave = new ArrayDeque<Tab>();
        mTabsToRestore = new ArrayDeque<TabRestoreDetails>();
        mSelectorIndex = selectorIndex;
        mObserver = observer;
        mPreferences = ContextUtils.getAppSharedPreferences();
        startMigrationTaskIfNecessary();
        startPrefetchTabListTask();
        startPrefetchActiveTabTask();
    }

    @Override
    protected File getStateDirectory() {
        // TODO(twellington): remove this method as the super-class separation was only useful for
        // document mode.
        return getOrCreateStateDirectory();
    }

    /**
     * The folder where the state should be saved to.
     * @return A file representing the directory that contains TabModelSelector states.
     */
    public static File getOrCreateStateDirectory() {
        synchronized (DIR_CREATION_LOCK) {
            if (sStateDirectory == null) {
                sStateDirectory = new File(
                        BaseStateDirectoryHolder.sDirectory, SAVED_STATE_DIRECTORY);
                StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
                StrictMode.allowThreadDiskWrites();
                try {
                    if (!sStateDirectory.exists() && !sStateDirectory.mkdirs()) {
                        Log.e(TAG, "Failed to create state folder: " + sStateDirectory);
                    }
                } finally {
                    StrictMode.setThreadPolicy(oldPolicy);
                }
            }
        }
        return sStateDirectory;
    }

    /**
     * Waits for the task that migrates all state files to their new location to finish.
     */
    @VisibleForTesting
    public void waitForMigrationToFinish() {
        if (sMigrationTask == null) return;
        try {
            sMigrationTask.get();
        } catch (InterruptedException e) {
        } catch (ExecutionException e) {
        }
    }

    /**
     * Sets the {@link TabContentManager} to use.
     * @param cache The {@link TabContentManager} to use.
     */
    public void setTabContentManager(TabContentManager cache) {
        mTabContentManager = cache;
    }

    private void logExecutionTime(String name, long time) {
        if (LibraryLoader.isInitialized()) {
            RecordHistogram.recordTimesHistogram("Android.StrictMode.TabPersistentStore." + name,
                    SystemClock.uptimeMillis() - time, TimeUnit.MILLISECONDS);
        }
    }

    public void saveState() {
        // Temporarily allowing disk access. TODO: Fix. See http://b/5518024
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
        try {
            long saveStateStartTime = SystemClock.uptimeMillis();
            // The list of tabs should be saved first in case our activity is terminated early.
            // Explicitly toss out any existing SaveListTask because they only save the TabModel as
            // it looked when the SaveListTask was first created.
            if (mSaveListTask != null) mSaveListTask.cancel(true);
            try {
                saveListToFile(serializeTabMetadata());
            } catch (IOException e) {
                Log.w(TAG, "Error while saving tabs state; will attempt to continue...", e);
            }
            logExecutionTime("SaveListTime", saveStateStartTime);

            // Add current tabs to save because they did not get a save signal yet.
            Tab currentStandardTab = TabModelUtils.getCurrentTab(mTabModelSelector.getModel(false));
            if (currentStandardTab != null && !mTabsToSave.contains(currentStandardTab)
                    && currentStandardTab.isTabStateDirty()
                    // For content URI, the read permission granted to an activity is not
                    // persistent.
                    && !isTabUrlContentScheme(currentStandardTab)) {
                mTabsToSave.addLast(currentStandardTab);
            }
            Tab currentIncognitoTab = TabModelUtils.getCurrentTab(mTabModelSelector.getModel(true));
            if (currentIncognitoTab != null && !mTabsToSave.contains(currentIncognitoTab)
                    && currentIncognitoTab.isTabStateDirty()
                    && !isTabUrlContentScheme(currentIncognitoTab)) {
                mTabsToSave.addLast(currentIncognitoTab);
            }
            // Wait for the current tab to save.
            if (mSaveTabTask != null) {
                // Cancel calls get() to wait for this to finish internally if it has to.
                // The issue is it may assume it cancelled the task, but the task still actually
                // wrote the state to disk.  That's why we have to check mStateSaved here.
                if (mSaveTabTask.cancel(false) && !mSaveTabTask.mStateSaved) {
                    // The task was successfully cancelled.  We should try to save this state again.
                    Tab cancelledTab = mSaveTabTask.mTab;
                    if (!mTabsToSave.contains(cancelledTab)
                            && cancelledTab.isTabStateDirty()
                            && !isTabUrlContentScheme(cancelledTab)) {
                        mTabsToSave.addLast(cancelledTab);
                    }
                }

                mSaveTabTask = null;
            }

            long saveTabsStartTime = SystemClock.uptimeMillis();
            // Synchronously save any remaining unsaved tabs (hopefully very few).
            for (Tab tab : mTabsToSave) {
                int id = tab.getId();
                boolean incognito = tab.isIncognito();
                FileOutputStream stream = null;
                try {
                    TabState state = tab.getState();
                    if (state != null) {
                        stream = openTabStateOutputStream(id, incognito);
                        TabState.saveState(stream, state, incognito);
                    }
                } catch (IOException e) {
                    Log.w(TAG, "Error while saving tabs state; will attempt to continue...", e);
                } catch (OutOfMemoryError e) {
                    Log.w(TAG, "Out of memory error while attempting to save tab state.  Erasing.");
                    deleteTabState(id, incognito);
                } finally {
                    StreamUtil.closeQuietly(stream);
                }
            }
            mTabsToSave.clear();
            logExecutionTime("SaveTabsTime", saveTabsStartTime);
            logExecutionTime("SaveStateTime", saveStateStartTime);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    /**
     * Restore saved state. Must be called before any tabs are added to the list.
     */
    public void loadState() {
        long time = SystemClock.uptimeMillis();

        // If a cleanup task is in progress, cancel it before loading state.
        synchronized (CLEAN_UP_TASK_LOCK) {
            if (sCleanupTask != null) sCleanupTask.cancel(true);
        }

        waitForMigrationToFinish();
        logExecutionTime("LoadStateTime", time);

        mCancelNormalTabLoads = false;
        mCancelIncognitoTabLoads = false;
        mNormalTabsRestored = new SparseIntArray();
        mIncognitoTabsRestored = new SparseIntArray();
        try {
            time = SystemClock.uptimeMillis();
            assert mTabModelSelector.getModel(true).getCount() == 0;
            assert mTabModelSelector.getModel(false).getCount() == 0;
            checkAndUpdateMaxTabId();
            long timeWaitingForPrefetch = SystemClock.uptimeMillis();
            DataInputStream stream = mPrefetchTabListTask.get();
            logExecutionTime("LoadStateInternalPrefetchTime", timeWaitingForPrefetch);
            readSavedStateFile(stream,
                    createOnTabStateReadCallback(mTabModelSelector.isIncognitoSelected()), null);
            logExecutionTime("LoadStateInternalTime", time);
        } catch (Exception e) {
            // Catch generic exception to prevent a corrupted state from crashing app on startup.
            Log.d(TAG, "loadState exception: " + e.toString(), e);
        }

        if (mObserver != null) mObserver.onInitialized(mTabsToRestore.size());
    }

    /**
     * Restore tab state.  Tab state is loaded asynchronously, other than the active tab which
     * can be forced to load synchronously.
     *
     * @param setActiveTab If true the last active tab given in the saved state is loaded
     *                     synchronously and set as the current active tab. If false all tabs are
     *                     loaded asynchronously.
     */
    public void restoreTabs(boolean setActiveTab) {
        if (setActiveTab) {
            // Restore and select the active tab, which is first in the restore list.
            // If the active tab can't be restored, restore and select another tab. Otherwise, the
            // tab model won't have a valid index and the UI will break. http://crbug.com/261378
            while (!mTabsToRestore.isEmpty()
                    && mNormalTabsRestored.size() == 0
                    && mIncognitoTabsRestored.size() == 0) {
                TabRestoreDetails tabToRestore = mTabsToRestore.removeFirst();
                restoreTab(tabToRestore, true);
            }
        }
        loadNextTab();
    }

    /**
     * If a tab is being restored with the given url, then restore the tab in a frozen state
     * synchronously.
     */
    public void restoreTabStateForUrl(String url) {
        restoreTabStateInternal(url, Tab.INVALID_TAB_ID);
    }

    /**
     * If a tab is being restored with the given id, then restore the tab in a frozen state
     * synchronously.
     */
    public void restoreTabStateForId(int id) {
        restoreTabStateInternal(null, id);
    }

    private void restoreTabStateInternal(String url, int id) {
        TabRestoreDetails tabToRestore = null;
        if (mLoadTabTask != null) {
            if ((url == null && mLoadTabTask.mTabToRestore.id == id)
                    || (url != null && TextUtils.equals(mLoadTabTask.mTabToRestore.url, url))) {
                // Steal the task of restoring the tab from the active load tab task.
                mLoadTabTask.cancel(false);
                tabToRestore = mLoadTabTask.mTabToRestore;
                loadNextTab();  // Queue up async task to load next tab after we're done here.
            }
        }

        if (tabToRestore == null) {
            if (url == null) {
                tabToRestore = getTabToRestoreById(id);
            } else {
                tabToRestore = getTabToRestoreByUrl(url);
            }
        }

        if (tabToRestore != null) {
            mTabsToRestore.remove(tabToRestore);
            restoreTab(tabToRestore, false);
        }
    }

    private void restoreTab(TabRestoreDetails tabToRestore, boolean setAsActive) {
        // As we do this in startup, and restoring the active tab's state is critical, we permit
        // this read in the event that the prefetch task is not available. Either:
        // 1. The user just upgraded, has not yet set the new active tab id pref yet. Or
        // 2. restoreTab is used to preempt async queue and restore immediately on the UI thread.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            long time = SystemClock.uptimeMillis();
            TabState state;
            int restoredTabId = mPreferences.getInt(PREF_ACTIVE_TAB_ID, Tab.INVALID_TAB_ID);
            if (restoredTabId == tabToRestore.id && mPrefetchActiveTabTask != null) {
                long timeWaitingForPrefetch = SystemClock.uptimeMillis();
                state = mPrefetchActiveTabTask.get();
                logExecutionTime("RestoreTabPrefetchTime", timeWaitingForPrefetch);
            } else {
                // Necessary to do on the UI thread as a last resort.
                state = TabState.restoreTabState(getStateDirectory(), tabToRestore.id);
            }
            logExecutionTime("RestoreTabTime", time);
            restoreTab(tabToRestore, state, setAsActive);
        } catch (Exception e) {
            // Catch generic exception to prevent a corrupted state from crashing the app
            // at startup.
            Log.d(TAG, "loadTabs exception: " + e.toString(), e);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    private void restoreTab(
            TabRestoreDetails tabToRestore, TabState tabState, boolean setAsActive) {
        // If we don't have enough information about the Tab, bail out.
        boolean isIncognito = isIncognitoTabBeingRestored(tabToRestore, tabState);
        if (tabState == null) {
            if (tabToRestore.isIncognito == null) {
                Log.w(TAG, "Failed to restore tab: not enough info about its type was available.");
                return;
            } else if (isIncognito) {
                Log.i(TAG, "Failed to restore Incognito tab: its TabState could not be restored.");
                return;
            }
        }

        TabModel model = mTabModelSelector.getModel(isIncognito);
        SparseIntArray restoredTabs = isIncognito ? mIncognitoTabsRestored : mNormalTabsRestored;
        int restoredIndex = 0;
        if (restoredTabs.size() > 0
                && tabToRestore.originalIndex > restoredTabs.keyAt(restoredTabs.size() - 1)) {
            // Restore at end if our index is greater than all restored tabs.
            restoredIndex = restoredTabs.size();
        } else {
             // Otherwise try to find the tab we should restore before, if any.
            for (int i = 0; i < restoredTabs.size(); i++) {
                if (restoredTabs.keyAt(i) > tabToRestore.originalIndex) {
                    Tab nextTabByIndex = TabModelUtils.getTabById(model, restoredTabs.valueAt(i));
                    restoredIndex = nextTabByIndex != null ? model.indexOf(nextTabByIndex) : -1;
                    break;
                }
            }
        }

        if (tabState != null) {
            mTabCreatorManager.getTabCreator(isIncognito).createFrozenTab(
                    tabState, tabToRestore.id, restoredIndex);
        } else {
            Log.w(TAG, "Failed to restore TabState; creating Tab with last known URL.");
            Tab fallbackTab = mTabCreatorManager.getTabCreator(isIncognito).createNewTab(
                    new LoadUrlParams(tabToRestore.url), TabModel.TabLaunchType.FROM_LINK, null);
            model.moveTab(fallbackTab.getId(), restoredIndex);
        }

        if (setAsActive) {
            TabModelUtils.setIndex(model, TabModelUtils.getTabIndexById(model, tabToRestore.id));
        }
        restoredTabs.put(tabToRestore.originalIndex, tabToRestore.id);
    }

    /**
     * @return Number of restored tabs on cold startup.
     */
    public int getRestoredTabCount() {
        return mTabsToRestore.size();
    }

    /**
     * Deletes all files in the tab state directory.
     */
    public void clearState() {
        synchronized (CLEAN_UP_TASK_LOCK) {
            if (sCleanupTask != null) sCleanupTask.cancel(true);
        }

        // Delete state files first. These files are deleted serially, ensuring that new state
        // files aren't created before the state files are cleared.
        for (int i = 0; i < TabWindowManager.MAX_SIMULTANEOUS_SELECTORS; i++) {
            deleteFileAsync(getStateFileName(i));
        }

        // Clean up the rest of the persistent store data.
        cleanUpPersistentData(true);

        onStateLoaded();
    }

    /**
     * Cancels loading of {@link Tab}s from disk from saved state. This is useful if the user
     * does an action which impacts all {@link Tab}s, not just the ones currently loaded into
     * the model. For example, if the user tries to close all {@link Tab}s, we need don't want
     * to restore old {@link Tab}s anymore.
     *
     * @param incognito Whether or not to ignore incognito {@link Tab}s or normal
     *                  {@link Tab}s as they are being restored.
     */
    public void cancelLoadingTabs(boolean incognito) {
        if (incognito) {
            mCancelIncognitoTabLoads = true;
        } else {
            mCancelNormalTabLoads = true;
        }
    }

    public void addTabToSaveQueue(Tab tab) {
        // TODO(ianwen): remove this check once we figure out a better plan to disable tab saving.
        if (tab.getDelegateFactory() instanceof CustomTabDelegateFactory) {
            return;
        }
        if (!mTabsToSave.contains(tab) && tab.isTabStateDirty() && !isTabUrlContentScheme(tab)) {
            mTabsToSave.addLast(tab);
        }
        saveNextTab();
    }

    public void removeTabFromQueues(Tab tab) {
        mTabsToSave.remove(tab);
        mTabsToRestore.remove(getTabToRestoreById(tab.getId()));

        if (mLoadTabTask != null && mLoadTabTask.mTabToRestore.id == tab.getId()) {
            mLoadTabTask.cancel(false);
            mLoadTabTask = null;
            loadNextTab();
        }

        if (mSaveTabTask != null && mSaveTabTask.mId == tab.getId()) {
            mSaveTabTask.cancel(false);
            mSaveTabTask = null;
            saveNextTab();
        }

        cleanupPersistentData(tab.getId(), tab.isIncognito());
    }

    private TabRestoreDetails getTabToRestoreByUrl(String url) {
        for (TabRestoreDetails tabBeingRestored : mTabsToRestore) {
            if (TextUtils.equals(tabBeingRestored.url, url)) {
                return tabBeingRestored;
            }
        }
        return null;
    }

    private TabRestoreDetails getTabToRestoreById(int id) {
        for (TabRestoreDetails tabBeingRestored : mTabsToRestore) {
            if (tabBeingRestored.id == id) {
                return tabBeingRestored;
            }
        }
        return null;
    }

    public void destroy() {
        mDestroyed = true;
        if (mLoadTabTask != null) mLoadTabTask.cancel(true);
        mTabsToSave.clear();
        mTabsToRestore.clear();
        if (mSaveTabTask != null) mSaveTabTask.cancel(false);
        if (mSaveListTask != null) mSaveListTask.cancel(true);
    }

    private void cleanupPersistentData(int id, boolean incognito) {
        deleteFileAsync(TabState.getTabStateFilename(id, incognito));
        // No need to forward that event to the tab content manager as this is already
        // done as part of the standard tab removal process.
    }

    private byte[] serializeTabMetadata() throws IOException {
        List<TabRestoreDetails> tabsToRestore = new ArrayList<TabRestoreDetails>();

        // The metadata file may be being written out before all of the Tabs have been restored.
        // Save that information out, as well.
        if (mLoadTabTask != null) tabsToRestore.add(mLoadTabTask.mTabToRestore);
        for (TabRestoreDetails details : mTabsToRestore) {
            tabsToRestore.add(details);
        }

        return serializeTabModelSelector(mTabModelSelector, tabsToRestore);
    }

    /**
     * Serializes {@code selector} to a byte array, copying out the data pertaining to tab ordering
     * and selected indices.
     * @param selector          The {@link TabModelSelector} to serialize.
     * @param tabsBeingRestored Tabs that are in the process of being restored.
     * @return                  {@code byte[]} containing the serialized state of {@code selector}.
     */
    @VisibleForTesting
    public static byte[] serializeTabModelSelector(TabModelSelector selector,
            List<TabRestoreDetails> tabsBeingRestored) throws IOException {
        ThreadUtils.assertOnUiThread();

        TabModel incognitoModel = selector.getModel(true);
        TabModelMetadata incognitoInfo = new TabModelMetadata(incognitoModel.index());
        for (int i = 0; i < incognitoModel.getCount(); i++) {
            incognitoInfo.ids.add(incognitoModel.getTabAt(i).getId());
            incognitoInfo.urls.add(incognitoModel.getTabAt(i).getUrl());
        }

        TabModel normalModel = selector.getModel(false);
        TabModelMetadata normalInfo = new TabModelMetadata(normalModel.index());
        for (int i = 0; i < normalModel.getCount(); i++) {
            normalInfo.ids.add(normalModel.getTabAt(i).getId());
            normalInfo.urls.add(normalModel.getTabAt(i).getUrl());
        }

        // Cache the active tab id to be pre-loaded next launch.
        int activeTabId = Tab.INVALID_TAB_ID;
        int activeIndex = normalModel.index();
        if (activeIndex != TabList.INVALID_TAB_INDEX) {
            activeTabId = normalModel.getTabAt(activeIndex).getId();
        }
        // Always override the existing value in case there is no active tab.
        ContextUtils.getAppSharedPreferences().edit().putInt(
                PREF_ACTIVE_TAB_ID, activeTabId).apply();

        return serializeMetadata(normalInfo, incognitoInfo, tabsBeingRestored);
    }

    /**
     * Serializes data from a {@link TabModelSelector} into a byte array.
     * @param standardInfo      Info about the regular {@link TabModel}.
     * @param incognitoInfo     Info about the Incognito {@link TabModel}.
     * @param tabsBeingRestored Tabs that are in the process of being restored.
     * @return                  {@code byte[]} containing the serialized state of {@code selector}.
     */
    public static byte[] serializeMetadata(TabModelMetadata standardInfo,
            TabModelMetadata incognitoInfo, @Nullable List<TabRestoreDetails> tabsBeingRestored)
            throws IOException {
        ThreadUtils.assertOnUiThread();

        int standardCount = standardInfo.ids.size();
        int incognitoCount = incognitoInfo.ids.size();

        // Determine how many Tabs there are, including those not yet been added to the TabLists.
        int numAlreadyLoaded = incognitoCount + standardCount;
        int numStillBeingLoaded = tabsBeingRestored == null ? 0 : tabsBeingRestored.size();
        int numTabsTotal = numStillBeingLoaded + numAlreadyLoaded;

        // Save the index file containing the list of tabs to restore.
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        DataOutputStream stream = new DataOutputStream(output);
        stream.writeInt(SAVED_STATE_VERSION);
        stream.writeInt(numTabsTotal);
        stream.writeInt(incognitoCount);
        stream.writeInt(incognitoInfo.index);
        stream.writeInt(standardInfo.index + incognitoCount);
        Log.d(TAG, "Serializing tab lists; counts: " + standardCount
                + ", " + incognitoCount
                + ", " + (tabsBeingRestored == null ? 0 : tabsBeingRestored.size()));

        // Save incognito state first, so when we load, if the incognito files are unreadable
        // we can fall back easily onto the standard selected tab.
        for (int i = 0; i < incognitoCount; i++) {
            stream.writeInt(incognitoInfo.ids.get(i));
            stream.writeUTF(incognitoInfo.urls.get(i));
        }
        for (int i = 0; i < standardCount; i++) {
            stream.writeInt(standardInfo.ids.get(i));
            stream.writeUTF(standardInfo.urls.get(i));
        }

        // Write out information about the tabs that haven't finished being loaded.
        // We shouldn't have to worry about Tab duplication because the tab details are processed
        // only on the UI Thread.
        if (tabsBeingRestored != null) {
            for (TabRestoreDetails details : tabsBeingRestored) {
                stream.writeInt(details.id);
                stream.writeUTF(details.url);
            }
        }

        stream.close();
        return output.toByteArray();
    }

    private void saveListToFile(byte[] listData) {
        if (Arrays.equals(mLastSavedMetadata, listData)) return;

        saveListToFile(getStateDirectory(), mSelectorIndex, listData);
        mLastSavedMetadata = listData;
        if (LibraryLoader.isInitialized()) {
            RecordHistogram.recordCountHistogram(
                    "Android.TabPersistentStore.MetadataFileSize", listData.length);
        }
    }

    /**
     * Atomically writes the given serialized data out to disk.
     * @param stateDirectory Directory to save TabModel data into.
     * @param selectorIndex  Index of the TabModelSelector to write out.
     * @param listData       TabModel data in the form of a serialized byte array.
     */
    public static void saveListToFile(File stateDirectory, int selectorIndex, byte[] listData) {
        synchronized (SAVE_LIST_LOCK) {
            // Save the index file containing the list of tabs to restore.
            File metadataFile = new File(stateDirectory, getStateFileName(selectorIndex));

            AtomicFile file = new AtomicFile(metadataFile);
            FileOutputStream stream = null;
            try {
                stream = file.startWrite();
                stream.write(listData, 0, listData.length);
                file.finishWrite(stream);
            } catch (IOException e) {
                if (stream != null) file.failWrite(stream);
                Log.e(TAG, "Failed to write file: " + metadataFile.getAbsolutePath());
            }
        }
    }

    /**
     * @param isIncognitoSelected Whether the tab model is incognito.
     * @return A callback for reading data from tab models.
     */
    private OnTabStateReadCallback createOnTabStateReadCallback(final boolean isIncognitoSelected) {
        return new OnTabStateReadCallback() {
            @Override
            public void onDetailsRead(int index, int id, String url, Boolean isIncognito,
                    boolean isStandardActiveIndex, boolean isIncognitoActiveIndex) {
                // Note that incognito tab may not load properly so we may need to use
                // the current tab from the standard model.
                // This logic only works because we store the incognito indices first.
                TabRestoreDetails details =
                        new TabRestoreDetails(id, index, isIncognito, url);

                if ((isIncognitoActiveIndex && isIncognitoSelected)
                        || (isStandardActiveIndex && !isIncognitoSelected)) {
                    // Active tab gets loaded first
                    mTabsToRestore.addFirst(details);
                } else {
                    mTabsToRestore.addLast(details);
                }

                if (mObserver != null) {
                    mObserver.onDetailsRead(
                            index, id, url, isStandardActiveIndex, isIncognitoActiveIndex);
                }
            }
        };
    }

    /**
     * If a global max tab ID has not been computed and stored before, then check all the state
     * folders and calculate a new global max tab ID to be used. Must be called before any new tabs
     * are created.
     *
     * @throws IOException
     */
    private void checkAndUpdateMaxTabId() throws IOException {
        if (mPreferences.getBoolean(PREF_HAS_COMPUTED_MAX_ID, false)) return;

        int maxId = 0;
        // Calculation of the max tab ID is done only once per user and is stored in
        // SharedPreferences afterwards.  This is done on the UI thread because it is on the
        // critical patch to initializing the TabIdManager with the correct max tab ID.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            for (int i = 0; i < TabWindowManager.MAX_SIMULTANEOUS_SELECTORS; i++) {
                File stateFile = new File(getStateDirectory(), getStateFileName(i));
                if (!stateFile.exists()) continue;

                DataInputStream stream = null;
                try {
                    stream = new DataInputStream(new BufferedInputStream(new FileInputStream(
                            stateFile)));
                    maxId = Math.max(maxId, readSavedStateFile(stream, null, null));
                } finally {
                    StreamUtil.closeQuietly(stream);
                }
            }
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
        TabIdManager.getInstance().incrementIdCounterTo(maxId);
        mPreferences.edit().putBoolean(PREF_HAS_COMPUTED_MAX_ID, true).apply();
    }

    private int readSavedStateFile(DataInputStream stream, OnTabStateReadCallback callback,
            SparseArray<Boolean> tabIds) throws IOException {
        long time = SystemClock.uptimeMillis();
        int nextId = 0;
        boolean skipUrlRead = false;
        boolean skipIncognitoCount = false;
        final int version = stream.readInt();
        if (version != SAVED_STATE_VERSION) {
            // We don't support restoring Tab data from before M18.
            if (version < 3) return 0;
            // Older versions are missing newer data.
            if (version < 5) skipIncognitoCount = true;
            if (version < 4) skipUrlRead = true;
        }

        final int count = stream.readInt();
        final int incognitoCount = skipIncognitoCount ? -1 : stream.readInt();
        final int incognitoActiveIndex = stream.readInt();
        final int standardActiveIndex = stream.readInt();
        if (count < 0 || incognitoActiveIndex >= count || standardActiveIndex >= count) {
            throw new IOException();
        }

        for (int i = 0; i < count; i++) {
            int id = stream.readInt();
            String tabUrl = skipUrlRead ? "" : stream.readUTF();
            if (id >= nextId) nextId = id + 1;
            if (tabIds != null) tabIds.append(id, true);

            Boolean isIncognito = (incognitoCount < 0) ? null : i < incognitoCount;
            if (callback != null) {
                callback.onDetailsRead(i, id, tabUrl, isIncognito,
                        i == standardActiveIndex, i == incognitoActiveIndex);
            }
        }
        logExecutionTime("ReadSavedStateTime", time);
        return nextId;
    }

    private void saveNextTab() {
        if (mSaveTabTask != null) return;
        if (!mTabsToSave.isEmpty()) {
            Tab tab = mTabsToSave.removeFirst();
            mSaveTabTask = new SaveTabTask(tab);
            mSaveTabTask.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
        } else {
            saveTabListAsynchronously();
        }
    }

    /**
     * Kick off an AsyncTask to save the current list of Tabs.
     */
    public void saveTabListAsynchronously() {
        if (mSaveListTask != null) mSaveListTask.cancel(true);
        mSaveListTask = new SaveListTask();
        mSaveListTask.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    private class SaveTabTask extends AsyncTask<Void, Void, Void> {
        Tab mTab;
        int mId;
        TabState mState;
        boolean mEncrypted;
        boolean mStateSaved = false;

        SaveTabTask(Tab tab) {
            mTab = tab;
            mId = tab.getId();
            mEncrypted = tab.isIncognito();
        }

        @Override
        protected void onPreExecute() {
            if (mDestroyed || isCancelled()) return;
            mState = mTab.getState();
        }

        @Override
        protected Void doInBackground(Void... voids) {
            mStateSaved = saveTabState(mId, mEncrypted, mState);
            return null;
        }

        @Override
        protected void onPostExecute(Void v) {
            if (mDestroyed || isCancelled()) return;
            if (mStateSaved) mTab.setIsTabStateDirty(false);
            mSaveTabTask = null;
            saveNextTab();
        }
    }

    private class SaveListTask extends AsyncTask<Void, Void, Void> {
        byte[] mListData;

        @Override
        protected void onPreExecute() {
            if (mDestroyed || isCancelled()) return;
            try {
                mListData = serializeTabMetadata();
            } catch (IOException e) {
                mListData = null;
            }
        }

        @Override
        protected Void doInBackground(Void... voids) {
            if (mListData == null || isCancelled()) return null;
            saveListToFile(mListData);
            mListData = null;
            return null;
        }

        @Override
        protected void onPostExecute(Void v) {
            if (mDestroyed || isCancelled()) return;

            if (mSaveListTask == this) {
                mSaveListTask = null;
                if (mObserver != null) mObserver.onMetadataSavedAsynchronously();
            }
        }
    }

    private void onStateLoaded() {
        if (mObserver != null) mObserver.onStateLoaded(mContext);
    }

    private void loadNextTab() {
        if (mDestroyed) return;

        if (mTabsToRestore.isEmpty()) {
            mNormalTabsRestored = null;
            mIncognitoTabsRestored = null;
            // Only clean up persistent data on application cold start.
            if (mSelectorIndex == 0
                    && TabWindowManager.getInstance().getNumberOfAssignedTabModelSelectors() == 1) {
                cleanUpPersistentData(false);
            }
            onStateLoaded();
            mLoadTabTask = null;
            Log.d(TAG, "Loaded tab lists; counts: " + mTabModelSelector.getModel(false).getCount()
                    + "," + mTabModelSelector.getModel(true).getCount());
        } else {
            TabRestoreDetails tabToRestore = mTabsToRestore.removeFirst();
            mLoadTabTask = new LoadTabTask(tabToRestore);
            mLoadTabTask.execute();
        }
    }

    /**
     * Creates an asynchronous task to delete persistent data. The task is run using a thread pool
     * and may be executed in parallel with other tasks. The cleanup task use a combination of the
     * current model and the tab state files for other models to determine which tab files should
     * be deleted. The cleanup task should be canceled if a second tab model is created.
     *
     * @param deleteAllFiles Whether all tab files should be deleted regardless of whether
     *                       they are found in a tab model.
     */
    private void cleanUpPersistentData(boolean deleteAllFiles) {
        synchronized (CLEAN_UP_TASK_LOCK) {
            if (sCleanupTask != null) sCleanupTask.cancel(true);
            sCleanupTask = new CleanUpTabStateDataTask(deleteAllFiles);
            sCleanupTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        }

        if (mTabContentManager != null) {
            mTabContentManager.cleanUpPersistentData(mTabModelSelector);
        }
    }

    /**
     * File mutations (e.g. saving & deleting) are explicitly serialized to ensure that they occur
     * in the correct order.
     *
     * @param file Name of file under the state directory to be deleted.
     */
    private void deleteFileAsync(final String file) {
        new AsyncTask<Void, Void, Void>() {
            @Override
            protected Void doInBackground(Void... voids) {
                File stateFile = new File(getStateDirectory(), file);
                if (stateFile.exists()) {
                    if (!stateFile.delete()) Log.e(TAG, "Failed to delete file: " + stateFile);
                }
                return null;
            }
        }.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
        // TODO(twellington): delete tab files using the thread pool rather than the serial
        // executor.
    }

    private static class CleanUpTabStateDataInfo {
        private final String[] mTabFileNames;
        private final SparseArray<Boolean> mOtherTabIds;

        public CleanUpTabStateDataInfo(String[] tabFileNames, SparseArray<Boolean> otherTabIds) {
            mTabFileNames = tabFileNames;
            mOtherTabIds = otherTabIds;
        }
    }

    private class CleanUpTabStateDataTask extends AsyncTask<Void, Void, CleanUpTabStateDataInfo> {
        private final boolean mDeleteAllFiles;

        /**
         * Creates a new AsyncTask to delete tab files. If deleteAllFiles is true, all tab files
         * will be deleted regardless of whether they currently belong to a tab model. If it is
         * false, only tab files that no longer belong to a model will be deleted. Tab files for
         * custom tabs will be deleted regardless of the deleteAllFiles value.
         */
        public CleanUpTabStateDataTask(boolean deleteAllFiles) {
            mDeleteAllFiles = deleteAllFiles;
        }

        @Override
        protected CleanUpTabStateDataInfo doInBackground(Void... voids) {
            if (mDestroyed) {
                return null;
            }
            SparseArray<Boolean> otherTabIds = new SparseArray<Boolean>();
            if (!mDeleteAllFiles) getTabsFromOtherStateFiles(otherTabIds);

            return new CleanUpTabStateDataInfo(getStateDirectory().list(), otherTabIds);
        }

        @Override
        protected void onPostExecute(CleanUpTabStateDataInfo tabStateInfo) {
            if (mDestroyed || tabStateInfo.mTabFileNames == null) return;

            for (String fileName : tabStateInfo.mTabFileNames) {
                Pair<Integer, Boolean> data = TabState.parseInfoFromFilename(fileName);
                if (data != null) {
                    TabModel model = mTabModelSelector.getModel(data.second);
                    if (mDeleteAllFiles || (TabModelUtils.getTabById(model, data.first) == null
                            && tabStateInfo.mOtherTabIds.get(data.first) == null)) {
                        // It might be more efficient to use a single task for all files, but
                        // the number of files is expected to be very small.
                        deleteFileAsync(fileName);
                    }
                }
            }
        }
    }

    /**
     * Gets the IDs of all tabs in TabModelSelectors other than the currently selected one. IDs for
     * custom tabs are excluded.
     * @param tabIds SparseArray to populate with TabIds.
     */
    private void getTabsFromOtherStateFiles(SparseArray<Boolean> tabIds) {
        for (int i = 0; i < TabWindowManager.MAX_SIMULTANEOUS_SELECTORS; i++) {
            if (i == mSelectorIndex) continue;

            File metadataFile = new File(getStateDirectory(), getStateFileName(i));
            if (metadataFile.exists()) {
                DataInputStream stream;
                try {
                    stream = new DataInputStream(
                            new BufferedInputStream(new FileInputStream(metadataFile)));
                    readSavedStateFile(stream, null, tabIds);
                } catch (Exception e) {
                    Log.e(TAG, "Unable to read state for " + metadataFile.getName() + ": " + e);
                }
            }
        }
    }

    private class LoadTabTask extends AsyncTask<Void, Void, TabState> {
        public final TabRestoreDetails mTabToRestore;

        public LoadTabTask(TabRestoreDetails tabToRestore) {
            mTabToRestore = tabToRestore;
        }

        @Override
        protected TabState doInBackground(Void... voids) {
            if (mDestroyed || isCancelled()) return null;
            try {
                return TabState.restoreTabState(getStateDirectory(), mTabToRestore.id);
            } catch (Exception e) {
                Log.w(TAG, "Unable to read state: " + e);
                return null;
            }
        }

        @Override
        protected void onPostExecute(TabState tabState) {
            if (mDestroyed || isCancelled()) return;

            boolean isIncognito = isIncognitoTabBeingRestored(mTabToRestore, tabState);
            boolean isLoadCancelled = (isIncognito && mCancelIncognitoTabLoads)
                    || (!isIncognito && mCancelNormalTabLoads);
            if (!isLoadCancelled) restoreTab(mTabToRestore, tabState, false);

            loadNextTab();
        }
    }

    private static final class TabRestoreDetails {
        public final int id;
        public final int originalIndex;
        public final String url;
        public final Boolean isIncognito;

        public TabRestoreDetails(int id, int originalIndex, Boolean isIncognito, String url) {
            this.id = id;
            this.originalIndex = originalIndex;
            this.url = url;
            this.isIncognito = isIncognito;
        }
    }

    /**
     * Creates a task to move and rename state files so that they are ultimately in the correct
     * directory. All state files are stored the same directory.
     */
    private void startMigrationTaskIfNecessary() {
        final boolean hasRunLegacyMigration =
                mPreferences.getBoolean(PREF_HAS_RUN_FILE_MIGRATION, false);
        final boolean hasRunMultiInstanceMigration =
                mPreferences.getBoolean(PREF_HAS_RUN_MULTI_INSTANCE_FILE_MIGRATION, false);

        if (hasRunLegacyMigration && hasRunMultiInstanceMigration) return;

        synchronized (MIGRATION_LOCK) {
            if (sMigrationTask != null) return;
            sMigrationTask = new AsyncTask<Void, Void, Void>() {
                @Override
                protected Void doInBackground(Void... params) {
                    if (!hasRunLegacyMigration) {
                        performLegacyMigration();
                        // If a legacy migration was performed, a multi-instance migration is not
                        // necessary.
                        setMultiInstanceFileMigrationPref();
                    } else {
                        performMultiInstanceMigration();
                    }
                    return null;
                }
            }.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
        }
    }

    /**
     * Upgrades users from an old version of Chrome when the state file was still in the root
     * directory.
     */
    @WorkerThread
    private void performLegacyMigration() {
        File newFolder = getStateDirectory();
        File[] newFiles = newFolder.listFiles();
        // Attempt migration if we have no tab state file in the new directory.
        if (newFiles == null || newFiles.length == 0) {
            File oldFolder = mContext.getFilesDir();
            File modelFile = new File(oldFolder, SAVED_STATE_FILE);
            if (modelFile.exists()) {
                if (!modelFile.renameTo(new File(newFolder,
                        getStateFileName(mSelectorIndex)))) {
                    Log.e(TAG, "Failed to rename file: " + modelFile);
                }
            }

            File[] files = oldFolder.listFiles();
            if (files != null) {
                for (File file : files) {
                    if (TabState.parseInfoFromFilename(file.getName()) != null) {
                        if (!file.renameTo(new File(newFolder, file.getName()))) {
                            Log.e(TAG, "Failed to rename file: " + file);
                        }
                    }
                }
            }
        }
        setLegacyFileMigrationPref();
    }

    /**
     * Upgrades users from an older version of Chrome when the state files for multi-instance
     * were each kept in separate subdirectories.
     */
    @WorkerThread
    private void performMultiInstanceMigration() {
        // 1. Rename tab metadata file for tab directory "0".
        File stateDir = getStateDirectory();
        File metadataFile = new File(stateDir, SAVED_STATE_FILE);
        if (metadataFile.exists()) {
            if (!metadataFile.renameTo(new File(stateDir, getStateFileName(mSelectorIndex)))) {
                Log.e(TAG, "Failed to rename file: " + metadataFile);
            }
        }

        // 2. Move files from other state directories.
        for (int i = TabModelSelectorImpl.CUSTOM_TABS_SELECTOR_INDEX;
                i < TabWindowManager.MAX_SIMULTANEOUS_SELECTORS; i++) {
            // Skip the directory we're migrating to.
            if (i == 0) continue;

            File otherStateDir = new File(BaseStateDirectoryHolder.sDirectory, Integer.toString(i));
            if (otherStateDir == null || !otherStateDir.exists()) continue;

            // Rename tab state file.
            metadataFile = new File(otherStateDir, SAVED_STATE_FILE);
            if (metadataFile.exists()) {
                if (!metadataFile.renameTo(new File(stateDir, getStateFileName(i)))) {
                    Log.e(TAG, "Failed to rename file: " + metadataFile);
                }
            }

            // Rename tab files.
            File[] files = otherStateDir.listFiles();
            if (files != null) {
                for (File file : files) {
                    if (TabState.parseInfoFromFilename(file.getName()) != null) {
                        // Custom tabs does not currently use tab files. Delete them rather than
                        // migrating.
                        if (i == TabModelSelectorImpl.CUSTOM_TABS_SELECTOR_INDEX) {
                            if (!file.delete()) {
                                Log.e(TAG, "Failed to delete file: " + file);
                            }
                            continue;
                        }

                        // If the tab was moved between windows in Android N multi-window, the tab
                        // file may exist in both directories. Keep whichever was modified more
                        // recently.
                        File newFileName = new File(stateDir, file.getName());
                        if (newFileName.exists()
                                && newFileName.lastModified() > file.lastModified()) {
                            if (!file.delete()) {
                                Log.e(TAG, "Failed to delete file: " + file);
                            }
                        } else if (!file.renameTo(newFileName)) {
                            Log.e(TAG, "Failed to rename file: " + file);
                        }
                    }
                }
            }

            // Delete other state directory.
            if (!otherStateDir.delete()) {
                Log.e(TAG, "Failed to delete directory: " + otherStateDir);
            }
        }

        setMultiInstanceFileMigrationPref();
    }

    private void setLegacyFileMigrationPref() {
        mPreferences.edit().putBoolean(PREF_HAS_RUN_FILE_MIGRATION, true).apply();
    }

    private void setMultiInstanceFileMigrationPref() {
        mPreferences.edit().putBoolean(PREF_HAS_RUN_MULTI_INSTANCE_FILE_MIGRATION, true).apply();
    }

    private boolean isTabUrlContentScheme(Tab tab) {
        String url = tab.getUrl();
        return url != null && url.startsWith("content");
    }

    /**
     * Determines if a Tab being restored is definitely an Incognito Tab.
     *
     * This function can fail to determine if a Tab is incognito if not enough data about the Tab
     * was successfully saved out.
     *
     * @return True if the tab is definitely Incognito, false if it's not or if it's undecideable.
     */
    private boolean isIncognitoTabBeingRestored(TabRestoreDetails tabDetails, TabState tabState) {
        if (tabState != null) {
            // The Tab's previous state was completely restored.
            return tabState.isIncognito();
        } else if (tabDetails.isIncognito != null) {
            // The TabState couldn't be restored, but we have some information about the tab.
            return tabDetails.isIncognito;
        } else {
            // The tab's type is undecideable.
            return false;
        }
    }

    private void startPrefetchTabListTask() {
        mPrefetchTabListTask = new AsyncTask<Void, Void, DataInputStream>() {
            @Override
            protected DataInputStream doInBackground(Void... params) {
                // This getStateDirectory should be the first call and will cache the result.
                File stateFile = new File(getStateDirectory(), getStateFileName(mSelectorIndex));
                if (!stateFile.exists()) return null;
                FileInputStream stream = null;
                byte[] data;
                try {
                    stream = new FileInputStream(stateFile);
                    data = new byte[(int) stateFile.length()];
                    stream.read(data);
                } catch (IOException exception) {
                    return null;
                } finally {
                    StreamUtil.closeQuietly(stream);
                }
                return new DataInputStream(new ByteArrayInputStream(data));
            }
        }.executeOnExecutor(getPrefetchExecutor());
    }

    private void startPrefetchActiveTabTask() {
        final int activeTabId = mPreferences.getInt(PREF_ACTIVE_TAB_ID, Tab.INVALID_TAB_ID);
        if (activeTabId == Tab.INVALID_TAB_ID) return;
        mPrefetchActiveTabTask = new AsyncTask<Void, Void, TabState>() {
            @Override
            protected TabState doInBackground(Void... params) {
                return TabState.restoreTabState(getStateDirectory(), activeTabId);
            }
        }.executeOnExecutor(getPrefetchExecutor());
    }

    private Executor getPrefetchExecutor() {
        return sMigrationTask == null ? AsyncTask.THREAD_POOL_EXECUTOR : AsyncTask.SERIAL_EXECUTOR;
    }

    /**
     * Directory containing all data for TabModels.  Each subdirectory stores info about different
     * TabModelSelectors, including metadata about each TabModel and TabStates for each of their
     * tabs.
     *
     * @return The parent state directory.
     */
    @VisibleForTesting
    public static File getOrCreateBaseStateDirectory() {
        Context appContext = ContextUtils.getApplicationContext();
        return appContext.getDir(BASE_STATE_FOLDER, Context.MODE_PRIVATE);
    }

    /**
     * @param selectorIndex The index that represents which state file to pull and save state to.
     * @return The name of the state file.
     */
    @VisibleForTesting
    public static String getStateFileName(int selectorIndex) {
        return SAVED_STATE_FILE + Integer.toString(selectorIndex);
    }

    /**
     * Sets where the base state directory is in tests.
     */
    @VisibleForTesting
    public static void setBaseStateDirectoryForTests(File directory) {
        BaseStateDirectoryHolder.sDirectory = directory;
    }
}
