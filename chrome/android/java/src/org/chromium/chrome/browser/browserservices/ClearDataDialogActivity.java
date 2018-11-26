// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v7.app.AppCompatActivity;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.website.SingleCategoryPreferences;
import org.chromium.chrome.browser.preferences.website.SiteSettingsCategory;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

/**
 * Activity that shows a dialog inviting the user to clear the browsing data for the origins
 * associated with Trusted Web Activity client app, which has just been uninstalled or had its data
 * cleared.
 */
public class ClearDataDialogActivity extends AppCompatActivity {
    private static final String EXTRA_DOMAINS = "org.chromium.chrome.extra.domains";
    private static final String EXTRA_ORIGINS = "org.chromium.chrome.extra.origins";

    /**
     * Creates an intent for launching this activity for the TWA client app that was uninstalled or
     * had its data cleared.
     */
    public static Intent createIntent(Context context, Collection<String> linkedDomains,
            Collection<String> linkedOrigins) {
        Intent intent = new Intent(context, ClearDataDialogActivity.class);
        intent.putExtra(EXTRA_DOMAINS, new ArrayList<>(linkedDomains));
        intent.putExtra(EXTRA_ORIGINS, new ArrayList<>(linkedOrigins));
        return intent;
    }

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        AlertDialog.Builder builder = new AlertDialog.Builder(this,
                android.R.style.Theme_DeviceDefault_Light_Dialog_Alert)
                .setTitle(R.string.twa_clear_data_dialog_title)
                .setMessage(R.string.twa_clear_data_dialog_message)
                .setPositiveButton(R.string.preferences, (ignored1, ignored2) -> {
                    openSettings();
                    finish();
                })
                .setNegativeButton(R.string.twa_clear_data_dialog_keep_data,
                        (ignored1, ignored2) -> finish());

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            builder.setOnDismissListener(ignored -> finish());
        }

        builder.create().show();
    }

    private void openSettings() {
        List<String> origins = getIntent().getStringArrayListExtra(EXTRA_ORIGINS);
        List<String> domains = getIntent().getStringArrayListExtra(EXTRA_DOMAINS);
        if (origins == null || origins.isEmpty() || domains == null || domains.isEmpty()) {
            assert false : "Invalid extras for ClearDataDialogActivity";
            return;
        }

        if (origins.size() == 1) {
            // When launched with EXTRA_SITE_ADDRESS, SingleWebsitePreferences will merge the
            // settings for top-level origin, so that given https://peconn.github.io and
            // peconn.github.io, we'll get the permission and data settings of both.
            openSingleWebsitePrefs(origins.get(0));
        } else {
            // Since there might be multiple entries per origin in the "All sites" screen,
            // such as https://peconn.github.io and peconn.github.io, we filter by domains
            // instead of origins.
            openFilteredAllSiteSettings(domains);
        }
    }

    private void openSingleWebsitePrefs(String origin) {
        startActivity(PreferencesLauncher.createIntentForSingleWebsitePreferences(this, origin));
    }

    private void openFilteredAllSiteSettings(Collection<String> domains) {
        Bundle extras = new Bundle();
        extras.putString(SingleCategoryPreferences.EXTRA_CATEGORY,
                SiteSettingsCategory.preferenceKey(SiteSettingsCategory.Type.ALL_SITES));
        extras.putString(SingleCategoryPreferences.EXTRA_TITLE,
                getString(R.string.twa_clear_data_site_selection_title));
        extras.putStringArrayList(
                SingleCategoryPreferences.EXTRA_SELECTED_DOMAINS, new ArrayList<>(domains));

        PreferencesLauncher.launchSettingsPage(this, SingleCategoryPreferences.class, extras);
    }
}
