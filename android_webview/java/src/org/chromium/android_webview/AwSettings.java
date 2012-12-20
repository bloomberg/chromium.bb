// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Process;
import android.webkit.WebSettings;

/**
 * Stores Android WebView specific settings that does not need to be synced to WebKit.
 * Use {@link org.chromium.content.browser.ContentSettings} for WebKit settings.
 *
 * Methods in this class can be called from any thread, including threads created by
 * the client of WebView.
 */
public class AwSettings {
    // Lock to protect all settings.
    private final Object mAwSettingsLock = new Object();

    private final Context mContext;
    private boolean mBlockNetworkLoads;  // Default depends on permission of embedding APK.
    private boolean mAllowContentUrlAccess = true;
    private boolean mAllowFileUrlAccess = true;
    private int mCacheMode = WebSettings.LOAD_DEFAULT;
    private boolean mShouldFocusFirstNode = true;

    public AwSettings(Context context) {
        mContext = context;
        mBlockNetworkLoads = mContext.checkPermission(
                android.Manifest.permission.INTERNET,
                Process.myPid(),
                Process.myUid()) != PackageManager.PERMISSION_GRANTED;
    }

    /**
     * See {@link android.webkit.WebSettings#setBlockNetworkLoads}.
     */
    public void setBlockNetworkLoads(boolean flag) {
        synchronized (mAwSettingsLock) {
            if (!flag && mContext.checkPermission(
                    android.Manifest.permission.INTERNET,
                    Process.myPid(),
                    Process.myUid()) != PackageManager.PERMISSION_GRANTED) {
                throw new SecurityException("Permission denied - " +
                        "application missing INTERNET permission");
            }
            mBlockNetworkLoads = flag;
        }
    }

    /**
     * See {@link android.webkit.WebSettings#getBlockNetworkLoads}.
     */
    public boolean getBlockNetworkLoads() {
        synchronized (mAwSettingsLock) {
            return mBlockNetworkLoads;
        }
    }

    /**
     * See {@link android.webkit.WebSettings#setAllowFileAccess}.
     */
    public void setAllowFileAccess(boolean allow) {
        synchronized (mAwSettingsLock) {
            if (mAllowFileUrlAccess != allow) {
                mAllowFileUrlAccess = allow;
            }
        }
    }

    /**
     * See {@link android.webkit.WebSettings#getAllowFileAccess}.
     */
    public boolean getAllowFileAccess() {
        synchronized (mAwSettingsLock) {
            return mAllowFileUrlAccess;
        }
    }

    /**
     * See {@link android.webkit.WebSettings#setAllowContentAccess}.
     */
    public void setAllowContentAccess(boolean allow) {
        synchronized (mAwSettingsLock) {
            if (mAllowContentUrlAccess != allow) {
                mAllowContentUrlAccess = allow;
            }
        }
    }

    /**
     * See {@link android.webkit.WebSettings#getAllowContentAccess}.
     */
    public boolean getAllowContentAccess() {
        synchronized (mAwSettingsLock) {
            return mAllowContentUrlAccess;
        }
    }

    /**
     * See {@link android.webkit.WebSettings#setCacheMode}.
     */
    public void setCacheMode(int mode) {
        synchronized (mAwSettingsLock) {
            if (mCacheMode != mode) {
                mCacheMode = mode;
            }
        }
    }

    /**
     * See {@link android.webkit.WebSettings#getCacheMode}.
     */
    public int getCacheMode() {
        synchronized (mAwSettingsLock) {
            return mCacheMode;
        }
    }

    /**
     * See {@link android.webkit.WebSettings#setNeedInitialFocus}.
     */
    public void setShouldFocusFirstNode(boolean flag) {
        synchronized (mAwSettingsLock) {
            mShouldFocusFirstNode = flag;
        }
    }

    /**
     * See {@link android.webkit.WebSettings#setNeedInitialFocus}.
     */
    public boolean shouldFocusFirstNode() {
        synchronized(mAwSettingsLock) {
            return mShouldFocusFirstNode;
        }
    }
}
