// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.app.Activity;
import android.content.Context;
import android.util.Log;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.CalledByNative;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.chrome.browser.identity.UniqueIdentificationGenerator;
import org.chromium.sync.internal_api.pub.PassphraseType;
import org.chromium.sync.internal_api.pub.base.ModelType;

import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Set;
import java.util.SortedSet;
import java.util.TreeSet;
import java.util.concurrent.CopyOnWriteArrayList;

/**
 * Android wrapper of the ProfileSyncService which provides access from the Java layer.
 * <p/>
 * This class mostly wraps native classes, but it make a few business logic decisions, both in Java
 * and in native.
 * <p/>
 * Only usable from the UI thread as the native ProfileSyncService requires its access to be in the
 * UI thread.
 * <p/>
 * See chrome/browser/sync/profile_sync_service.h for more details.
 */
public class ProfileSyncService {

    /**
     * Listener for the underlying sync status.
     */
    public interface SyncStateChangedListener {
        // Invoked when the status has changed.
        public void syncStateChanged();
    }

    private static final String TAG = "ProfileSyncService";

    @VisibleForTesting
    public static final String SESSION_TAG_PREFIX = "session_sync";

    private static ProfileSyncService sSyncSetupManager;

    @VisibleForTesting
    protected final Context mContext;

    // Sync state changes more often than listeners are added/removed, so using CopyOnWrite.
    private final List<SyncStateChangedListener> mListeners =
            new CopyOnWriteArrayList<SyncStateChangedListener>();

    // Native ProfileSyncServiceAndroid object. Can not be final since we set it to 0 in destroy().
    private final long mNativeProfileSyncServiceAndroid;

    /**
     * A helper method for retrieving the application-wide SyncSetupManager.
     * <p/>
     * Can only be accessed on the main thread.
     *
     * @param context the ApplicationContext is retrieved from the context used as an argument.
     * @return a singleton instance of the SyncSetupManager
     */
    @SuppressFBWarnings("LI_LAZY_INIT")
    public static ProfileSyncService get(Context context) {
        ThreadUtils.assertOnUiThread();
        if (sSyncSetupManager == null) {
            sSyncSetupManager = new ProfileSyncService(context);
        }
        return sSyncSetupManager;
    }

    /**
     * This is called pretty early in our application. Avoid any blocking operations here.
     */
    private ProfileSyncService(Context context) {
        ThreadUtils.assertOnUiThread();
        // We should store the application context, as we outlive any activity which may create us.
        mContext = context.getApplicationContext();

        // This may cause us to create ProfileSyncService even if sync has not
        // been set up, but ProfileSyncService::Startup() won't be called until
        // credentials are available.
        mNativeProfileSyncServiceAndroid = nativeInit();

        // When the application gets paused, tell sync to flush the directory to disk.
        ApplicationStatus.registerStateListenerForAllActivities(new ActivityStateListener() {
            @Override
            public void onActivityStateChange(Activity activity, int newState) {
                if (newState == ActivityState.PAUSED) {
                    flushDirectory();
                }
            }
        });
    }

    @CalledByNative
    private static long getProfileSyncServiceAndroid(Context context) {
        return get(context).mNativeProfileSyncServiceAndroid;
    }

    /**
     * If we are currently in the process of setting up sync, this method clears the
     * sync setup in progress flag.
     */
    @VisibleForTesting
    public void finishSyncFirstSetupIfNeeded() {
        if (isFirstSetupInProgress()) {
            setSyncSetupCompleted();
            setSetupInProgress(false);
        }
    }

    public void signOut() {
        nativeSignOutSync(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Signs in to sync, using the currently signed-in account.
     */
    public void syncSignIn() {
        nativeSignInSync(mNativeProfileSyncServiceAndroid);
        // Notify listeners right away that the sync state has changed (native side does not do
        // this)
        syncStateChanged();
    }

    public String querySyncStatus() {
        ThreadUtils.assertOnUiThread();
        return nativeQuerySyncStatusSummary(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Sets the the machine tag used by session sync to a unique value.
     */
    public void setSessionsId(UniqueIdentificationGenerator generator) {
        ThreadUtils.assertOnUiThread();
        String uniqueTag = generator.getUniqueId(null);
        if (uniqueTag.isEmpty()) {
            Log.e(TAG, "Unable to get unique tag for sync. "
                    + "This may lead to unexpected tab sync behavior.");
            return;
        }
        String sessionTag = SESSION_TAG_PREFIX + uniqueTag;
        if (!nativeSetSyncSessionsId(mNativeProfileSyncServiceAndroid, sessionTag)) {
            Log.e(TAG, "Unable to write session sync tag. "
                    + "This may lead to unexpected tab sync behavior.");
        }
    }

    /**
     * Returns the actual passphrase type being used for encryption.
     * The sync backend must be running (isSyncInitialized() returns true) before
     * calling this function.
     * <p/>
     * This method should only be used if you want to know the raw value. For checking whether
     * we should ask the user for a passphrase, use isPassphraseRequiredForDecryption().
     */
    public PassphraseType getPassphraseType() {
        assert isSyncInitialized();
        int passphraseType = nativeGetPassphraseType(mNativeProfileSyncServiceAndroid);
        return PassphraseType.fromInternalValue(passphraseType);
    }

    public boolean isSyncKeystoreMigrationDone() {
        assert isSyncInitialized();
        return nativeIsSyncKeystoreMigrationDone(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Returns true if the current explicit passphrase time is defined.
     */
    public boolean hasExplicitPassphraseTime() {
        assert isSyncInitialized();
        return nativeHasExplicitPassphraseTime(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Returns the current explicit passphrase time in milliseconds since epoch.
     */
    public long getExplicitPassphraseTime() {
        assert isSyncInitialized();
        return nativeGetExplicitPassphraseTime(mNativeProfileSyncServiceAndroid);
    }

    public String getSyncEnterGooglePassphraseBodyWithDateText() {
        assert isSyncInitialized();
        return nativeGetSyncEnterGooglePassphraseBodyWithDateText(mNativeProfileSyncServiceAndroid);
    }

    public String getSyncEnterCustomPassphraseBodyWithDateText() {
        assert isSyncInitialized();
        return nativeGetSyncEnterCustomPassphraseBodyWithDateText(mNativeProfileSyncServiceAndroid);
    }

    public String getCurrentSignedInAccountText() {
        assert isSyncInitialized();
        return nativeGetCurrentSignedInAccountText(mNativeProfileSyncServiceAndroid);
    }

    public String getSyncEnterCustomPassphraseBodyText() {
        return nativeGetSyncEnterCustomPassphraseBodyText(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Checks if sync is currently set to use a custom passphrase. The sync backend must be running
     * (isSyncInitialized() returns true) before calling this function.
     *
     * @return true if sync is using a custom passphrase.
     */
    public boolean isUsingSecondaryPassphrase() {
        assert isSyncInitialized();
        return nativeIsUsingSecondaryPassphrase(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Checks if we need a passphrase to decrypt a currently-enabled data type. This returns false
     * if a passphrase is needed for a type that is not currently enabled.
     *
     * @return true if we need a passphrase.
     */
    public boolean isPassphraseRequiredForDecryption() {
        assert isSyncInitialized();
        return nativeIsPassphraseRequiredForDecryption(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Checks if we need a passphrase to decrypt any data type (including types that aren't
     * currently enabled or supported, such as passwords). This API is used to determine if we
     * need to provide a decryption passphrase before we can re-encrypt with a custom passphrase.
     *
     * @return true if we need a passphrase for some type.
     */
    public boolean isPassphraseRequiredForExternalType() {
        assert isSyncInitialized();
        return nativeIsPassphraseRequiredForExternalType(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Checks if the sync backend is running.
     *
     * @return true if sync is initialized/running.
     */
    public boolean isSyncInitialized() {
        return nativeIsSyncInitialized(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Checks if the first sync setup is currently in progress.
     *
     * @return true if first sync setup is in progress
     */
    public boolean isFirstSetupInProgress() {
        return nativeIsFirstSetupInProgress(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Checks if encrypting all the data types is allowed.
     *
     * @return true if encrypting all data types is allowed, false if only passwords are allowed to
     * be encrypted.
     */
    public boolean isEncryptEverythingAllowed() {
        assert isSyncInitialized();
        return nativeIsEncryptEverythingAllowed(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Checks if the all the data types are encrypted.
     *
     * @return true if all data types are encrypted, false if only passwords are encrypted.
     */
    public boolean isEncryptEverythingEnabled() {
        assert isSyncInitialized();
        return nativeIsEncryptEverythingEnabled(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Turns on encryption of all data types. This only takes effect after sync configuration is
     * completed and setPreferredDataTypes() is invoked.
     */
    public void enableEncryptEverything() {
        assert isSyncInitialized();
        nativeEnableEncryptEverything(mNativeProfileSyncServiceAndroid);
    }

    public void setEncryptionPassphrase(String passphrase, boolean isGaia) {
        assert isSyncInitialized();
        nativeSetEncryptionPassphrase(mNativeProfileSyncServiceAndroid, passphrase, isGaia);
    }

    public boolean isCryptographerReady() {
        assert isSyncInitialized();
        return nativeIsCryptographerReady(mNativeProfileSyncServiceAndroid);
    }

    public boolean setDecryptionPassphrase(String passphrase) {
        assert isSyncInitialized();
        return nativeSetDecryptionPassphrase(mNativeProfileSyncServiceAndroid, passphrase);
    }

    public GoogleServiceAuthError.State getAuthError() {
        int authErrorCode = nativeGetAuthError(mNativeProfileSyncServiceAndroid);
        return GoogleServiceAuthError.State.fromCode(authErrorCode);
    }

    /**
     * Gets the set of data types that are currently syncing.
     *
     * This is affected by whether sync is on.
     *
     * @return Set of active data types.
     */
    public Set<ModelType> getActiveDataTypes() {
        long modelTypeSelection = nativeGetActiveDataTypes(mNativeProfileSyncServiceAndroid);
        return modelTypeSelectionToSet(modelTypeSelection);
    }

    /**
     * Gets the set of data types that are enabled in sync.
     *
     * This is unaffected by whether sync is on.
     *
     * @return Set of preferred types.
     */
    public Set<ModelType> getPreferredDataTypes() {
        // TODO(maxbogue): Correct this line to GetPreferredDataTypes once
        // downstream uses are fixed.
        long modelTypeSelection = nativeGetActiveDataTypes(mNativeProfileSyncServiceAndroid);
        return modelTypeSelectionToSet(modelTypeSelection);
    }

    @VisibleForTesting
    public static Set<ModelType> modelTypeSelectionToSet(long modelTypeSelection) {
        Set<ModelType> syncTypes = new HashSet<ModelType>();
        if ((modelTypeSelection & ModelTypeSelection.AUTOFILL) != 0) {
            syncTypes.add(ModelType.AUTOFILL);
        }
        if ((modelTypeSelection & ModelTypeSelection.AUTOFILL_PROFILE) != 0) {
            syncTypes.add(ModelType.AUTOFILL_PROFILE);
        }
        if ((modelTypeSelection & ModelTypeSelection.BOOKMARK) != 0) {
            syncTypes.add(ModelType.BOOKMARK);
        }
        if ((modelTypeSelection & ModelTypeSelection.EXPERIMENTS) != 0) {
            syncTypes.add(ModelType.EXPERIMENTS);
        }
        if ((modelTypeSelection & ModelTypeSelection.NIGORI) != 0) {
            syncTypes.add(ModelType.NIGORI);
        }
        if ((modelTypeSelection & ModelTypeSelection.PASSWORD) != 0) {
            syncTypes.add(ModelType.PASSWORD);
        }
        if ((modelTypeSelection & ModelTypeSelection.SESSION) != 0) {
            syncTypes.add(ModelType.SESSION);
        }
        if ((modelTypeSelection & ModelTypeSelection.TYPED_URL) != 0) {
            syncTypes.add(ModelType.TYPED_URL);
        }
        if ((modelTypeSelection & ModelTypeSelection.HISTORY_DELETE_DIRECTIVE) != 0) {
            syncTypes.add(ModelType.HISTORY_DELETE_DIRECTIVE);
        }
        if ((modelTypeSelection & ModelTypeSelection.DEVICE_INFO) != 0) {
            syncTypes.add(ModelType.DEVICE_INFO);
        }
        if ((modelTypeSelection & ModelTypeSelection.PROXY_TABS) != 0) {
            syncTypes.add(ModelType.PROXY_TABS);
        }
        if ((modelTypeSelection & ModelTypeSelection.FAVICON_IMAGE) != 0) {
            syncTypes.add(ModelType.FAVICON_IMAGE);
        }
        if ((modelTypeSelection & ModelTypeSelection.FAVICON_TRACKING) != 0) {
            syncTypes.add(ModelType.FAVICON_TRACKING);
        }
        if ((modelTypeSelection & ModelTypeSelection.SUPERVISED_USER_SETTING) != 0) {
            syncTypes.add(ModelType.MANAGED_USER_SETTING);
        }
        if ((modelTypeSelection & ModelTypeSelection.SUPERVISED_USER_WHITELIST) != 0) {
            syncTypes.add(ModelType.MANAGED_USER_WHITELIST);
        }
        return syncTypes;
    }

    public boolean hasKeepEverythingSynced() {
        return nativeHasKeepEverythingSynced(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Enables syncing for the passed data types.
     *
     * @param syncEverything Set to true if the user wants to sync all data types
     *                       (including new data types we add in the future).
     * @param enabledTypes   The set of types to enable. Ignored (can be null) if
     *                       syncEverything is true.
     */
    public void setPreferredDataTypes(boolean syncEverything, Set<ModelType> enabledTypes) {
        long modelTypeSelection = 0;
        if (syncEverything || enabledTypes.contains(ModelType.AUTOFILL)) {
            modelTypeSelection |= ModelTypeSelection.AUTOFILL;
        }
        if (syncEverything || enabledTypes.contains(ModelType.BOOKMARK)) {
            modelTypeSelection |= ModelTypeSelection.BOOKMARK;
        }
        if (syncEverything || enabledTypes.contains(ModelType.PASSWORD)) {
            modelTypeSelection |= ModelTypeSelection.PASSWORD;
        }
        if (syncEverything || enabledTypes.contains(ModelType.PROXY_TABS)) {
            modelTypeSelection |= ModelTypeSelection.PROXY_TABS;
        }
        if (syncEverything || enabledTypes.contains(ModelType.TYPED_URL)) {
            modelTypeSelection |= ModelTypeSelection.TYPED_URL;
        }
        nativeSetPreferredDataTypes(
                mNativeProfileSyncServiceAndroid, syncEverything, modelTypeSelection);
    }

    public void setSyncSetupCompleted() {
        nativeSetSyncSetupCompleted(mNativeProfileSyncServiceAndroid);
    }

    public boolean hasSyncSetupCompleted() {
        return nativeHasSyncSetupCompleted(mNativeProfileSyncServiceAndroid);
    }

    public boolean isStartSuppressed() {
        return nativeIsStartSuppressed(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Notifies sync whether sync setup is in progress - this tells sync whether it should start
     * syncing data types when it starts up, or if it should just stay in "configuration mode".
     *
     * @param inProgress True to put sync in configuration mode, false to turn off configuration
     *                   and allow syncing.
     */
    public void setSetupInProgress(boolean inProgress) {
        nativeSetSetupInProgress(mNativeProfileSyncServiceAndroid, inProgress);
    }

    public void addSyncStateChangedListener(SyncStateChangedListener listener) {
        ThreadUtils.assertOnUiThread();
        mListeners.add(listener);
    }

    public void removeSyncStateChangedListener(SyncStateChangedListener listener) {
        ThreadUtils.assertOnUiThread();
        mListeners.remove(listener);
    }

    public boolean hasUnrecoverableError() {
        return nativeHasUnrecoverableError(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Called when the state of the native sync engine has changed, so various
     * UI elements can update themselves.
     */
    @CalledByNative
    public void syncStateChanged() {
        if (!mListeners.isEmpty()) {
            for (SyncStateChangedListener listener : mListeners) {
                listener.syncStateChanged();
            }
        }
    }

    @VisibleForTesting
    public String getSyncInternalsInfoForTest() {
        ThreadUtils.assertOnUiThread();
        return nativeGetAboutInfoForTest(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Starts the sync engine.
     */
    public void enableSync() {
        nativeEnableSync(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Stops the sync engine.
     */
    public void disableSync() {
        nativeDisableSync(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Flushes the sync directory.
     */
    public void flushDirectory() {
        nativeFlushDirectory(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Returns the time when the last sync cycle was completed.
     *
     * @return The difference measured in microseconds, between last sync cycle completion time
     * and 1 January 1970 00:00:00 UTC.
     */
    @VisibleForTesting
    public long getLastSyncedTimeForTest() {
        return nativeGetLastSyncedTimeForTest(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Overrides the Sync engine's NetworkResources. This is used to set up the Sync FakeServer for
     * testing.
     *
     * @param networkResources the pointer to the NetworkResources created by the native code. It
     *                         is assumed that the Java caller has ownership of this pointer;
     *                         ownership is transferred as part of this call.
     */
    public void overrideNetworkResourcesForTest(long networkResources) {
        nativeOverrideNetworkResourcesForTest(mNativeProfileSyncServiceAndroid, networkResources);
    }

    @CalledByNative
    private static String modelTypeSelectionToStringForTest(long modelTypeSelection) {
        SortedSet<String> set = new TreeSet<String>();
        Set<ModelType> filteredTypes = ModelType.filterOutNonInvalidationTypes(
                modelTypeSelectionToSet(modelTypeSelection));
        for (ModelType type : filteredTypes) {
            set.add(type.toString());
        }
        StringBuilder sb = new StringBuilder();
        Iterator<String> it = set.iterator();
        if (it.hasNext()) {
            sb.append(it.next());
            while (it.hasNext()) {
                sb.append(", ");
                sb.append(it.next());
            }
        }
        return sb.toString();
    }

    /**
     * @return Whether sync is enabled to sync urls or open tabs with a non custom passphrase.
     */
    public boolean isSyncingUrlsWithKeystorePassphrase() {
        return isSyncInitialized()
            && getPreferredDataTypes().contains(ModelType.TYPED_URL)
            && getPassphraseType().equals(PassphraseType.KEYSTORE_PASSPHRASE);
    }

    // Native methods
    private native long nativeInit();
    private native void nativeEnableSync(long nativeProfileSyncServiceAndroid);
    private native void nativeDisableSync(long nativeProfileSyncServiceAndroid);
    private native void nativeFlushDirectory(long nativeProfileSyncServiceAndroid);
    private native void nativeSignInSync(long nativeProfileSyncServiceAndroid);
    private native void nativeSignOutSync(long nativeProfileSyncServiceAndroid);
    private native boolean nativeSetSyncSessionsId(
            long nativeProfileSyncServiceAndroid, String tag);
    private native String nativeQuerySyncStatusSummary(long nativeProfileSyncServiceAndroid);
    private native int nativeGetAuthError(long nativeProfileSyncServiceAndroid);
    private native boolean nativeIsSyncInitialized(long nativeProfileSyncServiceAndroid);
    private native boolean nativeIsFirstSetupInProgress(long nativeProfileSyncServiceAndroid);
    private native boolean nativeIsEncryptEverythingAllowed(long nativeProfileSyncServiceAndroid);
    private native boolean nativeIsEncryptEverythingEnabled(long nativeProfileSyncServiceAndroid);
    private native void nativeEnableEncryptEverything(long nativeProfileSyncServiceAndroid);
    private native boolean nativeIsPassphraseRequiredForDecryption(
            long nativeProfileSyncServiceAndroid);
    private native boolean nativeIsPassphraseRequiredForExternalType(
            long nativeProfileSyncServiceAndroid);
    private native boolean nativeIsUsingSecondaryPassphrase(long nativeProfileSyncServiceAndroid);
    private native boolean nativeSetDecryptionPassphrase(
            long nativeProfileSyncServiceAndroid, String passphrase);
    private native void nativeSetEncryptionPassphrase(
            long nativeProfileSyncServiceAndroid, String passphrase, boolean isGaia);
    private native boolean nativeIsCryptographerReady(long nativeProfileSyncServiceAndroid);
    private native int nativeGetPassphraseType(long nativeProfileSyncServiceAndroid);
    private native boolean nativeHasExplicitPassphraseTime(long nativeProfileSyncServiceAndroid);
    private native long nativeGetExplicitPassphraseTime(long nativeProfileSyncServiceAndroid);
    private native String nativeGetSyncEnterGooglePassphraseBodyWithDateText(
            long nativeProfileSyncServiceAndroid);
    private native String nativeGetSyncEnterCustomPassphraseBodyWithDateText(
            long nativeProfileSyncServiceAndroid);
    private native String nativeGetCurrentSignedInAccountText(long nativeProfileSyncServiceAndroid);
    private native String nativeGetSyncEnterCustomPassphraseBodyText(
            long nativeProfileSyncServiceAndroid);
    private native boolean nativeIsSyncKeystoreMigrationDone(long nativeProfileSyncServiceAndroid);
    private native long nativeGetActiveDataTypes(long nativeProfileSyncServiceAndroid);
    private native long nativeGetPreferredDataTypes(long nativeProfileSyncServiceAndroid);
    private native void nativeSetPreferredDataTypes(
            long nativeProfileSyncServiceAndroid, boolean syncEverything, long modelTypeSelection);
    private native void nativeSetSetupInProgress(
            long nativeProfileSyncServiceAndroid, boolean inProgress);
    private native void nativeSetSyncSetupCompleted(long nativeProfileSyncServiceAndroid);
    private native boolean nativeHasSyncSetupCompleted(long nativeProfileSyncServiceAndroid);
    private native boolean nativeIsStartSuppressed(long nativeProfileSyncServiceAndroid);
    private native boolean nativeHasKeepEverythingSynced(long nativeProfileSyncServiceAndroid);
    private native boolean nativeHasUnrecoverableError(long nativeProfileSyncServiceAndroid);
    private native String nativeGetAboutInfoForTest(long nativeProfileSyncServiceAndroid);
    private native long nativeGetLastSyncedTimeForTest(long nativeProfileSyncServiceAndroid);
    private native void nativeOverrideNetworkResourcesForTest(
            long nativeProfileSyncServiceAndroid, long networkResources);
}
