// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk.h2o;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.ParcelFileDescriptor;

import org.chromium.webapk.lib.common.WebApkCommonUtils;
import org.chromium.webapk.shell_apk.WebApkSharedPreferences;

import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.OutputStream;
import java.util.concurrent.atomic.AtomicReference;

/** ContentProvider for screenshot of splash screen. */
public class SplashContentProvider
        extends ContentProvider implements ContentProvider.PipeDataWriter<Void> {
    /** Holds value which gets cleared after {@link ExpiringData#CLEAR_DATA_INTERVAL_MS}.  */
    private static class ExpiringData {
        /** Time in milliseconds after constructing the object to clear the cached data. */
        private static final int CLEAR_CACHED_DATA_INTERVAL_MS = 10000;

        private byte[] mCachedData;
        private Handler mHandler;

        public ExpiringData(byte[] cachedData, Runnable expiryTask) {
            mCachedData = cachedData;
            mHandler = new Handler();
            mHandler.postDelayed(expiryTask, CLEAR_CACHED_DATA_INTERVAL_MS);
        }

        public byte[] getCachedData() {
            return mCachedData;
        }

        public void removeCallbacks() {
            mHandler.removeCallbacksAndMessages(null);
        }
    }

    /**
     * Maximum size in bytes of screenshot to transfer to browser. The screenshot should be
     * downsampled to fit. Capping the maximum size of the screenshot decreases bitmap encoding
     * time and image transfer time.
     */
    public static final int MAX_TRANSFER_SIZE_BYTES = 1024 * 1024 * 4;

    /** Mime type of transferred screenshot. */
    private static final String SPLASH_MIME_TYPE = "image/png";

    private static AtomicReference<ExpiringData> sCachedSplashPngBytes = new AtomicReference<>();

    /** The URI handled by this content provider. */
    private String mContentProviderUri;

    /**
     * Temporarily caches the passed-in splash screen screenshot. To preserve memory, the cached
     * data is cleared after a delay.
     */
    public static void cache(
            Context context, byte[] splashPngBytes, int splashWidth, int splashHeight) {
        SharedPreferences.Editor editor = WebApkSharedPreferences.getPrefs(context).edit();
        editor.putInt(WebApkSharedPreferences.PREF_SPLASH_WIDTH, splashWidth);
        editor.putInt(WebApkSharedPreferences.PREF_SPLASH_HEIGHT, splashHeight);
        editor.apply();

        getAndSetCachedData(splashPngBytes);
    }

    public static void clearCache() {
        getAndSetCachedData(null);
    }

    /**
     * Sets the cached splash screen screenshot and returns the old one.
     * Thread safety: Can be called from any thread.
     */
    private static byte[] getAndSetCachedData(byte[] newSplashPngBytes) {
        ExpiringData newData = null;
        if (newSplashPngBytes != null) {
            newData = new ExpiringData(newSplashPngBytes, SplashContentProvider::clearCache);
        }
        ExpiringData oldCachedData = sCachedSplashPngBytes.getAndSet(newData);
        if (oldCachedData == null) return null;

        oldCachedData.removeCallbacks();
        return oldCachedData.getCachedData();
    }

    @Override
    public boolean onCreate() {
        mContentProviderUri =
                WebApkCommonUtils.generateSplashContentProviderUri(getContext().getPackageName());
        return true;
    }

    @Override
    public ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException {
        if (uri != null && uri.toString().equals(mContentProviderUri)) {
            return openPipeHelper(null, null, null, null, this);
        }
        return null;
    }

    @Override
    public void writeDataToPipe(
            ParcelFileDescriptor output, Uri uri, String mimeType, Bundle opts, Void unused) {
        try (OutputStream out = new FileOutputStream(output.getFileDescriptor())) {
            byte[] cachedSplashPngBytes = getAndSetCachedData(null);
            if (cachedSplashPngBytes != null) {
                out.write(cachedSplashPngBytes);
            } else {
                // One way that this case gets hit is when the WebAPK is brought to the foreground
                // via Android Recents after the Android OOM killer has killed the host browser but
                // not SplashActivity.
                Bitmap splashScreenshot = recreateAndScreenshotSplash();
                if (splashScreenshot != null) {
                    splashScreenshot.compress(Bitmap.CompressFormat.PNG, 100, out);
                }
            }
            out.flush();
        } catch (Exception e) {
        }
    }

    @Override
    public String getType(Uri uri) {
        if (uri != null && uri.toString().equals(mContentProviderUri)) {
            return SPLASH_MIME_TYPE;
        }
        return null;
    }

    @Override
    public int update(Uri uri, ContentValues values, String where, String[] whereArgs) {
        throw new UnsupportedOperationException();
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        throw new UnsupportedOperationException();
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        throw new UnsupportedOperationException();
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs,
            String sortOrder) {
        throw new UnsupportedOperationException();
    }

    /** Builds splashscreen at size that it was last displayed and screenshots it. */
    private Bitmap recreateAndScreenshotSplash() {
        Context context = getContext().getApplicationContext();
        SharedPreferences prefs = WebApkSharedPreferences.getPrefs(context);
        int splashWidth = prefs.getInt(WebApkSharedPreferences.PREF_SPLASH_WIDTH, -1);
        int splashHeight = prefs.getInt(WebApkSharedPreferences.PREF_SPLASH_HEIGHT, -1);
        return SplashUtils.createAndImmediatelyScreenshotSplashView(
                context, splashWidth, splashHeight, MAX_TRANSFER_SIZE_BYTES);
    }
}
