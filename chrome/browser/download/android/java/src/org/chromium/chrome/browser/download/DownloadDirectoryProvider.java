// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Build;
import android.os.Environment;
import android.text.TextUtils;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.download.DirectoryOption.DownloadLocationDirectoryType;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.io.File;
import java.util.ArrayList;

/**
 * Class to provide download related directory options including the default download directory on
 * the primary storage, or a private directory on external SD card.
 *
 * This class uses an asynchronous task to retrieve the directories, and guarantee only one task
 * can execute at any time. Multiple tasks may cause certain device fail to retrieve download
 * directories. Should be used on main thread.
 *
 * Also, this class listens to SD card insertion and removal events to update the directory
 * options accordingly.
 */
public class DownloadDirectoryProvider {
    private static final String TAG = "DownloadDirectory";

    /**
     * Delegate class to query directories from Android API. Should be created on main thread
     * and used on background thread in {@link AsyncTask}.
     */
    public interface Delegate {
        /**
         * Get the primary download directory in public external storage. The directory will be
         * created if it doesn't exist. Should be called on background thread.
         * @return The download directory. Can be an invalid directory if failed to create the
         *         directory.
         */
        File getPrimaryDownloadDirectory();

        /**
         * Get external files directories for {@link Environment#DIRECTORY_DOWNLOADS}.
         * @return A list of directories.
         */
        File[] getExternalFilesDirs();
    }

    /**
     * Class that calls Android API to get download directories.
     */
    public static class DownloadDirectoryProviderDelegate implements Delegate {
        @Override
        public File getPrimaryDownloadDirectory() {
            return DownloadDirectoryProvider.getPrimaryDownloadDirectory();
        }

        @Override
        public File[] getExternalFilesDirs() {
            return ContextUtils.getApplicationContext().getExternalFilesDirs(
                    Environment.DIRECTORY_DOWNLOADS);
        }
    }

    /**
     * Asynchronous task to retrieve all download directories on a background thread. Only one task
     * can exist at the same time.
     *
     * The logic to retrieve directories should match
     * {@link PathUtils#getAllPrivateDownloadsDirectories}.
     */
    private class AllDirectoriesTask extends AsyncTask<ArrayList<DirectoryOption>> {
        private DownloadDirectoryProvider.Delegate mDelegate;

        AllDirectoriesTask(DownloadDirectoryProvider.Delegate delegate) {
            mDelegate = delegate;
        }

        @Override
        protected ArrayList<DirectoryOption> doInBackground() {
            ArrayList<DirectoryOption> dirs = new ArrayList<>();

            // Retrieve default directory.
            File defaultDirectory = mDelegate.getPrimaryDownloadDirectory();

            // If no default directory, return an error option.
            if (defaultDirectory == null) {
                dirs.add(new DirectoryOption(
                        null, 0, 0, DirectoryOption.DownloadLocationDirectoryType.ERROR));
                return dirs;
            }

            DirectoryOption defaultOption = toDirectoryOption(
                    defaultDirectory, DirectoryOption.DownloadLocationDirectoryType.DEFAULT);
            dirs.add(defaultOption);
            recordDirectoryType(DirectoryOption.DownloadLocationDirectoryType.DEFAULT);

            // Retrieve additional directories, i.e. the external SD card directory.
            mExternalStorageDirectory = Environment.getExternalStorageDirectory().getAbsolutePath();
            File[] files;

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
                files = mDelegate.getExternalFilesDirs();
            } else {
                files = new File[] {Environment.getExternalStorageDirectory()};
            }

            if (files.length <= 1) return dirs;

            boolean hasAddtionalDirectory = false;
            for (int i = 0; i < files.length; ++i) {
                if (files[i] == null) continue;

                // Skip primary storage directory.
                if (files[i].getAbsolutePath().contains(mExternalStorageDirectory)) continue;
                dirs.add(toDirectoryOption(
                        files[i], DirectoryOption.DownloadLocationDirectoryType.ADDITIONAL));
                hasAddtionalDirectory = true;
            }

            if (hasAddtionalDirectory) {
                recordDirectoryType(DirectoryOption.DownloadLocationDirectoryType.ADDITIONAL);
            }

            return dirs;
        }

        @Override
        protected void onPostExecute(ArrayList<DirectoryOption> dirs) {
            mDirectoryOptions = dirs;
            mDirectoriesReady = true;
            mNeedsUpdate = false;

            for (Callback<ArrayList<DirectoryOption>> callback : mCallbacks) {
                callback.onResult(mDirectoryOptions);
            }

            mCallbacks.clear();
            mAllDirectoriesTask = null;
        }

        private DirectoryOption toDirectoryOption(
                File dir, @DownloadLocationDirectoryType int type) {
            if (dir == null) return null;
            return new DirectoryOption(
                    dir.getAbsolutePath(), dir.getUsableSpace(), dir.getTotalSpace(), type);
        }
    }

    // Singleton instance.
    private static class LazyHolder {
        private static DownloadDirectoryProvider sInstance = new DownloadDirectoryProvider();
    }

    /**
     * Get the instance of directory provider.
     * @return The singleton directory provider instance.
     */
    public static DownloadDirectoryProvider getInstance() {
        return LazyHolder.sInstance;
    }

    /**
     * Sets the directory provider for testing.
     * @param provider The directory provider used in tests.
     */
    public void setDirectoryProviderForTesting(DownloadDirectoryProvider provider) {
        LazyHolder.sInstance = provider;
    }

    /**
     * BroadcastReceiver to listen to external SD card insertion and removal events.
     */
    private final class ExternalSDCardReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (intent.getAction().equals(Intent.ACTION_MEDIA_REMOVED)
                    || intent.getAction().equals(Intent.ACTION_MEDIA_MOUNTED)
                    || intent.getAction().equals(Intent.ACTION_MEDIA_EJECT)) {
                // When receiving SD card events, immediately retrieve download directory may not
                // yield correct result, mark needs update to force to fire another
                // AllDirectoriesTask on next getAllDirectoriesOptions call.
                mNeedsUpdate = true;
            }
        }
    }

    private ExternalSDCardReceiver mExternalSDCardReceiver;
    private boolean mDirectoriesReady;
    private boolean mNeedsUpdate;
    private AllDirectoriesTask mAllDirectoriesTask;
    private ArrayList<DirectoryOption> mDirectoryOptions;
    private String mExternalStorageDirectory;
    private ArrayList<Callback<ArrayList<DirectoryOption>>> mCallbacks = new ArrayList<>();

    protected DownloadDirectoryProvider() {
        registerSDCardReceiver();
    }

    /**
     * Get all available download directories.
     * @param callback The callback that carries the result of all download directories.
     */
    public void getAllDirectoriesOptions(Callback<ArrayList<DirectoryOption>> callback) {
        // Use cache value.
        if (!mNeedsUpdate && mDirectoriesReady) {
            PostTask.postTask(
                    UiThreadTaskTraits.DEFAULT, () -> callback.onResult(mDirectoryOptions));
            return;
        }

        mCallbacks.add(callback);
        updateDirectories();
    }

    /**
     * Retrieves the external storage directory from in-memory cache. On Android M,
     * {@link Environment#getExternalStorageDirectory} may access disk, so this operation can't be
     * done on main thread.
     * @return The external storage path or null if the or null if the asynchronous task to query
     * the directories is not finished.
     */
    public String getExternalStorageDirectory() {
        if (mDirectoriesReady) return mExternalStorageDirectory;
        return null;
    }

    /**
     * Get the primary download directory in public external storage. The directory will be created
     * if it doesn't exist. Should be called on background thread.
     * @return The download directory. Can be an invalid directory if failed to create the
     *         directory.
     */
    public static File getPrimaryDownloadDirectory() {
        File downloadDir =
                Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);

        // Create the directory if needed.
        if (!downloadDir.exists()) {
            try {
                downloadDir.mkdirs();
            } catch (SecurityException e) {
                Log.e(TAG, "Exception when creating download directory.", e);
            }
        }
        return downloadDir;
    }

    /**
     * Returns whether the downloaded file path is on an external SD card.
     * @param filePath The download file path.
     */
    public static boolean isDownloadOnSDCard(String filePath) {
        File[] dirs = ContextUtils.getApplicationContext().getExternalFilesDirs(
                Environment.DIRECTORY_DOWNLOADS);
        if (dirs.length <= 1 || TextUtils.isEmpty(filePath)) return false;
        for (int i = 1; i < dirs.length; ++i) {
            if (dirs[i] == null) continue;
            if (filePath.startsWith(dirs[i].getAbsolutePath())) return true;
        }
        return false;
    }

    private void updateDirectories() {
        // If asynchronous task is pending, wait for its result.
        if (mAllDirectoriesTask != null) return;

        mAllDirectoriesTask = new AllDirectoriesTask(new DownloadDirectoryProviderDelegate());
        mAllDirectoriesTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    private void registerSDCardReceiver() {
        IntentFilter filter = new IntentFilter(Intent.ACTION_MEDIA_REMOVED);
        filter.addAction(Intent.ACTION_MEDIA_MOUNTED);
        filter.addAction(Intent.ACTION_MEDIA_EJECT);
        filter.addDataScheme("file");
        mExternalSDCardReceiver = new ExternalSDCardReceiver();
        ContextUtils.getApplicationContext().registerReceiver(mExternalSDCardReceiver, filter);
    }

    private void recordDirectoryType(@DirectoryOption.DownloadLocationDirectoryType int type) {
        RecordHistogram.recordEnumeratedHistogram("MobileDownload.Location.DirectoryType", type,
                DirectoryOption.DownloadLocationDirectoryType.NUM_ENTRIES);
    }
}
