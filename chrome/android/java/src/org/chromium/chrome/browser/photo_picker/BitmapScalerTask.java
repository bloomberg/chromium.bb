// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.photo_picker;

import android.graphics.Bitmap;
import android.os.AsyncTask;
import android.os.SystemClock;
import android.util.LruCache;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;

import java.util.concurrent.TimeUnit;

/**
 * A worker task to scale bitmaps in the background.
 */
class BitmapScalerTask extends AsyncTask<Bitmap, Void, Bitmap> {
    private final LruCache<String, Bitmap> mCache;
    private final String mFilePath;
    private final int mSize;

    /**
     * A BitmapScalerTask constructor.
     */
    public BitmapScalerTask(LruCache<String, Bitmap> cache, String filePath, int size) {
        mCache = cache;
        mFilePath = filePath;
        mSize = size;
    }

    /**
     * Scales the image provided. Called on a non-UI thread.
     * @param params Ignored, do not use.
     * @return A sorted list of images (by last-modified first).
     */
    @Override
    protected Bitmap doInBackground(Bitmap... bitmaps) {
        assert !ThreadUtils.runningOnUiThread();

        if (isCancelled()) return null;

        long begin = SystemClock.elapsedRealtime();
        Bitmap bitmap = BitmapUtils.scale(bitmaps[0], mSize, false);
        long scaleTime = SystemClock.elapsedRealtime() - begin;
        RecordHistogram.recordTimesHistogram(
                "Android.PhotoPicker.BitmapScalerTask", scaleTime, TimeUnit.MILLISECONDS);
        return bitmap;
    }

    /**
     * Communicates the results back to the client. Called on the UI thread.
     * @param result The resulting scaled bitmap.
     */
    @Override
    protected void onPostExecute(Bitmap result) {
        if (isCancelled()) {
            return;
        }

        mCache.put(mFilePath, result);
    }
}
