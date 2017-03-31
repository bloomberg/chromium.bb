// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.util.Pair;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.View;
import android.view.View.OnCreateContextMenuListener;

import org.chromium.base.Callback;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.OnCloseContextMenuListener;

import java.util.List;

/**
 * A helper class that handles generating context menus for {@link ContentViewCore}s.
 */
public class ContextMenuHelper implements OnCreateContextMenuListener {
    private long mNativeContextMenuHelper;

    private ContextMenuPopulator mPopulator;
    private ContextMenuParams mCurrentContextMenuParams;
    private Context mContext;
    private Callback<Integer> mCallback;
    private Runnable mOnMenuShown;
    private Runnable mOnMenuClosed;
    private Callback<Bitmap> mOnThumbnailReceivedCallback;

    private ContextMenuHelper(long nativeContextMenuHelper) {
        mNativeContextMenuHelper = nativeContextMenuHelper;
    }

    @CalledByNative
    private static ContextMenuHelper create(long nativeContextMenuHelper) {
        return new ContextMenuHelper(nativeContextMenuHelper);
    }

    @CalledByNative
    private void destroy() {
        if (mPopulator != null) mPopulator.onDestroy();
        mNativeContextMenuHelper = 0;
    }

    /**
     * @param populator A {@link ContextMenuPopulator} that is responsible for managing and showing
     *                  context menus.
     */
    @CalledByNative
    private void setPopulator(ContextMenuPopulator populator) {
        if (mPopulator != null) mPopulator.onDestroy();
        mPopulator = populator;
    }

    /**
     * Starts showing a context menu for {@code view} based on {@code params}.
     * @param contentViewCore The {@link ContentViewCore} to show the menu to.
     * @param params          The {@link ContextMenuParams} that indicate what menu items to show.
     */
    @CalledByNative
    private void showContextMenu(final ContentViewCore contentViewCore, ContextMenuParams params) {
        if (params.isFile()) return;
        View view = contentViewCore.getContainerView();
        final WindowAndroid windowAndroid = contentViewCore.getWindowAndroid();

        if (view == null || view.getVisibility() != View.VISIBLE || view.getParent() == null
                || windowAndroid == null || windowAndroid.getContext().get() == null
                || mPopulator == null) {
            return;
        }

        mCurrentContextMenuParams = params;
        mContext = windowAndroid.getContext().get();
        mCallback = new Callback<Integer>() {
            @Override
            public void onResult(Integer result) {
                mPopulator.onItemSelected(
                        ContextMenuHelper.this, mCurrentContextMenuParams, result);
            }
        };
        mOnMenuShown = new Runnable() {
            @Override
            public void run() {
                WebContents webContents = contentViewCore.getWebContents();
                RecordHistogram.recordBooleanHistogram("ContextMenu.Shown", webContents != null);
            }
        };
        mOnMenuClosed = new Runnable() {
            @Override
            public void run() {
                if (mNativeContextMenuHelper == 0) return;
                nativeOnContextMenuClosed(mNativeContextMenuHelper);
            }
        };

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CUSTOM_CONTEXT_MENU)) {
            List<Pair<Integer, List<ContextMenuItem>>> items =
                    mPopulator.buildContextMenu(null, mContext, mCurrentContextMenuParams);

            final ContextMenuUi menuUi = new TabularContextMenuUi();
            menuUi.displayMenu(mContext, mCurrentContextMenuParams, items, mCallback, mOnMenuShown,
                    mOnMenuClosed);
            if (mCurrentContextMenuParams.isImage()) {
                getThumbnail(new Callback<Bitmap>() {
                    @Override
                    public void onResult(Bitmap result) {
                        ((TabularContextMenuUi) menuUi).onImageThumbnailRetrieved(result);
                    }
                });
            }
            return;
        }

        // The Platform Context Menu requires the listener within this hepler since this helper and
        // provides context menu for us to show.
        view.setOnCreateContextMenuListener(this);
        if (view.showContextMenu()) {
            mOnMenuShown.run();
            windowAndroid.addContextMenuCloseListener(new OnCloseContextMenuListener() {
                @Override
                public void onContextMenuClosed() {
                    mOnMenuClosed.run();
                    windowAndroid.removeContextMenuCloseListener(this);
                }
            });
        }
    }

    /**
     * Starts a download based on the current {@link ContextMenuParams}.
     * @param isLink Whether or not the download target is a link.
     */
    public void startContextMenuDownload(boolean isLink, boolean isDataReductionProxyEnabled) {
        if (mNativeContextMenuHelper != 0) {
            nativeOnStartDownload(mNativeContextMenuHelper, isLink, isDataReductionProxyEnabled);
        }
    }

    /**
     * Trigger an image search for the current image that triggered the context menu.
     */
    public void searchForImage() {
        if (mNativeContextMenuHelper == 0) return;
        nativeSearchForImage(mNativeContextMenuHelper);
    }

    /**
     * Share the image that triggered the current context menu.
     */
    public void shareImage() {
        if (mNativeContextMenuHelper == 0) return;
        nativeShareImage(mNativeContextMenuHelper);
    }

    @CalledByNative
    private void onShareImageReceived(
            WindowAndroid windowAndroid, byte[] jpegImageData) {
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) return;

        ShareHelper.shareImage(activity, jpegImageData);
    }

    /**
     * Gets the thumbnail of the current image that triggered the context menu.
     * @param callback Called once the the thumbnail is received.
     */
    private void getThumbnail(Callback<Bitmap> callback) {
        mOnThumbnailReceivedCallback = callback;
        if (mNativeContextMenuHelper == 0) return;
        int maxSizePx = mContext.getResources().getDimensionPixelSize(
                R.dimen.context_menu_header_image_max_size);
        nativeRetrieveHeaderThumbnail(mNativeContextMenuHelper, maxSizePx);
    }

    @CalledByNative
    private void onHeaderThumbnailReceived(WindowAndroid windowAndroid, byte[] jpegImageData) {
        // TODO(tedchoc): Decode in separate process before launch.
        Bitmap bitmap = BitmapFactory.decodeByteArray(jpegImageData, 0, jpegImageData.length);
        if (mOnThumbnailReceivedCallback != null) {
            mOnThumbnailReceivedCallback.onResult(bitmap);
        }
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View v, ContextMenuInfo menuInfo) {
        List<Pair<Integer, List<ContextMenuItem>>> items =
                mPopulator.buildContextMenu(menu, v.getContext(), mCurrentContextMenuParams);
        ContextMenuUi menuUi = new PlatformContextMenuUi(menu);
        menuUi.displayMenu(
                mContext, mCurrentContextMenuParams, items, mCallback, mOnMenuShown, mOnMenuClosed);
    }

    /**
     * @return The {@link ContextMenuPopulator} responsible for populating the context menu.
     */
    @VisibleForTesting
    public ContextMenuPopulator getPopulator() {
        return mPopulator;
    }

    private native void nativeOnStartDownload(
            long nativeContextMenuHelper, boolean isLink, boolean isDataReductionProxyEnabled);
    private native void nativeSearchForImage(long nativeContextMenuHelper);
    private native void nativeShareImage(long nativeContextMenuHelper);
    private native void nativeOnContextMenuClosed(long nativeContextMenuHelper);
    private native void nativeRetrieveHeaderThumbnail(long nativeContextMenuHelper, int maxSizePx);
}
