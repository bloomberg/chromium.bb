// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Process;
import android.webkit.WebSettings;

import org.chromium.base.JNINamespace;
import org.chromium.base.ThreadUtils;

/**
 * Stores Android WebView specific settings that does not need to be synced to WebKit.
 * Use {@link org.chromium.content.browser.ContentSettings} for WebKit settings.
 *
 * Methods in this class can be called from any thread, including threads created by
 * the client of WebView.
 */
@JNINamespace("android_webview")
public class AwSettings {
    // Lock to protect all settings.
    private final Object mAwSettingsLock = new Object();

    private final Context mContext;
    private boolean mBlockNetworkLoads;  // Default depends on permission of embedding APK.
    private boolean mAllowContentUrlAccess = true;
    private boolean mAllowFileUrlAccess = true;
    private int mCacheMode = WebSettings.LOAD_DEFAULT;
    private boolean mShouldFocusFirstNode = true;
    private boolean mGeolocationEnabled = true;

    // The native side of this object.
    private int mNativeAwSettings = 0;

    public AwSettings(Context context, int nativeWebContents) {
        mContext = context;
        mBlockNetworkLoads = mContext.checkPermission(
                android.Manifest.permission.INTERNET,
                Process.myPid(),
                Process.myUid()) != PackageManager.PERMISSION_GRANTED;
        mNativeAwSettings = nativeInit(nativeWebContents);
        assert mNativeAwSettings != 0;
    }

    public void destroy() {
        nativeDestroy(mNativeAwSettings);
        mNativeAwSettings = 0;
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
     * Set whether fixed layout mode is enabled. Must be updated together
     * with ContentSettings.UseWideViewport, which maps on WebSettings.viewport_enabled.
     */
    public void setEnableFixedLayoutMode(final boolean enable) {
        // There is no need to lock, because the native code doesn't
        // read anything from the Java side.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                nativeSetEnableFixedLayoutMode(mNativeAwSettings, enable);
            }
        });
    }

    /**
     * Sets the initial scale for this WebView. The default value
     * is -1. A non-default value overrides initial scale set by
     * the meta viewport tag.
     */
    public void setInitialPageScale(final float scaleInPercent) {
        // There is no need to lock, because the native code doesn't
        // read anything from the Java side.
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                nativeSetInitialPageScale(mNativeAwSettings, scaleInPercent);
            }
        });
    }

    /**
     * Sets the text zoom of the page in percent. This kind of zooming is
     * only applicable when Text Autosizing is turned off. Passing -1 will
     * reset the zoom to the default value.
     */
    public void setTextZoom(final int textZoom) {
        // There is no need to lock, because the native code doesn't
        // read anything from the Java side.
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                nativeSetTextZoom(mNativeAwSettings, textZoom);
            }
        });
    }

    public void resetScrollAndScaleState() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                nativeResetScrollAndScaleState(mNativeAwSettings);
            }
        });
    }

    /**
     * See {@link android.webkit.WebSettings#setNeedInitialFocus}.
     */
    public boolean shouldFocusFirstNode() {
        synchronized(mAwSettingsLock) {
            return mShouldFocusFirstNode;
        }
    }

    /**
     * See {@link android.webkit.WebSettings#setGeolocationEnabled}.
     */
    public void setGeolocationEnabled(boolean flag) {
        synchronized (mAwSettingsLock) {
            if (mGeolocationEnabled != flag) {
                mGeolocationEnabled = flag;
            }
        }
    }

    /**
     * @return Returns if geolocation is currently enabled.
     */
    boolean getGeolocationEnabled() {
        synchronized (mAwSettingsLock) {
            return mGeolocationEnabled;
        }
    }

    public void setWebContents(int nativeWebContents) {
        nativeSetWebContents(mNativeAwSettings, nativeWebContents);
    }

    private native int nativeInit(int webContentsPtr);

    private native void nativeDestroy(int nativeAwSettings);

    private native void nativeResetScrollAndScaleState(int nativeAwSettings);

    private native void nativeSetWebContents(int nativeAwSettings, int nativeWebContents);

    private native void nativeSetEnableFixedLayoutMode(int nativeAwSettings, boolean enable);

    private native void nativeSetInitialPageScale(int nativeAwSettings, float scaleInPercent);

    private native void nativeSetTextZoom(int nativeAwSettings, int textZoom);
}
