// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.content;

import static java.lang.Math.min;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.support.annotation.Nullable;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.native_page.NativePage;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.display.DisplayAndroid;

import java.util.ArrayList;
import java.util.List;

/**
 * The TabContentManager is responsible for serving tab contents to the UI components. Contents
 * could be live or static thumbnails.
 */
@JNINamespace("android")
public class TabContentManager {
    private final float mThumbnailScale;
    private final int mFullResThumbnailsMaxSize;
    private final ContentOffsetProvider mContentOffsetProvider;
    private int[] mPriorityTabIds;
    private long mNativeTabContentManager;

    private final ArrayList<ThumbnailChangeListener> mListeners =
            new ArrayList<ThumbnailChangeListener>();

    private boolean mSnapshotsEnabled;

    /**
     * The Java interface for listening to thumbnail changes.
     */
    public interface ThumbnailChangeListener {
        /**
         * @param id The tab id.
         */
        public void onThumbnailChange(int id);
    }

    /**
     * @param context               The context that this cache is created in.
     * @param resourceId            The resource that this value might be defined in.
     * @param commandLineSwitch     The switch for which we would like to extract value from.
     * @return the value of an integer resource.  If the value is overridden on the command line
     * with the given switch, return the override instead.
     */
    private static int getIntegerResourceWithOverride(Context context, int resourceId,
            String commandLineSwitch) {
        int val = context.getResources().getInteger(resourceId);
        String switchCount = CommandLine.getInstance().getSwitchValue(commandLineSwitch);
        if (switchCount != null) {
            int count = Integer.parseInt(switchCount);
            val = count;
        }
        return val;
    }

    /**
     * @param context               The context that this cache is created in.
     * @param contentOffsetProvider The provider of content parameter.
     */
    public TabContentManager(Context context, ContentOffsetProvider contentOffsetProvider,
            boolean snapshotsEnabled) {
        mContentOffsetProvider = contentOffsetProvider;
        mSnapshotsEnabled = snapshotsEnabled;

        // Override the cache size on the command line with --thumbnails=100
        int defaultCacheSize = getIntegerResourceWithOverride(
                context, R.integer.default_thumbnail_cache_size, ChromeSwitches.THUMBNAILS);

        mFullResThumbnailsMaxSize = defaultCacheSize;

        int compressionQueueMaxSize =
                context.getResources().getInteger(R.integer.default_compression_queue_size);
        int writeQueueMaxSize =
                context.getResources().getInteger(R.integer.default_write_queue_size);

        // Override the cache size on the command line with
        // --approximation-thumbnails=100
        int approximationCacheSize = getIntegerResourceWithOverride(context,
                R.integer.default_approximation_thumbnail_cache_size,
                ChromeSwitches.APPROXIMATION_THUMBNAILS);

        float thumbnailScale = 1.f;
        boolean useApproximationThumbnails;
        DisplayAndroid display = DisplayAndroid.getNonMultiDisplay(context);
        float deviceDensity = display.getDipScale();
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)) {
            // Scale all tablets to MDPI.
            thumbnailScale = 1.f / deviceDensity;
            useApproximationThumbnails = false;
        } else {
            // For phones, reduce the amount of memory usage by capturing a lower-res thumbnail for
            // devices with resolution higher than HDPI (crbug.com/357740).
            if (deviceDensity > 1.5f) {
                thumbnailScale = 1.5f / deviceDensity;
            }
            useApproximationThumbnails = true;
        }
        mThumbnailScale = thumbnailScale;

        mPriorityTabIds = new int[mFullResThumbnailsMaxSize];

        mNativeTabContentManager = nativeInit(defaultCacheSize,
                approximationCacheSize, compressionQueueMaxSize, writeQueueMaxSize,
                useApproximationThumbnails);
    }

    /**
     * Destroy the native component.
     */
    public void destroy() {
        if (mNativeTabContentManager != 0) {
            nativeDestroy(mNativeTabContentManager);
            mNativeTabContentManager = 0;
        }
    }

    @CalledByNative
    private long getNativePtr() {
        return mNativeTabContentManager;
    }

    /**
     * Add a listener to thumbnail changes.
     * @param listener The listener of thumbnail change events.
     */
    public void addThumbnailChangeListener(ThumbnailChangeListener listener) {
        if (!mListeners.contains(listener)) {
            mListeners.add(listener);
        }
    }

    /**
     * Remove a listener to thumbnail changes.
     * @param listener The listener of thumbnail change events.
     */
    public void removeThumbnailChangeListener(ThumbnailChangeListener listener) {
        mListeners.remove(listener);
    }

    private Bitmap readbackNativePage(final Tab tab, float scale) {
        Bitmap bitmap = null;
        NativePage page = tab.getNativePage();
        if (page == null) {
            return bitmap;
        }

        View viewToDraw = page.getView();
        if (viewToDraw == null || viewToDraw.getWidth() == 0 || viewToDraw.getHeight() == 0) {
            return bitmap;
        }

        if (page instanceof InvalidationAwareThumbnailProvider) {
            if (!((InvalidationAwareThumbnailProvider) page).shouldCaptureThumbnail()) {
                return null;
            }
        }

        float overlayTranslateY = mContentOffsetProvider.getOverlayTranslateY();

        float leftMargin = 0.f;
        float topMargin = 0.f;
        if (viewToDraw.getLayoutParams() instanceof MarginLayoutParams) {
            MarginLayoutParams params = (MarginLayoutParams) viewToDraw.getLayoutParams();
            leftMargin = params.leftMargin;
            topMargin = params.topMargin;
        }

        try {
            bitmap = Bitmap.createBitmap(
                    (int) ((viewToDraw.getWidth() + leftMargin) * mThumbnailScale),
                    (int) ((viewToDraw.getHeight() + topMargin - overlayTranslateY)
                            * mThumbnailScale),
                    Bitmap.Config.ARGB_8888);
        } catch (OutOfMemoryError ex) {
            return null;
        }

        Canvas c = new Canvas(bitmap);
        c.scale(scale, scale);
        c.translate(leftMargin, -overlayTranslateY + topMargin);
        if (page instanceof InvalidationAwareThumbnailProvider) {
            ((InvalidationAwareThumbnailProvider) page).captureThumbnail(c);
        } else {
            viewToDraw.draw(c);
        }

        return bitmap;
    }

    /**
     * @param tabId The id of the {@link Tab} to check for a full sized thumbnail of.
     * @return      Whether or not there is a full sized cached thumbnail for the {@link Tab}
     *              identified by {@code tabId}.
     */
    public boolean hasFullCachedThumbnail(int tabId) {
        if (mNativeTabContentManager == 0) return false;
        return nativeHasFullCachedThumbnail(mNativeTabContentManager, tabId);
    }

    /**
     * Call to get a thumbnail for a given tab from disk through a {@link Callback}. If there is
     * no up-to-date thumbnail on disk for the given tab, callback returns null.
     * Currently this reads a compressed file from disk and sends the Bitmap over the
     * JNI boundary after decompressing. In its current form, should be used for experimental
     * purposes only.
     * TODO(yusufo): Change the plumbing so that at the least a {@link android.net.Uri} is sent
     * over JNI of an uncompressed file on disk.
     * @param tab The tab to get the thumbnail for.
     * @param callback The callback to send the {@link Bitmap} with.
     */
    public void getTabThumbnailWithCallback(
            Tab tab, Callback<Bitmap> callback, boolean forceUpdate) {
        if (mNativeTabContentManager == 0 || !mSnapshotsEnabled) return;

        if (forceUpdate) {
            cacheTabThumbnail(tab, (bitmap) -> {
                if (bitmap != null) {
                    callback.onResult(bitmap);
                    return;
                }
                // If invalidation is not needed, cacheTabThumbnail() might not do anything, so we
                // need to fall back to reading from disk.
                nativeGetTabThumbnailWithCallback(mNativeTabContentManager, tab.getId(), callback);
            });
        } else {
            nativeGetTabThumbnailWithCallback(mNativeTabContentManager, tab.getId(), callback);
        }
    }

    /**
     * Cache the content of a tab as a thumbnail.
     * @param tab The tab whose content we will cache.
     */
    public void cacheTabThumbnail(final Tab tab) {
        cacheTabThumbnail(tab, null);
    }

    /**
     * Cache the content of a tab as a thumbnail.
     * @param tab The tab whose content we will cache.
     * @param callback The callback to send the {@link Bitmap} with.
     */
    public void cacheTabThumbnail(final Tab tab, @Nullable Callback<Bitmap> callback) {
        if (mNativeTabContentManager != 0 && mSnapshotsEnabled) {
            if (tab.getNativePage() != null) {
                Bitmap nativePageBitmap = readbackNativePage(tab, mThumbnailScale);
                if (nativePageBitmap == null) {
                    if (callback != null) callback.onResult(null);
                    return;
                }
                nativeCacheTabWithBitmap(mNativeTabContentManager, tab, nativePageBitmap,
                        mThumbnailScale);
                if (callback != null) {
                    // In portrait mode, we want to show thumbnails in squares.
                    // Therefore, the thumbnail saved in portrait mode needs to be cropped to
                    // a square, or it would become too tall and break the layout.
                    Matrix matrix = new Matrix();
                    matrix.setScale(0.5f, 0.5f);
                    Bitmap resized =
                            Bitmap.createBitmap(nativePageBitmap, 0, 0, nativePageBitmap.getWidth(),
                                    min(nativePageBitmap.getWidth(), nativePageBitmap.getHeight()),
                                    matrix, true);
                    callback.onResult(resized);
                }
                nativePageBitmap.recycle();
            } else {
                if (tab.getWebContents() == null) return;
                nativeCacheTab(mNativeTabContentManager, tab, mThumbnailScale, callback);
            }
        }
    }

    /**
     * Invalidate a thumbnail if the content of the tab has been changed.
     * @param tabId The id of the {@link Tab} thumbnail to check.
     * @param url   The current URL of the {@link Tab}.
     */
    public void invalidateIfChanged(int tabId, String url) {
        if (mNativeTabContentManager != 0) {
            nativeInvalidateIfChanged(mNativeTabContentManager, tabId, url);
        }
    }

    /**
     * Invalidate a thumbnail of the tab whose id is |id|.
     * @param tabId The id of the {@link Tab} thumbnail to check.
     * @param url   The current URL of the {@link Tab}.
     */
    public void invalidateTabThumbnail(int id, String url) {
        invalidateIfChanged(id, url);
    }


    /**
     * Update the priority-ordered list of visible tabs.
     * @param priority The list of tab ids ordered in terms of priority.
     */
    public void updateVisibleIds(List<Integer> priority, int primaryTabId) {
        if (mNativeTabContentManager != 0) {
            int idsSize = min(mFullResThumbnailsMaxSize, priority.size());

            if (idsSize != mPriorityTabIds.length) {
                mPriorityTabIds = new int[idsSize];
            }

            for (int i = 0; i < idsSize; i++) {
                mPriorityTabIds[i] = priority.get(i);
            }
            nativeUpdateVisibleIds(mNativeTabContentManager, mPriorityTabIds, primaryTabId);
        }
    }


    /**
     * Removes a thumbnail of the tab whose id is |tabId|.
     * @param tabId The Id of the tab whose thumbnail is being removed.
     */
    public void removeTabThumbnail(int tabId) {
        if (mNativeTabContentManager != 0) {
            nativeRemoveTabThumbnail(mNativeTabContentManager, tabId);
        }
    }

    @CalledByNative
    protected void notifyListenersOfThumbnailChange(int tabId) {
        for (ThumbnailChangeListener listener : mListeners) {
            listener.onThumbnailChange(tabId);
        }
    }

    // Class Object Methods
    private native long nativeInit(int defaultCacheSize, int approximationCacheSize,
            int compressionQueueMaxSize, int writeQueueMaxSize, boolean useApproximationThumbnail);
    private native boolean nativeHasFullCachedThumbnail(long nativeTabContentManager, int tabId);
    private native void nativeCacheTab(long nativeTabContentManager, Object tab,
            float thumbnailScale, Callback<Bitmap> callback);
    private native void nativeCacheTabWithBitmap(long nativeTabContentManager, Object tab,
            Object bitmap, float thumbnailScale);
    private native void nativeInvalidateIfChanged(long nativeTabContentManager, int tabId,
            String url);
    private native void nativeUpdateVisibleIds(
            long nativeTabContentManager, int[] priority, int primaryTabId);
    private native void nativeRemoveTabThumbnail(long nativeTabContentManager, int tabId);
    private native void nativeGetTabThumbnailWithCallback(
            long nativeTabContentManager, int tabId, Callback<Bitmap> callback);
    private static native void nativeDestroy(long nativeTabContentManager);
}
