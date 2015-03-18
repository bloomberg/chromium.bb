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
import android.net.Uri;
import android.os.AsyncTask;
import android.preference.PreferenceManager;
import android.util.Log;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.ui.UiUtils;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Collections;
import java.util.List;

/**
 * A helper class that helps to start an intent to share titles and URLs.
 */
public class ShareHelper {

    private static final String TAG = "ShareHelper";

    private static final String PACKAGE_NAME_KEY = "last_shared_package_name";
    private static final String CLASS_NAME_KEY = "last_shared_class_name";

    /**
     * Directory name for screenshots.
     */
    private static final String SCREENSHOT_DIRECTORY_NAME = "screenshot";

    private ShareHelper() {}

    private static void deleteScreenshotFiles(File file) {
        if (!file.exists()) return;
        if (file.isDirectory()) {
            for (File f : file.listFiles()) deleteScreenshotFiles(f);
        }
        if (!file.delete()) {
            Log.w(TAG, "Failed to delete screenshot file: " + file.getAbsolutePath());
        }
    }

    /**
     * Clears all shared screenshot files.
     */
    public static void clearSharedScreenshots(final Context context) {
        new AsyncTask<Void, Void, Void>() {
            @Override
            protected Void doInBackground(Void... params) {
                try {
                    File imagePath = UiUtils.getDirectoryForImageCapture(context);
                    deleteScreenshotFiles(new File(imagePath, SCREENSHOT_DIRECTORY_NAME));
                } catch (IOException ie) {
                    // Ignore exception.
                }
                return null;
            }
        }.execute();
    }

    /**
     * Creates and shows a share intent picker dialog or starts a share intent directly with the
     * activity that was most recently used to share based on shareDirectly value.
     *
     * This function will save |screenshot| under {app's root}/files/images/screenshot (or
     * /sdcard/DCIM/browser-images/screenshot if ADK is lower than JB MR2).
     * Cleaning up doesn't happen automatically, and so an app should call clearSharedScreenshots()
     * explicitly when needed.
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
        Intent intent = getShareIntent(title, url, null);
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
                makeIntentAndShare(activity, title, url, screenshot, component);
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
        makeIntentAndShare(activity, title, url, screenshot, component);
    }

    private static void makeIntentAndShare(final Activity activity, final String title,
            final String url, final Bitmap screenshot, final ComponentName component) {
        if (screenshot == null) {
            activity.startActivity(getDirectShareIntentForComponent(title, url, null, component));
        } else {
            new AsyncTask<Void, Void, File>() {
                @Override
                protected File doInBackground(Void... params) {
                    FileOutputStream fOut = null;
                    try {
                        File path = new File(UiUtils.getDirectoryForImageCapture(activity),
                                SCREENSHOT_DIRECTORY_NAME);
                        if (path.exists() || path.mkdir()) {
                            File saveFile = File.createTempFile(
                                    String.valueOf(System.currentTimeMillis()), ".jpg", path);
                            fOut = new FileOutputStream(saveFile);
                            screenshot.compress(Bitmap.CompressFormat.JPEG, 85, fOut);
                            fOut.flush();
                            fOut.close();

                            return saveFile;
                        }
                    } catch (IOException ie) {
                        if (fOut != null) {
                            try {
                                fOut.close();
                            } catch (IOException e) {
                                // Ignore exception.
                            }
                        }
                    }

                    return null;
                }

                @Override
                protected void onPostExecute(File saveFile) {
                    if (ApplicationStatus.getStateForApplication()
                            != ApplicationState.HAS_DESTROYED_ACTIVITIES) {
                        Uri screenshotUri = saveFile == null
                                ? null : UiUtils.getUriForImageCaptureFile(activity, saveFile);
                        activity.startActivity(getDirectShareIntentForComponent(
                                title, url, screenshotUri, component));
                    }
                }
            }.execute();
        }
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

    @VisibleForTesting
    public static Intent getShareIntent(String title, String url, Uri screenshotUri) {
        url = DomDistillerUrlUtils.getOriginalUrlFromDistillerUrl(url);
        Intent intent = new Intent(Intent.ACTION_SEND);
        intent.addFlags(ApiCompatibilityUtils.getActivityNewDocumentFlag());
        intent.setType("text/plain");
        intent.putExtra(Intent.EXTRA_SUBJECT, title);
        intent.putExtra(Intent.EXTRA_TEXT, url);
        if (screenshotUri != null) {
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            intent.putExtra(Intent.EXTRA_STREAM, screenshotUri);
        }
        return intent;
    }

    private static Intent getDirectShareIntentForComponent(
            String title, String url, Uri screenshotUri, ComponentName component) {
        Intent intent = getShareIntent(title, url, screenshotUri);
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
