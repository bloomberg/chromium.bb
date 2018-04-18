// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Activity;
import android.content.ComponentName;
import android.os.Bundle;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.SnackbarActivity;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorNotificationBridgeUiFactory;
import org.chromium.chrome.browser.download.ui.DownloadFilter;
import org.chromium.chrome.browser.download.ui.DownloadManagerUi;
import org.chromium.chrome.browser.download.ui.DownloadManagerUi.DownloadUiObserver;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.ui.base.ActivityAndroidPermissionDelegate;
import org.chromium.ui.base.AndroidPermissionDelegate;

import java.lang.ref.WeakReference;
import java.util.Deque;
import java.util.LinkedList;

/**
 * Activity for managing downloads handled through Chrome.
 */
public class DownloadActivity extends SnackbarActivity {
    private DownloadManagerUi mDownloadManagerUi;
    private boolean mIsOffTheRecord;
    private AndroidPermissionDelegate mPermissionDelegate;

    /** Caches the stack of filters applied to let the user backtrack through their history. */
    private final Deque<String> mBackStack = new LinkedList<>();

    private final DownloadUiObserver mUiObserver = new DownloadUiObserver() {
        @Override
        public void onManagerDestroyed() { }

        @Override
        public void onFilterChanged(int filter) {
            String url = DownloadFilter.getUrlForFilter(filter);
            if (mBackStack.isEmpty() || !mBackStack.peek().equals(url)) {
                mBackStack.push(url);
            }
        }
    };

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // Loads offline pages and prefetch downloads.
        OfflineContentAggregatorNotificationBridgeUiFactory.instance();
        boolean isOffTheRecord = DownloadUtils.shouldShowOffTheRecordDownloads(getIntent());
        boolean showPrefetchContent = DownloadUtils.shouldShowPrefetchContent(getIntent());
        ComponentName parentComponent = IntentUtils.safeGetParcelableExtra(
                getIntent(), IntentHandler.EXTRA_PARENT_COMPONENT);
        mPermissionDelegate =
                new ActivityAndroidPermissionDelegate(new WeakReference<Activity>(this));
        mDownloadManagerUi = new DownloadManagerUi(
                this, isOffTheRecord, parentComponent, true, getSnackbarManager());
        setContentView(mDownloadManagerUi.getView());
        mIsOffTheRecord = isOffTheRecord;
        mDownloadManagerUi.addObserver(mUiObserver);
        // Call updateForUrl() to align with how DownloadPage interacts with DownloadManagerUi.
        mDownloadManagerUi.updateForUrl(UrlConstants.DOWNLOADS_URL);
        if (showPrefetchContent) mDownloadManagerUi.expandPrefetchSection();
    }

    @Override
    public void onResume() {
        super.onResume();
        DownloadUtils.checkForExternallyRemovedDownloads(mDownloadManagerUi.getBackendProvider(),
                mIsOffTheRecord);
    }

    @Override
    public void onBackPressed() {
        if (mDownloadManagerUi.onBackPressed()) return;
        // The top of the stack always represents the current filter. When back is pressed,
        // the top is popped off and the new top indicates what filter to use. If there are
        // no filters remaining, the Activity itself is closed.
        if (mBackStack.size() > 1) {
            mBackStack.pop();
            mDownloadManagerUi.updateForUrl(mBackStack.peek());
        } else {
            if (!mBackStack.isEmpty()) mBackStack.pop();
            super.onBackPressed();
        }
    }

    @Override
    protected void onDestroy() {
        mDownloadManagerUi.onDestroyed();
        super.onDestroy();
    }

    @VisibleForTesting
    DownloadManagerUi getDownloadManagerUiForTests() {
        return mDownloadManagerUi;
    }

    public AndroidPermissionDelegate getAndroidPermissionDelegate() {
        return mPermissionDelegate;
    }

    @Override
    public void onRequestPermissionsResult(
            int requestCode, String[] permissions, int[] grantResults) {
        mPermissionDelegate.onRequestPermissionsResult(requestCode, permissions, grantResults);
    }
}
