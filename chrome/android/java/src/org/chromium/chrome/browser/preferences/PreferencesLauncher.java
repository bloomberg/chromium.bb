// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.app.Activity;
import android.app.Fragment;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.preferences.autofill.AutofillPaymentMethodsFragment;
import org.chromium.chrome.browser.preferences.autofill.AutofillProfilesFragment;
import org.chromium.chrome.browser.preferences.website.SingleWebsitePreferences;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/**
 * A utility class for launching Chrome Settings.
 */
public class PreferencesLauncher {
    private static final String TAG = "PreferencesLauncher";

    /**
     * Launches settings, either on the top-level page or on a subpage.
     *
     * @param context The current Activity, or an application context if no Activity is available.
     * @param fragment The fragment to show, or null to show the top-level page.
     */
    public static void launchSettingsPage(
            Context context, @Nullable Class<? extends Fragment> fragment) {
        launchSettingsPage(context, fragment, null);
    }

    /**
     * Launches settings, either on the top-level page or on a subpage.
     *
     * @param context The current Activity, or an application context if no Activity is available.
     * @param fragment The name of the fragment to show, or null to show the top-level page.
     * @param fragmentArgs The arguments bundle to initialize the instance of subpage fragment.
     */
    public static void launchSettingsPage(Context context,
            @Nullable Class<? extends Fragment> fragment, @Nullable Bundle fragmentArgs) {
        String fragmentName = fragment != null ? fragment.getName() : null;
        Intent intent = createIntentForSettingsPage(context, fragmentName, fragmentArgs);
        IntentUtils.safeStartActivity(context, intent);
    }

    /**
     * Creates an intent for launching settings, either on the top-level settings page or a specific
     * subpage.
     *
     * @param context The current Activity, or an application context if no Activity is available.
     * @param fragmentName The name of the fragment to show, or null to show the top-level page.
     */
    public static Intent createIntentForSettingsPage(
            Context context, @Nullable String fragmentName) {
        return createIntentForSettingsPage(context, fragmentName, null);
    }

    /**
     * Creates an intent for launching settings, either on the top-level settings page or a specific
     * subpage.
     *
     * @param context The current Activity, or an application context if no Activity is available.
     * @param fragmentName The name of the fragment to show, or null to show the top-level page.
     * @param fragmentArgs The arguments bundle to initialize the instance of subpage fragment.
     */
    public static Intent createIntentForSettingsPage(
            Context context, @Nullable String fragmentName, @Nullable Bundle fragmentArgs) {
        Intent intent = new Intent();
        intent.setClass(context, Preferences.class);
        if (!(context instanceof Activity)) {
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        }
        if (fragmentName != null) {
            intent.putExtra(Preferences.EXTRA_SHOW_FRAGMENT, fragmentName);
        }
        if (fragmentArgs != null) {
            intent.putExtra(Preferences.EXTRA_SHOW_FRAGMENT_ARGUMENTS, fragmentArgs);
        }
        return intent;
    }

    /**
     * Creates an intent to launch single website preferences for the specified {@param url}.
     */
    public static Intent createIntentForSingleWebsitePreferences(Context context, String url) {
        return createIntentForSettingsPage(
                context, SingleWebsitePreferences.class.getName(),
                SingleWebsitePreferences.createFragmentArgsForSite(url));
    }

    @CalledByNative
    private static void showAutofillProfileSettings(WebContents webContents) {
        RecordUserAction.record("AutofillAddressesViewed");
        showSettingSubpage(webContents, AutofillProfilesFragment.class);
    }

    @CalledByNative
    private static void showAutofillCreditCardSettings(WebContents webContents) {
        RecordUserAction.record("AutofillCreditCardsViewed");
        showSettingSubpage(webContents, AutofillPaymentMethodsFragment.class);
    }

    @CalledByNative
    private static void showPasswordSettings(WebContents webContents) {
        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) return;
        WeakReference<Activity> currentActivity = window.getActivity();
        AppHooks.get().createManagePasswordsUIProvider().showManagePasswordsUI(
                currentActivity.get());
    }

    private static void showSettingSubpage(
            WebContents webContents, Class<? extends Fragment> fragment) {
        WeakReference<Activity> currentActivity =
                webContents.getTopLevelNativeWindow().getActivity();
        launchSettingsPage(currentActivity.get(), fragment);
    }
}
