// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui;

import android.app.Activity;
import android.content.Context;
import android.os.AsyncTask;

import org.chromium.chrome.browser.preferences.privacy.ClearBrowsingDataPreferences;
import org.chromium.chrome.browser.provider.ChromeBrowserProviderClient;
import org.chromium.chrome.browser.signin.SigninManager;

import java.util.EnumSet;

/**
 * Modal dialog for clearing sync data. This allows the user to clear browsing data as well as
 * other synced data types like bookmarks.
 */
public class ClearSyncDataPreferences extends ClearBrowsingDataPreferences {
    private Context mApplicationContext;

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        mApplicationContext = context.getApplicationContext();
    }

    @Override
    @SuppressWarnings("deprecation")
    public void onAttach(Activity activity) {
        super.onAttach(activity);
        mApplicationContext = activity.getApplicationContext();
    }

    @Override
    protected DialogOption[] getDialogOptions() {
        return DialogOption.values();
    }

    @Override
    protected boolean isOptionSelectedByDefault(DialogOption option) {
        return true;
    }

    @Override
    protected void onOptionSelected() {
        if (mApplicationContext == null) return;

        showProgressDialog();

        // Bookmarks will be deleted locally, and not passed on to the native side.
        final EnumSet<DialogOption> selectedOptions = getSelectedOptions();
        final boolean shouldDeleteBookmarks =
                selectedOptions.contains(DialogOption.CLEAR_BOOKMARKS_DATA);
        selectedOptions.remove(DialogOption.CLEAR_BOOKMARKS_DATA);

        // Clear bookmarks first and then clear browsing data. Clear browsing data will remove
        // the progress dialog.
        // TODO(shashishekhar) We should not need an async task here, since bookmarks operations
        // are on UI thread. ChromeBrowserProvider enforces that call to Bookmark API is not on
        // the UI thread. http://crbug.com/225050
        new AsyncTask<Void, Void, Void>() {
            @Override
            protected Void doInBackground(Void... arg0) {
                if (shouldDeleteBookmarks) {
                    ChromeBrowserProviderClient.removeAllUserBookmarks(mApplicationContext);
                }
                return null;
            }

            @Override
            protected void onPostExecute(Void result) {
                clearBrowsingData(selectedOptions);

                if (shouldDeleteBookmarks) {
                    // onPostExecute is back in the UI thread.
                    SigninManager.get(mApplicationContext).clearLastSignedInUser();
                }
            }
        }.execute();
    }
}
