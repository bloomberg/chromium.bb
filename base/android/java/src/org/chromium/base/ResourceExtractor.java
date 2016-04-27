// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.Trace;
import android.preference.PreferenceManager;

import org.chromium.base.annotations.SuppressFBWarnings;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CancellationException;
import java.util.concurrent.ExecutionException;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

/**
 * Handles extracting the necessary resources bundled in an APK and moving them to a location on
 * the file system accessible from the native code.
 */
public class ResourceExtractor {

    private static final String TAG = "cr.base";
    private static final String ICU_DATA_FILENAME = "icudtl.dat";
    private static final String V8_NATIVES_DATA_FILENAME = "natives_blob.bin";
    private static final String V8_SNAPSHOT_DATA_FILENAME = "snapshot_blob.bin";
    private static final String APP_VERSION_PREF = "org.chromium.base.ResourceExtractor.Version";

    private static ResourceEntry[] sResourcesToExtract = new ResourceEntry[0];

    /**
     * Holds information about a res/raw file (e.g. locale .pak files).
     */
    public static final class ResourceEntry {
        public final int resourceId;
        public final String pathWithinApk;
        public final String extractedFileName;

        public ResourceEntry(int resourceId, String pathWithinApk, String extractedFileName) {
            this.resourceId = resourceId;
            this.pathWithinApk = pathWithinApk;
            this.extractedFileName = extractedFileName;
        }
    }

    private class ExtractTask extends AsyncTask<Void, Void, Void> {
        private static final int BUFFER_SIZE = 16 * 1024;

        private final List<Runnable> mCompletionCallbacks = new ArrayList<Runnable>();

        private void extractResourceHelper(InputStream is, File outFile, byte[] buffer)
                throws IOException {
            OutputStream os = null;
            try {
                os = new FileOutputStream(outFile);
                Log.i(TAG, "Extracting resource %s", outFile);

                int count = 0;
                while ((count = is.read(buffer, 0, BUFFER_SIZE)) != -1) {
                    os.write(buffer, 0, count);
                }
            } finally {
                try {
                    if (os != null) {
                        os.close();
                    }
                } finally {
                    if (is != null) {
                        is.close();
                    }
                }
            }
        }

        private void doInBackgroundImpl() {
            final File outputDir = getOutputDir();
            if (!outputDir.exists() && !outputDir.mkdirs()) {
                Log.e(TAG, "Unable to create pak resources directory!");
                return;
            }

            beginTraceSection("checkPakTimeStamp");
            long curAppVersion = getApplicationVersion();
            SharedPreferences sharedPrefs = PreferenceManager.getDefaultSharedPreferences(
                    mContext.getApplicationContext());
            long prevAppVersion = sharedPrefs.getLong(APP_VERSION_PREF, 0);
            boolean versionChanged = curAppVersion != prevAppVersion;
            endTraceSection();

            if (versionChanged) {
                deleteFiles();
                // Use the version only to see if files should be deleted, not to skip extraction.
                // We've seen files be corrupted, so always attempt extraction.
                // http://crbug.com/606413
                sharedPrefs.edit().putLong(APP_VERSION_PREF, curAppVersion).apply();
            }

            beginTraceSection("WalkAssets");
            byte[] buffer = new byte[BUFFER_SIZE];
            try {
                for (ResourceEntry entry : sResourcesToExtract) {
                    File output = new File(outputDir, entry.extractedFileName);
                    // TODO(agrieve): It would be better to check that .length == expectedLength.
                    //     http://crbug.com/606413
                    if (output.length() != 0) {
                        continue;
                    }
                    beginTraceSection("ExtractResource");
                    InputStream inputStream = mContext.getResources().openRawResource(
                            entry.resourceId);
                    try {
                        extractResourceHelper(inputStream, output, buffer);
                    } finally {
                        endTraceSection(); // ExtractResource
                    }
                }
            } catch (IOException e) {
                // TODO(benm): See crbug/152413.
                // Try to recover here, can we try again after deleting files instead of
                // returning null? It might be useful to gather UMA here too to track if
                // this happens with regularity.
                Log.w(TAG, "Exception unpacking required pak resources: %s", e.getMessage());
                deleteFiles();
                return;
            } finally {
                endTraceSection(); // WalkAssets
            }
        }

        @Override
        protected Void doInBackground(Void... unused) {
            // TODO(lizeb): Use chrome tracing here (and above in
            // doInBackgroundImpl) when it will be possible. This is currently
            // not doable since the native library is not loaded yet, and the
            // TraceEvent calls are dropped before this point.
            beginTraceSection("ResourceExtractor.ExtractTask.doInBackground");
            try {
                doInBackgroundImpl();
            } finally {
                endTraceSection();
            }
            return null;
        }

        private void onPostExecuteImpl() {
            for (int i = 0; i < mCompletionCallbacks.size(); i++) {
                mCompletionCallbacks.get(i).run();
            }
            mCompletionCallbacks.clear();
        }

        @Override
        protected void onPostExecute(Void result) {
            beginTraceSection("ResourceExtractor.ExtractTask.onPostExecute");
            try {
                onPostExecuteImpl();
            } finally {
                endTraceSection();
            }
        }

        // Looks for a timestamp file on disk that indicates the version of the APK that
        // the resource paks were extracted from. Returns null if a timestamp was found
        // and it indicates that the resources match the current APK. Otherwise returns
        // a String that represents the filename of a timestamp to create.
        // Note that we do this to avoid adding a BroadcastReceiver on
        // android.content.Intent#ACTION_PACKAGE_CHANGED as that causes process churn
        // on (re)installation of *all* APK files.
        private long getApplicationVersion() {
            PackageManager pm = mContext.getPackageManager();
            try {
                PackageInfo pi = pm.getPackageInfo(mContext.getPackageName(), 0);
                if (pi != null && pi.versionCode > 1) {
                    return pi.versionCode;
                }
            } catch (PackageManager.NameNotFoundException e) {
                throw new RuntimeException(e);
            }
            // For dev builds, use the APK's CRC-32.
            try {
                ZipFile apk = new ZipFile(mContext.getApplicationInfo().sourceDir);
                ZipEntry manifestEntry = apk.getEntry("META-INF/MANIFEST.MF");
                apk.close();
                return manifestEntry.getCrc();
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        }

        @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
        private void beginTraceSection(String section) {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR2) return;
            Trace.beginSection(section);
        }

        @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
        private void endTraceSection() {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR2) return;
            Trace.endSection();
        }
    }

    private final Context mContext;
    private ExtractTask mExtractTask;

    private static ResourceExtractor sInstance;

    public static ResourceExtractor get(Context context) {
        if (sInstance == null) {
            sInstance = new ResourceExtractor(context);
        }
        return sInstance;
    }

    /**
     * Specifies the files that should be extracted from the APK.
     * and moved to {@link #getOutputDir()}.
     */
    @SuppressFBWarnings("EI_EXPOSE_STATIC_REP2")
    public static void setResourcesToExtract(ResourceEntry[] entries) {
        assert (sInstance == null || sInstance.mExtractTask == null)
                : "Must be called before startExtractingResources is called";
        sResourcesToExtract = entries;
    }

    private ResourceExtractor(Context context) {
        mContext = context.getApplicationContext();
    }

    /**
     * Synchronously wait for the resource extraction to be completed.
     * <p>
     * This method is bad and you should feel bad for using it.
     *
     * @see #addCompletionCallback(Runnable)
     */
    public void waitForCompletion() {
        if (shouldSkipPakExtraction()) {
            return;
        }

        assert mExtractTask != null;

        try {
            mExtractTask.get();
        } catch (CancellationException e) {
            // Don't leave the files in an inconsistent state.
            deleteFiles();
        } catch (ExecutionException e2) {
            deleteFiles();
        } catch (InterruptedException e3) {
            deleteFiles();
        }
    }

    /**
     * Adds a callback to be notified upon the completion of resource extraction.
     * <p>
     * If the resource task has already completed, the callback will be posted to the UI message
     * queue.  Otherwise, it will be executed after all the resources have been extracted.
     * <p>
     * This must be called on the UI thread.  The callback will also always be executed on
     * the UI thread.
     *
     * @param callback The callback to be enqueued.
     */
    public void addCompletionCallback(Runnable callback) {
        ThreadUtils.assertOnUiThread();

        Handler handler = new Handler(Looper.getMainLooper());
        if (shouldSkipPakExtraction()) {
            handler.post(callback);
            return;
        }

        assert mExtractTask != null;
        assert !mExtractTask.isCancelled();
        if (mExtractTask.getStatus() == AsyncTask.Status.FINISHED) {
            handler.post(callback);
        } else {
            mExtractTask.mCompletionCallbacks.add(callback);
        }
    }

    /**
     * This will extract the application pak resources in an
     * AsyncTask. Call waitForCompletion() at the point resources
     * are needed to block until the task completes.
     */
    public void startExtractingResources() {
        if (mExtractTask != null) {
            return;
        }

        // If a previous release extracted resources, and the current release does not,
        // deleteFiles() will not run and some files will be left. This currently
        // can happen for ContentShell, but not for Chrome proper, since we always extract
        // locale pak files.
        if (shouldSkipPakExtraction()) {
            return;
        }

        mExtractTask = new ExtractTask();
        mExtractTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    private File getAppDataDir() {
        return new File(PathUtils.getDataDirectory(mContext));
    }

    private File getOutputDir() {
        return new File(getAppDataDir(), "paks");
    }

    /**
     * Pak files (UI strings and other resources) should be updated along with
     * Chrome. A version mismatch can lead to a rather broken user experience.
     * Failing to update the V8 snapshot files will lead to a version mismatch
     * between V8 and the loaded snapshot which will cause V8 to crash, so this
     * is treated as an error. The ICU data (icudtl.dat) is less
     * version-sensitive, but still can lead to malfunction/UX misbehavior. So,
     * we regard failing to update them as an error.
     */
    private void deleteFiles() {
        File icudata = new File(getAppDataDir(), ICU_DATA_FILENAME);
        if (icudata.exists() && !icudata.delete()) {
            Log.e(TAG, "Unable to remove the icudata %s", icudata.getName());
        }
        File v8_natives = new File(getAppDataDir(), V8_NATIVES_DATA_FILENAME);
        if (v8_natives.exists() && !v8_natives.delete()) {
            Log.e(TAG, "Unable to remove the v8 data %s", v8_natives.getName());
        }
        File v8_snapshot = new File(getAppDataDir(), V8_SNAPSHOT_DATA_FILENAME);
        if (v8_snapshot.exists() && !v8_snapshot.delete()) {
            Log.e(TAG, "Unable to remove the v8 data %s", v8_snapshot.getName());
        }
        File dir = getOutputDir();
        if (dir.exists()) {
            File[] files = dir.listFiles();

            if (files != null) {
                for (File file : files) {
                    if (!file.delete()) {
                        Log.e(TAG, "Unable to remove existing resource %s", file.getName());
                    }
                }
            }
        }
    }

    /**
     * Pak extraction not necessarily required by the embedder.
     */
    private static boolean shouldSkipPakExtraction() {
        return sResourcesToExtract.length == 0;
    }
}
