// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.AsyncTask;
import android.os.Build;
import android.util.Pair;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.FileUtils;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.StreamUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.TabState;
import org.chromium.chrome.browser.document.DocumentActivity;
import org.chromium.chrome.browser.document.DocumentUtils;
import org.chromium.chrome.browser.document.IncognitoDocumentActivity;
import org.chromium.chrome.browser.preferences.DocumentModeManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabModelMetadata;
import org.chromium.chrome.browser.tabmodel.document.ActivityDelegate;
import org.chromium.chrome.browser.tabmodel.document.ActivityDelegateImpl;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModel;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModelImpl;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModelSelector;
import org.chromium.chrome.browser.tabmodel.document.StorageDelegate;
import org.chromium.chrome.browser.util.FeatureUtilities;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.channels.FileChannel;
import java.util.HashSet;
import java.util.Set;

/**
 * Divorces Chrome's tabs from Android's Overview menu.  Assumes native libraries are unavailable.
 *
 * Migration from document mode to tabbed mode occurs in two main phases:
 *
 * 1) NON-DESTRUCTIVE MIGRATION:
 *    TabState files for the normal DocumentTabModel are copied from the document mode directories
 *    into the tabbed mode directory.  Incognito tabs are silently dropped, as with the previous
 *    migration pathway.
 *
 *    TODO(dfalcantara): Check what happens if a user last viewed an Incognito tab.
 *    TODO(dfalcantara): Check what happens on other launchers.
 *
 *    Once all TabState files are copied, a TabModel metadata file is written out for the tabbed
 *    mode {@link TabModelImpl} to read out.  Because the native library is not available, the file
 *    will be incomplete but usable; it will be corrected by the TabModelImpl when it loads it and
 *    all of the TabState files up.  See {@link #writeTabModelMetadata} for details.
 *
 * 2) CLEANUP OF ALL DOCUMENT-RELATED THINGS:
 *    DocumentActivity tasks in Android's Recents are removed, TabState files in the document mode
 *    directory are deleted, and document mode preferences are cleared.
 *
 *    TODO(dfalcantara): Clean up the incognito notification, if possible.
 *    TODO(dfalcantara): Add histograms for tracking migration progress.
 *
 * TODO(dfalcantara): Potential pitfalls that need to be accounted for:
 *   - Consistently crashing during migration means you can never open Chrome until you clear data.
 *   - Successfully migrating, but crashing while deleting things and closing off tasks.
 *   - Failing to copy all the TabState files over during migration because of a lack of space.
 */
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
public class DocumentModeAssassin {
    /** Alerted about progress along the migration pipeline. */
    public static class DocumentModeAssassinObserver {
        /**
         * Called on the UI thread when the DocumentModeAssassin has progressed along its pipeline,
         * and when the DocumentModeAssasssinObserver is first added to the DocumentModeAssassin.
         *
         * @param newStage New stage of the pipeline.
         */
        public void onStageChange(int newStage) {
        }

        /**
         * Called on the background thread when a TabState file has been copied from the document
         * to tabbed mode directory.
         *
         * @param copiedId ID of the Tab whose TabState file was copied.
         */
        public void onTabStateFileCopied(int copiedId) {
        }
    }

    /** Stages of the pipeline.  Each stage is blocked off by a STARTED and DONE pair. */
    static final int STAGE_UNINITIALIZED = 0;
    static final int STAGE_INITIALIZED = 1;
    static final int STAGE_COPY_TAB_STATES_STARTED = 2;
    static final int STAGE_COPY_TAB_STATES_DONE = 3;
    static final int STAGE_WRITE_TABMODEL_METADATA_STARTED = 4;
    static final int STAGE_WRITE_TABMODEL_METADATA_DONE = 5;
    static final int STAGE_CHANGE_SETTINGS_STARTED = 6;
    static final int STAGE_CHANGE_SETTINGS_DONE = 7;
    static final int STAGE_DELETION_STARTED = 8;
    static final int STAGE_DONE = 9;

    private static final String TAG = "DocumentModeAssassin";

    /** Which TabModelSelectorImpl to copy files into during migration. */
    private static final int TAB_MODEL_INDEX = 0;

    /** Creates and holds the Singleton. */
    private static class LazyHolder {
        private static final DocumentModeAssassin INSTANCE = new DocumentModeAssassin();
    }

    /** Returns the Singleton instance. */
    public static DocumentModeAssassin getInstance() {
        return LazyHolder.INSTANCE;
    }

    /** IDs of Tabs that have had their TabState files copied between directories successfully. */
    private final Set<Integer> mMigratedTabIds = new HashSet<Integer>();

    /** Observers of the migration pipeline. */
    private final ObserverList<DocumentModeAssassinObserver> mObservers =
            new ObserverList<DocumentModeAssassinObserver>();

    /** Current stage of the migration. */
    private int mStage = STAGE_UNINITIALIZED;

    /** Whether or not startStage is allowed to progress along the migration pipeline. */
    private boolean mIsPipelineActive;

    /** Returns whether or not a migration to tabbed mode from document mode is necessary. */
    public static boolean isMigrationNecessary() {
        return FeatureUtilities.isDocumentMode(ApplicationStatus.getApplicationContext());
    }

    /** Migrates the user from document mode to tabbed mode if necessary. */
    @VisibleForTesting
    public void migrateFromDocumentToTabbedMode() {
        ThreadUtils.assertOnUiThread();

        if (!isMigrationNecessary()) {
            // Don't kick off anything if we don't need to.
            setStage(STAGE_UNINITIALIZED, STAGE_DONE);
            return;
        } else if (mStage != STAGE_UNINITIALIZED) {
            // Migration is already underway.
            return;
        }

        // TODO(dfalcantara): Add a pathway to catch repeated migration failures.

        setStage(STAGE_UNINITIALIZED, STAGE_INITIALIZED);
    }

    /**
     * Makes copies of {@link TabState} files in the document mode directory and places them in the
     * tabbed mode directory.  Only non-Incognito tabs are transferred.
     *
     * TODO(dfalcantara): Prevent migrating chrome:// pages?
     *
     * @param selectedTabId     ID of the last viewed non-Incognito tab.
     * @param documentDirectory File pointing at the DocumentTabModel TabState file directory.
     * @param tabbedDirectory   File pointing at tabbed mode TabState file directory.
     */
    void copyTabStateFiles(
            final int selectedTabId, final File documentDirectory, final File tabbedDirectory) {
        ThreadUtils.assertOnUiThread();
        if (!setStage(STAGE_INITIALIZED, STAGE_COPY_TAB_STATES_STARTED)) return;

        new AsyncTask<Void, Void, Void>() {
            @Override
            protected Void doInBackground(Void... params) {
                Log.d(TAG, "Copying TabState files from document to tabbed mode directory.");
                assert mMigratedTabIds.size() == 0;

                File[] allTabStates = documentDirectory.listFiles();
                if (allTabStates != null) {
                    // If we know what tab the user was last viewing, copy just that TabState file
                    // before all the other ones to mitigate storage issues for devices with limited
                    // available storage.
                    if (selectedTabId != Tab.INVALID_TAB_ID) {
                        copyTabStateFilesInternal(allTabStates, selectedTabId, true);
                    }

                    // Copy over everything else.
                    copyTabStateFilesInternal(allTabStates, selectedTabId, false);
                }
                return null;
            }

            @Override
            protected void onPostExecute(Void result) {
                Log.d(TAG, "Finished copying files.");
                setStage(STAGE_COPY_TAB_STATES_STARTED, STAGE_COPY_TAB_STATES_DONE);
            }

            /**
             * Copies the files from the document mode directory to the tabbed mode directory.
             *
             * @param allTabStates        Listing of all files in the document mode directory.
             * @param selectedTabId       ID of the non-Incognito tab the user last viewed.  May be
             *                            {@link Tab#INVALID_TAB_ID} if the ID is unknown.
             * @param copyOnlySelectedTab Copy only the TabState file for the selectedTabId.
             */
            private void copyTabStateFilesInternal(
                    File[] allTabStates, int selectedTabId, boolean copyOnlySelectedTab) {
                assert !ThreadUtils.runningOnUiThread();
                for (int i = 0; i < allTabStates.length; i++) {
                    // Trawl the directory for non-Incognito TabState files.
                    String fileName = allTabStates[i].getName();
                    Pair<Integer, Boolean> tabInfo = TabState.parseInfoFromFilename(fileName);
                    if (tabInfo == null || tabInfo.second) continue;

                    // Ignore any files that are not relevant for the current pass.
                    int tabId = tabInfo.first;
                    if (selectedTabId != Tab.INVALID_TAB_ID) {
                        if (copyOnlySelectedTab && tabId != selectedTabId) continue;
                        if (!copyOnlySelectedTab && tabId == selectedTabId) continue;
                    }

                    // Copy the file over.
                    File oldFile = allTabStates[i];
                    File newFile = new File(tabbedDirectory, fileName);
                    FileInputStream inputStream = null;
                    FileOutputStream outputStream = null;

                    try {
                        inputStream = new FileInputStream(oldFile);
                        outputStream = new FileOutputStream(newFile);

                        FileChannel inputChannel = inputStream.getChannel();
                        FileChannel outputChannel = outputStream.getChannel();
                        inputChannel.transferTo(0, inputChannel.size(), outputChannel);
                        mMigratedTabIds.add(tabId);

                        for (DocumentModeAssassinObserver observer : mObservers) {
                            observer.onTabStateFileCopied(tabId);
                        }
                    } catch (IOException e) {
                        Log.e(TAG, "Failed to copy: " + oldFile.getName() + " to "
                                + newFile.getName());
                    } finally {
                        StreamUtil.closeQuietly(inputStream);
                        StreamUtil.closeQuietly(outputStream);
                    }
                }
            }
        }.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }


    /**
     * Converts information about the normal {@link DocumentTabModel} into info for
     * {@link TabModelImpl}, then persists it to storage.  Incognito is intentionally not migrated.
     *
     * Because the native library is not available, we have no way of getting the URL from the
     * {@link TabState} files.  Instead, the TabModel metadata file this function writes out
     * writes out inaccurate URLs for each tab:
     * - When the TabState for a Tab is available, a URL of "" is saved out because the
     *   {@link TabPersistentStore} ignores it and restores it from the TabState, anyway.
     *
     * - If a TabState isn't available, we fall back to using the initial URL that was used to spawn
     *   a document mode Tab.
     *
     * These tradeoffs are deemed acceptable because the URL from the metadata file isn't commonly
     * used immediately:
     *
     * 1) {@link TabPersistentStore} uses the URL to allow reusing already open tabs for Home screen
     *    Intents.  If a Tab doesn't match the Intent's URL, a new Tab is created.  This is already
     *    the case when a cold start launches into document mode because the data is unavailable at
     *    startup.
     *
     * 2) {@link TabModelImpl} uses the URL when it fails to load a Tab's persisted TabState.  This
     *    means that the user loses some navigation history, but it's not a case document mode would
     *    have been able to recover from anyway because the TabState stores the URL data.
     *
     * @param tabbedDirectory Directory containing all of the main TabModel's files.
     * @param normalTabModel  DocumentTabModel containing information about non-Incognito tabs.
     * @param migratedTabIds  IDs of Tabs whose TabState files were copied successfully.
     */
    void writeTabModelMetadata(final File tabbedDirectory, final DocumentTabModel normalTabModel,
            final Set<Integer> migratedTabIds) {
        ThreadUtils.assertOnUiThread();
        if (!setStage(STAGE_COPY_TAB_STATES_DONE, STAGE_WRITE_TABMODEL_METADATA_STARTED)) return;

        new AsyncTask<Void, Void, Boolean>() {
            private byte[] mSerializedMetadata;

            @Override
            protected void onPreExecute() {
                Log.d(TAG, "Beginning to write tabbed mode metadata files.");

                // Collect information about all the normal tabs on the UI thread.
                TabModelMetadata normalMetadata = new TabModelMetadata(normalTabModel.index());
                for (int i = 0; i < normalTabModel.getCount(); i++) {
                    int tabId = normalTabModel.getTabAt(i).getId();
                    normalMetadata.ids.add(tabId);

                    if (migratedTabIds.contains(tabId)) {
                        // Don't save a URL because it's in the TabState.
                        normalMetadata.urls.add("");
                    } else {
                        // The best that can be done is to fall back to the initial URL for the Tab.
                        Log.e(TAG, "Couldn't restore state for #" + tabId + "; using initial URL.");
                        normalMetadata.urls.add(normalTabModel.getInitialUrlForDocument(tabId));
                    }
                }

                // Incognito tabs are dropped.
                TabModelMetadata incognitoMetadata =
                        new TabModelMetadata(TabModel.INVALID_TAB_INDEX);

                try {
                    mSerializedMetadata = TabPersistentStore.serializeMetadata(
                            normalMetadata, incognitoMetadata, null);
                } catch (IOException e) {
                    Log.e(TAG, "Failed to serialize the TabModel.", e);
                    mSerializedMetadata = null;
                }
            }

            @Override
            protected Boolean doInBackground(Void... params) {
                if (mSerializedMetadata != null) {
                    TabPersistentStore.saveListToFile(tabbedDirectory, mSerializedMetadata);
                    return true;
                } else {
                    return false;
                }
            }

            @Override
            protected void onPostExecute(Boolean result) {
                // TODO(dfalcantara): What do we do if the metadata file failed to be written out?
                Log.d(TAG, "Finished writing tabbed mode metadata file.");
                setStage(STAGE_WRITE_TABMODEL_METADATA_STARTED, STAGE_WRITE_TABMODEL_METADATA_DONE);
            }
        }.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    /**
     * Moves the user to tabbed mode by opting them out.
     * @param context Context to grab SharedPreferences from.
     */
    void changePreferences(Context context) {
        ThreadUtils.assertOnUiThread();
        if (!setStage(STAGE_WRITE_TABMODEL_METADATA_DONE, STAGE_CHANGE_SETTINGS_STARTED)) return;

        // Record that the user has opted-out of document mode now that their data has been
        // safely copied to the other directory.
        Log.d(TAG, "Setting tabbed mode preference.");
        DocumentModeManager.getInstance(context).setOptedOutState(
                DocumentModeManager.OPTED_OUT_OF_DOCUMENT_MODE);

        setStage(STAGE_CHANGE_SETTINGS_STARTED, STAGE_CHANGE_SETTINGS_DONE);
    }

    /** TODO(dfalcantara): Add a unit test for this function. */
    private void deleteDocumentModeData(final Context context) {
        ThreadUtils.assertOnUiThread();
        if (!setStage(STAGE_CHANGE_SETTINGS_DONE, STAGE_DELETION_STARTED)) return;

        new AsyncTask<Void, Void, Void>() {
            @Override
            protected Void doInBackground(Void... params) {
                Log.d(TAG, "Starting to delete document mode data.");

                // Remove all the {@link DocumentActivity} tasks from Android's Recents list.  Users
                // viewing Recents during migration will continue to see their tabs until they exit.
                // Reselecting a migrated tab will kick the user to the launcher without crashing.
                // TODO(dfalcantara): Confirm that the different Android flavors work the same way.
                ActivityDelegate delegate = new ActivityDelegateImpl(
                        DocumentActivity.class, IncognitoDocumentActivity.class);
                delegate.finishAllDocumentActivities();

                // Delete the old tab state directory.
                StorageDelegate migrationStorageDelegate = new StorageDelegate();
                FileUtils.recursivelyDeleteFile(migrationStorageDelegate.getStateDirectory());

                // Clean up the {@link DocumentTabModel} shared preferences.
                SharedPreferences prefs = context.getSharedPreferences(
                        DocumentTabModelImpl.PREF_PACKAGE, Context.MODE_PRIVATE);
                SharedPreferences.Editor editor = prefs.edit();
                editor.clear();
                editor.apply();
                return null;
            }

            @Override
            protected void onPostExecute(Void result) {
                Log.d(TAG, "Finished deleting document mode data.");
                setStage(STAGE_DELETION_STARTED, STAGE_DONE);
            }
        }.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    /**
     * Updates the stage of the migration.
     * @param expectedStage Stage of the pipeline that is currently expected.
     * @param newStage      Stage of the pipeline that is being activated.
     */
    private boolean setStage(int expectedStage, int newStage) {
        ThreadUtils.assertOnUiThread();

        assert mStage == expectedStage;
        mStage = newStage;

        for (DocumentModeAssassinObserver callback : mObservers) callback.onStageChange(newStage);
        startStage(newStage);
        return true;
    }

    /**
     * Kicks off tasks for the new state of the pipeline.
     *
     * We don't wait for the DocumentTabModel to finish parsing its metadata file before proceeding
     * with migration because it doesn't have actionable information:
     *
     * 1) WE DON'T NEED TO RE-POPULATE THE "RECENTLY CLOSED" LIST:
     *    The metadata file contains a list of tabs Chrome knew about before it died, which
     *    could differ from the list of tabs in Android Overview.  The canonical list of
     *    live tabs, however, has always been the ones displayed by the Android Overview.
     *
     * 2) RETARGETING MIGRATED TABS FROM THE HOME SCREEN IS A CORNER CASE:
     *    The only downside here is that Chrome ends up creating a new tab for a home screen
     *    shortcut the first time they start Chrome after migration.  This was already
     *    broken for document mode during cold starts, anyway.
     */
    private void startStage(int newStage) {
        ThreadUtils.assertOnUiThread();
        if (!mIsPipelineActive) return;

        Context context = ApplicationStatus.getApplicationContext();
        if (newStage == STAGE_INITIALIZED) {
            Log.d(TAG, "Migrating user into tabbed mode.");
            DocumentTabModelSelector selector = ChromeApplication.getDocumentTabModelSelector();
            DocumentTabModelImpl normalTabModel =
                    (DocumentTabModelImpl) selector.getModel(false);
            int selectedTabId = DocumentUtils.getLastShownTabIdFromPrefs(context, false);

            File documentDirectory = normalTabModel.getStorageDelegate().getStateDirectory();
            File tabbedDirectory = TabPersistentStore.getStateDirectory(context, TAB_MODEL_INDEX);
            copyTabStateFiles(selectedTabId, documentDirectory, tabbedDirectory);
        } else if (newStage == STAGE_COPY_TAB_STATES_DONE) {
            Log.d(TAG, "Writing tabbed mode metadata file.");
            DocumentTabModelSelector selector = ChromeApplication.getDocumentTabModelSelector();
            DocumentTabModelImpl normalTabModel =
                    (DocumentTabModelImpl) selector.getModel(false);
            File tabbedDirectory =
                    TabPersistentStore.getStateDirectory(context, TAB_MODEL_INDEX);
            writeTabModelMetadata(tabbedDirectory, normalTabModel, mMigratedTabIds);
        } else if (newStage == STAGE_WRITE_TABMODEL_METADATA_DONE) {
            Log.d(TAG, "Changing user preference.");
            changePreferences(context);
        } else if (newStage == STAGE_CHANGE_SETTINGS_DONE) {
            Log.d(TAG, "Cleaning up document mode data.");
            deleteDocumentModeData(context);
        }
    }


    /**
     * Returns the current stage of the pipeline.
     */
    @VisibleForTesting
    public int getStage() {
        return mStage;
    }


    /**
     * Adds a observer that is alerted as migration progresses.
     *
     * @param observer Observer to add.
     */
    @VisibleForTesting
    public void addObserver(final DocumentModeAssassinObserver observer) {
        ThreadUtils.assertOnUiThread();
        mObservers.addObserver(observer);
    }

    /**
     * Removes an Observer.
     *
     * @param observer Observer to remove.
     */
    @VisibleForTesting
    public void removeObserver(final DocumentModeAssassinObserver observer) {
        ThreadUtils.assertOnUiThread();
        mObservers.removeObserver(observer);
    }

    /**
     * Creates a DocumentModeAssassin that starts at the given stage and does not automatically
     * move along the pipeline.
     *
     * @param stage Stage to start at.  See the STAGE_* values above.
     * @return DocumentModeAssassin that can be used for testing specific stages of the pipeline.
     */
    @VisibleForTesting
    public static DocumentModeAssassin createForTesting(int stage) {
        return new DocumentModeAssassin(stage, false);
    }

    private DocumentModeAssassin() {
        this(isMigrationNecessary() ? STAGE_UNINITIALIZED : STAGE_DONE, true);
    }

    private DocumentModeAssassin(int stage, boolean isPipelineActive) {
        mStage = stage;
        mIsPipelineActive = isPipelineActive;
    }
}
