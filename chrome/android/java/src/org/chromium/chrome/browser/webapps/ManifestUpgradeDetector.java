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
import org.chromium.chrome.browser.tab.Tab;

/**
 * This class checks whether the WebAPK needs to be re-installed and sends a request to re-install
 * the WebAPK if it needs to be re-installed.
 */
public class ManifestUpgradeDetector implements ManifestUpgradeDetectorFetcher.Callback {
    /**
     * The names of <meta-data> in the WebAPK's AndroidManifest.xml whose values are needed to
     * determine whether a WebAPK needs to be upgraded but which are not present in
     * {@link WebappInfo}. The names must stay in sync with
     * {@linkorg.chromium.webapk.lib.runtime_library.HostBrowserLauncher}.
     */
    public static final String META_DATA_START_URL = "org.chromium.webapk.shell_apk.startUrl";

    /** The WebAPK's tab. */
    private final Tab mTab;

    private WebappInfo mWebappInfo;

    private String mStartUrl;

    /**
     * Fetches the WebAPK's Web Manifest from the web.
     */
    private ManifestUpgradeDetectorFetcher mFetcher;

    private static final String TAG = "cr_UpgradeDetector";

    public ManifestUpgradeDetector(Tab tab, WebappInfo info) {
        mTab = tab;
        mWebappInfo = info;
    }

    /**
     * Starts fetching the web manifest resources.
     */
    public void start() {
        if (mWebappInfo.webManifestUri() == null
                || mWebappInfo.webManifestUri().equals(Uri.EMPTY)) {
            return;
        }

        if (mFetcher != null) return;

        getMetaDataFromAndroidManifest();
        mFetcher = createFetcher(
                mTab, mWebappInfo.scopeUri().toString(), mWebappInfo.webManifestUri().toString());
        mFetcher.start(this);
    }

    /**
     * Creates ManifestUpgradeDataFetcher.
     */
    protected ManifestUpgradeDetectorFetcher createFetcher(Tab tab, String scopeUrl,
            String manifestUrl) {
        return new ManifestUpgradeDetectorFetcher(tab, scopeUrl, manifestUrl);
    }

    private void getMetaDataFromAndroidManifest() {
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
        if (mFetcher != null) {
            mFetcher.destroy();
        }
        mFetcher = null;
    }

    /**
     * Called when the updated Web Manifest has been fetched.
     */
    @Override
    public void onGotManifestData(String startUrl, String scopeUrl, String name, String shortName,
            int displayMode, int orientation, long themeColor, long backgroundColor) {
        mFetcher.destroy();
        mFetcher = null;

        // TODO(hanxi): crbug.com/627824. Validate whether the new WebappInfo is
        // WebAPK-compatible.
        final WebappInfo newInfo = WebappInfo.create(mWebappInfo.id(), startUrl,
                scopeUrl, mWebappInfo.encodedIcon(), name, shortName, displayMode, orientation,
                mWebappInfo.source(), themeColor, backgroundColor, mWebappInfo.isIconGenerated(),
                mWebappInfo.webApkPackageName(), mWebappInfo.webManifestUri().toString());
        if (requireUpgrade(newInfo)) {
            upgrade();
        }

        onComplete();
    }

    /**
     * Checks whether the WebAPK needs to be upgraded provided the new Web Manifest info.
     */
    private boolean requireUpgrade(WebappInfo newInfo) {
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

    protected void upgrade() {}

    protected void onComplete() {}
}
