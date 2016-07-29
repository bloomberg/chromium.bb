// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;

/**
 * This class interacts with C++ to detect whether resources in web manifest of a WebAPK have been
 * updated.
 */
public class ManifestUpgradeDetector extends EmptyTabObserver {
    /**
     * The names of <meta-data> in the WebAPK's AndroidManifest.xml whose values are needed to
     * determine whether a WebAPK needs to be upgraded but which are not present in
     * {@link WebappInfo}. The names must stay in sync with
     * {@linkorg.chromium.webapk.lib.runtime_library.HostBrowserLauncher}.
     */
    private static final String META_DATA_START_URL = "startUrl";

    /** Pointer to the native side ManifestUpgradeDetector. */
    private long mNativePointer;

    /** The tab that is being observed. */
    private final Tab mTab;

    private WebappInfo mWebappInfo;

    private String mStartUrl;

    /**
     * A flag for whether metadata is set via {@link set*ForTesting()}, rather than set
     * from WebAPK's AndroidManifest.xml.
     */
    private boolean mOverrideMetadataForTesting = false;

    private static final String TAG = "cr_UpgradeDetector";

    public ManifestUpgradeDetector(Tab tab, WebappInfo info) {
        mTab = tab;
        mWebappInfo = info;
    }

    @VisibleForTesting
    public void setOverrideMetadataForTesting(boolean overrideMetadataForTesting) {
        mOverrideMetadataForTesting = overrideMetadataForTesting;
    }

    @VisibleForTesting
    public void setStartUrlForTesting(String startUrl) {
        mStartUrl = startUrl;
    }

    /**
     * Starts fetching the web manifest resources.
     */
    public void start() {
        if (mWebappInfo.webManifestUri() == null
                || mWebappInfo.webManifestUri().equals(Uri.EMPTY)) {
            return;
        }

        if (mNativePointer != 0) return;

        getMetaDataFromAndroidManifest();
        mNativePointer = nativeInitialize(mTab.getWebContents(),
                mWebappInfo.scopeUri().toString(), mWebappInfo.webManifestUri().toString());

        mTab.addObserver(this);
        nativeStart(mNativePointer);
    }

    private void getMetaDataFromAndroidManifest() {
        if (mOverrideMetadataForTesting) {
            // The metadata are set via {@link set*ForTesting()}.
            return;
        }
        try {
            ApplicationInfo appinfo =
                    ContextUtils.getApplicationContext().getPackageManager().getApplicationInfo(
                            mWebappInfo.webApkPackageName(), PackageManager.GET_META_DATA);
            mStartUrl = appinfo.metaData.getString(META_DATA_START_URL);
        } catch (PackageManager.NameNotFoundException e) {
            e.printStackTrace();
        }
    }

    /**
     * Puts the object in a state where it is safe to be destroyed.
     */
    public void destroy() {
        if (mNativePointer != 0) {
            nativeDestroy(mNativePointer);
        }
        mNativePointer = 0;
    }

    @Override
    public void onWebContentsSwapped(Tab tab, boolean didStartLoad,
            boolean didFinishLoad) {
        updatePointers();
    }

    @Override
    public void onContentChanged(Tab tab) {
        updatePointers();
    }

    /**
     * Updates which WebContents the native ManifestUpgradeDetector is monitoring.
     */
    private void updatePointers() {
        nativeReplaceWebContents(mNativePointer, mTab.getWebContents());
    }

    /**
     * Called when the updated Web Manifest has been fetched.
     */
    @CalledByNative
    private void onDataAvailable(String startUrl, String scope, String name, String shortName,
            int displayMode, int orientation, long themeColor, long backgroundColor) {
        mTab.removeObserver(this);
        destroy();

        // TODO(hanxi): crbug.com/627824. Validate whether the new WebappInfo is
        // WebAPK-compatible.
        final WebappInfo newInfo = WebappInfo.create(mWebappInfo.id(), startUrl,
                scope, mWebappInfo.encodedIcon(), name, shortName, displayMode, orientation,
                mWebappInfo.source(), themeColor, backgroundColor, mWebappInfo.isIconGenerated(),
                mWebappInfo.webApkPackageName(), mWebappInfo.webManifestUri().toString());
        if (requireUpgrade(newInfo)) {
            upgrade();
        }
    }

    /**
     * Checks whether the WebAPK needs to be upgraded provided the new Web Manifest info.
     */
    protected boolean requireUpgrade(WebappInfo newInfo) {
        boolean scopeMatch = mWebappInfo.scopeUri().equals(newInfo.scopeUri());
        if (!scopeMatch) {
            // Sometimes the scope doesn't match due to a missing "/" at the end of the scope URL.
            // Print log to find such cases.
            Log.d(TAG, "Needs to request update since the scope from WebappInfo (%s) doesn't match"
                    + "the one fetched from Web Manifest(%s).", mWebappInfo.scopeUri().toString(),
                    newInfo.scopeUri().toString());
            return true;
        }

        // TODO(hanxi): Add icon comparison.
        if (!TextUtils.equals(mStartUrl, newInfo.uri().toString())
                || !TextUtils.equals(mWebappInfo.shortName(), newInfo.shortName())
                || !TextUtils.equals(mWebappInfo.name(), newInfo.name())
                || mWebappInfo.backgroundColor() != newInfo.backgroundColor()
                || mWebappInfo.themeColor() != newInfo.themeColor()
                || mWebappInfo.orientation() != newInfo.orientation()
                || mWebappInfo.displayMode() != newInfo.displayMode()) {
            return true;
        }

        return false;
    }

    private void upgrade() {}

    private native long nativeInitialize(WebContents webContents, String scope,
            String webManifestUrl);
    private native void nativeReplaceWebContents(long nativeManifestUpgradeDetector,
            WebContents webContents);
    private native void nativeDestroy(long nativeManifestUpgradeDetector);
    private native void nativeStart(long nativeManifestUpgradeDetector);
}
