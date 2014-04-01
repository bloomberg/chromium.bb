// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.ResolveInfo;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.preference.PreferenceManager;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;

import org.chromium.chrome.R;

import java.util.Collections;
import java.util.List;

/**
 * A helper class that helps to start an intent to share titles and URLs.
 */
public class ShareHelper {

    private static final String PACKAGE_NAME_KEY = "last_shared_package_name";
    private static final String CLASS_NAME_KEY = "last_shared_class_name";

    /**
     * Intent extra for sharing screenshots via the Share intent.
     *
     * Copied from {@link android.provider.Browser} as it is marked as {@literal @hide}.
     */
    private static final String EXTRA_SHARE_SCREENSHOT = "share_screenshot";

    private ShareHelper() {}

    /**
     * Creates and shows a share intent picker dialog or starts a share intent directly with the
     * activity that was most recently used to share based on shareDirectly value.
     *
     * @param shareDirectly Whether it should share directly with the activity that was most
     *                      recently used to share.
     * @param activity Activity that is used to access package manager.
     * @param title Title of the page to be shared.
     * @param url URL of the page to be shared.
     * @param screenshot Screenshot of the page to be shared.
     */
    public static void share(boolean shareDirectly, Activity activity, String title, String url,
            Bitmap screenshot) {
        if (shareDirectly) {
            shareWithLastUsed(activity, title, url, screenshot);
        } else {
            showShareDialog(activity, title, url, screenshot);
        }
    }

    /**
     * Creates and shows a share intent picker dialog.
     *
     * @param activity Activity that is used to access package manager.
     * @param title Title of the page to be shared.
     * @param url URL of the page to be shared.
     * @param screenshot Screenshot of the page to be shared.
     */
    private static void showShareDialog(final Activity activity, final String title,
            final String url, final Bitmap screenshot) {
        Intent intent = getShareIntent(title, url, screenshot);
        PackageManager manager = activity.getPackageManager();
        List<ResolveInfo> resolveInfoList = manager.queryIntentActivities(intent, 0);
        assert resolveInfoList.size() > 0;
        if (resolveInfoList.size() == 0) return;
        Collections.sort(resolveInfoList, new ResolveInfo.DisplayNameComparator(manager));

        final ShareDialogAdapter adapter =
                new ShareDialogAdapter(activity, manager, resolveInfoList);
        AlertDialog.Builder builder = new AlertDialog.Builder(activity);
        builder.setTitle(activity.getString(R.string.share_link_chooser_title));
        builder.setAdapter(adapter, null);

        final AlertDialog dialog = builder.create();
        dialog.show();
        dialog.getListView().setOnItemClickListener(new OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                ResolveInfo info = adapter.getItem(position);
                ActivityInfo ai = info.activityInfo;
                ComponentName component =
                        new ComponentName(ai.applicationInfo.packageName, ai.name);
                setLastShareComponentName(activity, component);
                Intent intent = getDirectShareIntentForComponent(title, url, screenshot, component);
                activity.startActivity(intent);
                dialog.dismiss();
            }
        });
    }

    /**
     * Starts a share intent with the activity that was most recently used to share.
     * If there is no most recently used activity, it does nothing.
     * @param activity Activity that is used to start the share intent.
     * @param title Title of the page to be shared.
     * @param url URL of the page to be shared.
     * @param screenshot Screenshot of the page to be shared.
     */
    private static void shareWithLastUsed(
            Activity activity, String title, String url, Bitmap screenshot) {
        ComponentName component = getLastShareComponentName(activity);
        if (component == null) return;
        Intent intent = getDirectShareIntentForComponent(title, url, screenshot, component);
        activity.startActivity(intent);
    }

    /**
     * Set the icon and the title for the menu item used for direct share.
     *
     * @param activity Activity that is used to access the package manager.
     * @param item The menu item that is used for direct share
     */
    public static void configureDirectShareMenuItem(Activity activity, MenuItem item) {
        Drawable directShareIcon = null;
        CharSequence directShareTitle = null;

        ComponentName component = getLastShareComponentName(activity);
        if (component != null) {
            try {
                directShareIcon = activity.getPackageManager().getActivityIcon(component);
                ApplicationInfo ai = activity.getPackageManager().getApplicationInfo(
                        component.getPackageName(), 0);
                directShareTitle = activity.getPackageManager().getApplicationLabel(ai);
            } catch (NameNotFoundException exception) {
                // Use the default null values.
            }
        }

        item.setIcon(directShareIcon);
        if (directShareTitle != null) {
            item.setTitle(activity.getString(R.string.accessibility_menu_share_via,
                    directShareTitle));
        }
    }

    private static Intent getShareIntent(String title, String url, Bitmap screenshot) {
        Intent intent = new Intent(Intent.ACTION_SEND);
        intent.setType("text/plain");
        intent.putExtra(Intent.EXTRA_SUBJECT, title);
        intent.putExtra(Intent.EXTRA_TEXT, url);
        if (screenshot != null) intent.putExtra(EXTRA_SHARE_SCREENSHOT, screenshot);
        return intent;
    }

    private static Intent getDirectShareIntentForComponent(String title, String url,
            Bitmap screenshot, ComponentName component) {
        Intent intent = getShareIntent(title, url, screenshot);
        intent.addFlags(Intent.FLAG_ACTIVITY_FORWARD_RESULT
                | Intent.FLAG_ACTIVITY_PREVIOUS_IS_TOP);
        intent.setComponent(component);
        return intent;
    }

    private static ComponentName getLastShareComponentName(Context context) {
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
        String packageName = preferences.getString(PACKAGE_NAME_KEY, null);
        String className = preferences.getString(CLASS_NAME_KEY, null);
        if (packageName == null || className == null) return null;
        return new ComponentName(packageName, className);
    }

    private static void setLastShareComponentName(Context context, ComponentName component) {
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
        SharedPreferences.Editor editor = preferences.edit();
        editor.putString(PACKAGE_NAME_KEY, component.getPackageName());
        editor.putString(CLASS_NAME_KEY, component.getClassName());
        editor.apply();
    }
}
