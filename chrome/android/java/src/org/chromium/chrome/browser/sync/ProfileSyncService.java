// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.accounts.Account;
import android.content.Context;
import android.os.AsyncTask;
import android.util.Log;

import com.google.common.annotations.VisibleForTesting;

import org.chromium.base.CalledByNative;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.identity.UniqueIdentificationGenerator;
import org.chromium.sync.internal_api.pub.SyncDecryptionPassphraseType;
import org.chromium.sync.internal_api.pub.base.ModelType;
import org.chromium.sync.notifier.SyncStatusHelper;
import org.chromium.sync.signin.AccountManagerHelper;

import java.util.HashSet;
import java.util.List;
import java.util.Set;
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

    public interface SyncStateChangedListener {
        // Invoked when the underlying sync status has changed.
        public void syncStateChanged();
    }

    private static final String TAG = ProfileSyncService.class.getSimpleName();

    @VisibleForTesting
    public static final String SESSION_TAG_PREFIX = "session_sync";

    private static ProfileSyncService sSyncSetupManager;

    @VisibleForTesting
    protected final Context mContext;

    // Sync state changes more often than listeners are added/removed, so using CopyOnWrite.
    private final List<SyncStateChangedListener> mListeners =
            new CopyOnWriteArrayList<SyncStateChangedListener>();

    // Native ProfileSyncServiceAndroid object. Can not be final since we set it to 0 in destroy().
    private final int mNativeProfileSyncServiceAndroid;

    /**
     * A helper method for retrieving the application-wide SyncSetupManager.
     * <p/>
     * Can only be accessed on the main thread.
     *
     * @param context the ApplicationContext is retrieved from the context used as an argument.
     * @return a singleton instance of the SyncSetupManager
     */
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
    }

    @CalledByNative
    private static int getProfileSyncServiceAndroid(Context context) {
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
     * Signs in to sync, using the existing auth token.
     */
    public void syncSignIn(String account) {
        syncSignInWithAuthToken(account, "");
    }

    /**
     * Signs in to sync.
     *
     * @param account   The username of the account that is signing in.
     * @param authToken A chromiumsync auth token for sync to use, or empty if
     *                  sync should use its existing auth token if available.
     */
    public void syncSignInWithAuthToken(String account, String authToken) {
        nativeSignInSync(mNativeProfileSyncServiceAndroid, account, authToken);
        // Notify listeners right away that the sync state has changed (native side does not do
        // this)
        syncStateChanged();
    }

    public void requestSyncFromNativeChrome(String objectId, long version, String payload) {
        ThreadUtils.assertOnUiThread();
        nativeNudgeSyncer(mNativeProfileSyncServiceAndroid, objectId, version, payload);
    }

    /**
     * Nudge the syncer to start a new sync cycle.
     */
    @VisibleForTesting
    public void requestSyncCycleForTest() {
        ThreadUtils.assertOnUiThread();
        requestSyncFromNativeChrome("", 0, "");
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
            Log.e(TAG, "Unable to get unique tag for sync. " +
                    "This may lead to unexpected tab sync behavior.");
            return;
        }
        String sessionTag = SESSION_TAG_PREFIX + uniqueTag;
        if (!nativeSetSyncSessionsId(mNativeProfileSyncServiceAndroid, sessionTag)) {
            Log.e(TAG, "Unable to write session sync tag. " +
                    "This may lead to unexpected tab sync behavior.");
        }
    }

    private Account getAccountOrNullFromUsername(String username) {
        if (username == null) {
            Log.e(TAG, "username is null");
            return null;
        }

        final AccountManagerHelper accountManagerHelper = AccountManagerHelper.get(mContext);
        final Account account = accountManagerHelper.getAccountFromName(username);
        if (account == null) {
            Log.e(TAG, "Account not found for provided username.");
            return null;
        }
        return account;
    }

    /**
     * Requests a new auth token from the AccountManager. Invalidates the old token
     * if |invalidAuthToken| is not empty.
     */
    @CalledByNative
    public void getNewAuthToken(final String username, final String invalidAuthToken) {
        final Account account = getAccountOrNullFromUsername(username);
        if (account == null) return;

        // Since this is blocking, do it in the background.
        new AsyncTask<Void, Void, String>() {

            @Override
            public String doInBackground(Void... params) {
                // Invalidate our old auth token and fetch a new one.
                AccountManagerHelper accountManagerHelper = AccountManagerHelper.get(mContext);
                return accountManagerHelper.getNewAuthToken(
                        account, invalidAuthToken, SyncStatusHelper.AUTH_TOKEN_TYPE_SYNC);
            }

            @Override
            public void onPostExecute(String authToken) {
                if (authToken == null) {
                    // TODO(sync): Need to hook LOGIN_ACCOUNTS_CHANGED_ACTION (http://b/5354713).
                    Log.d(TAG, "Auth token for sync was null.");
                } else {
                    Log.d(TAG, "Successfully retrieved sync auth token.");
                    nativeTokenAvailable(mNativeProfileSyncServiceAndroid, username, authToken);
                }
            }
        }.execute();
    }

   /**
    * Called by native to invalidate an OAuth2 token.
    */
    @CalledByNative
    public void invalidateOAuth2AuthToken(String scope, String accessToken) {
        AccountManagerHelper.get(mContext).invalidateAuthToken(scope, accessToken);
    }

    /**
     * Called by native to retrieve OAuth2 tokens.
     *
     * @param username the native username (full address)
     * @param scope the scope to get an auth token for (without Android-style 'oauth2:' prefix).
     * @param oldAuthToken if provided, the token will be invalidated before getting a new token.
     * @param nativeCallback the pointer to the native callback that should be run upon completion.
     */
    @CalledByNative
    public void getOAuth2AuthToken(String username, String scope, final int nativeCallback) {
        final Account account = getAccountOrNullFromUsername(username);
        if (account == null) {
            nativeOAuth2TokenFetched(
                mNativeProfileSyncServiceAndroid, nativeCallback, null, false);
            return;
        }
        final String oauth2Scope = "oauth2:" + scope;

        new AsyncTask<Void, Void, String>() {
            @Override
            public String doInBackground(Void... params) {
                AccountManagerHelper accountManagerHelper = AccountManagerHelper.get(mContext);
                return accountManagerHelper.getAuthTokenFromBackground(account, oauth2Scope);
            }

            @Override
            public void onPostExecute(String authToken) {
                nativeOAuth2TokenFetched(
                    mNativeProfileSyncServiceAndroid, nativeCallback, authToken, authToken != null);
            }
        }.execute();
    }

    /**
     * Checks if a password or a passphrase is required for decryption of sync data.
     * <p/>
     * Returns NONE if the state is unavailable, or decryption passphrase/password is not required.
     *
     * @return the enum describing the decryption passphrase type required
     */
    public SyncDecryptionPassphraseType getSyncDecryptionPassphraseTypeIfRequired() {
        // ProfileSyncService::IsUsingSecondaryPassphrase() requires the sync backend to be
        // initialized, and that happens just after OnPassphraseRequired(). Therefore, we need to
        // guard that call with a check of the sync backend since we can not be sure which
        // passphrase type we should tell the user we need.
        // This is tracked in:
        // http://code.google.com/p/chromium/issues/detail?id=108127
        if (isSyncInitialized() && isPassphraseRequiredForDecryption()) {
            return getSyncDecryptionPassphraseType();
        }
        return SyncDecryptionPassphraseType.NONE;
    }

    /**
     * Returns the actual passphrase type being used for encryption. The sync backend must be
     * running (isSyncInitialized() returns true) before calling this function.
     * <p/>
     * This method should only be used if you want to know the raw value. For checking whether we
     * should ask the user for a passphrase, you should instead use
     * getSyncDecryptionPassphraseTypeIfRequired().
     */
    public SyncDecryptionPassphraseType getSyncDecryptionPassphraseType() {
        assert isSyncInitialized();
        int passphraseType = nativeGetPassphraseType(mNativeProfileSyncServiceAndroid);
        return SyncDecryptionPassphraseType.fromInternalValue(passphraseType);
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

    public String getSyncEnterGooglePassphraseBodyWithDateText() {
        assert isSyncInitialized();
        return nativeGetSyncEnterGooglePassphraseBodyWithDateText(mNativeProfileSyncServiceAndroid);
    }

    public String getSyncEnterCustomPassphraseBodyWithDateText() {
        assert isSyncInitialized();
        return nativeGetSyncEnterCustomPassphraseBodyWithDateText(mNativeProfileSyncServiceAndroid);
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
     * Gets the set of data types that are currently enabled to sync.
     *
     * @return Set of enabled types.
     */
    public Set<ModelType> getPreferredDataTypes() {
        Set<ModelType> syncTypes = new HashSet<ModelType>();

        if (nativeIsAutofillSyncEnabled(mNativeProfileSyncServiceAndroid)) {
            syncTypes.add(ModelType.AUTOFILL);
        }
        if (nativeIsBookmarkSyncEnabled(mNativeProfileSyncServiceAndroid)) {
            syncTypes.add(ModelType.BOOKMARK);
        }
        if (nativeIsPasswordSyncEnabled(mNativeProfileSyncServiceAndroid)) {
            syncTypes.add(ModelType.PASSWORD);
        }
        if (nativeIsTypedUrlSyncEnabled(mNativeProfileSyncServiceAndroid)) {
            syncTypes.add(ModelType.TYPED_URL);
        }
        if (nativeIsSessionSyncEnabled(mNativeProfileSyncServiceAndroid)) {
            syncTypes.add(ModelType.SESSION);
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
        if (syncEverything || enabledTypes.contains(ModelType.SESSION)) {
            modelTypeSelection |= ModelTypeSelection.SESSION;
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

    // Native methods
    private native void nativeNudgeSyncer(
            int nativeProfileSyncServiceAndroid, String objectId, long version, String payload);
    private native int nativeInit();
    private native void nativeEnableSync(int nativeProfileSyncServiceAndroid);
    private native void nativeDisableSync(int nativeProfileSyncServiceAndroid);
    private native void nativeSignInSync(
            int nativeProfileSyncServiceAndroid, String username, String authToken);
    private native void nativeSignOutSync(int nativeProfileSyncServiceAndroid);
    private native void nativeTokenAvailable(
            int nativeProfileSyncServiceAndroid, String username, String authToken);
    private native boolean nativeSetSyncSessionsId(int nativeProfileSyncServiceAndroid, String tag);
    private native String nativeQuerySyncStatusSummary(int nativeProfileSyncServiceAndroid);
    private native int nativeGetAuthError(int nativeProfileSyncServiceAndroid);
    private native boolean nativeIsSyncInitialized(int nativeProfileSyncServiceAndroid);
    private native boolean nativeIsFirstSetupInProgress(int nativeProfileSyncServiceAndroid);
    private native boolean nativeIsEncryptEverythingEnabled(int nativeProfileSyncServiceAndroid);
    private native void nativeEnableEncryptEverything(int nativeProfileSyncServiceAndroid);
    private native boolean nativeIsPassphraseRequiredForDecryption(
            int nativeProfileSyncServiceAndroid);
    private native boolean nativeIsPassphraseRequiredForExternalType(
            int nativeProfileSyncServiceAndroid);
    private native boolean nativeIsUsingSecondaryPassphrase(int nativeProfileSyncServiceAndroid);
    private native boolean nativeSetDecryptionPassphrase(
            int nativeProfileSyncServiceAndroid, String passphrase);
    private native void nativeSetEncryptionPassphrase(
            int nativeProfileSyncServiceAndroid, String passphrase, boolean isGaia);
    private native boolean nativeIsCryptographerReady(int nativeProfileSyncServiceAndroid);
    private native int nativeGetPassphraseType(int nativeProfileSyncServiceAndroid);
    private native boolean nativeHasExplicitPassphraseTime(int nativeProfileSyncServiceAndroid);
    private native String nativeGetSyncEnterGooglePassphraseBodyWithDateText(
            int nativeProfileSyncServiceAndroid);
    private native String nativeGetSyncEnterCustomPassphraseBodyWithDateText(
            int nativeProfileSyncServiceAndroid);
    private native String nativeGetSyncEnterCustomPassphraseBodyText(
            int nativeProfileSyncServiceAndroid);
    private native boolean nativeIsSyncKeystoreMigrationDone(int nativeProfileSyncServiceAndroid);
    private native void nativeSetPreferredDataTypes(
            int nativeProfileSyncServiceAndroid, boolean syncEverything, long modelTypeSelection);
    private native void nativeSetSetupInProgress(
            int nativeProfileSyncServiceAndroid, boolean inProgress);
    private native void nativeSetSyncSetupCompleted(int nativeProfileSyncServiceAndroid);
    private native boolean nativeHasSyncSetupCompleted(int nativeProfileSyncServiceAndroid);
    private native boolean nativeHasKeepEverythingSynced(int nativeProfileSyncServiceAndroid);
    private native boolean nativeIsAutofillSyncEnabled(int nativeProfileSyncServiceAndroid);
    private native boolean nativeIsBookmarkSyncEnabled(int nativeProfileSyncServiceAndroid);
    private native boolean nativeIsPasswordSyncEnabled(int nativeProfileSyncServiceAndroid);
    private native boolean nativeIsTypedUrlSyncEnabled(int nativeProfileSyncServiceAndroid);
    private native boolean nativeIsSessionSyncEnabled(int nativeProfileSyncServiceAndroid);
    private native boolean nativeHasUnrecoverableError(int nativeProfileSyncServiceAndroid);
    private native String nativeGetAboutInfoForTest(int nativeProfileSyncServiceAndroid);
    private native void nativeOAuth2TokenFetched(
            int nativeProfileSyncServiceAndroid, int nativeCallback, String authToken,
            boolean result);
}
