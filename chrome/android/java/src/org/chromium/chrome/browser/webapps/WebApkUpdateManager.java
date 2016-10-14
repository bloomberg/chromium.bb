// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.os.Bundle;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.webapk.lib.client.WebApkVersion;
import org.chromium.webapk.lib.common.WebApkMetaDataKeys;

import java.util.concurrent.TimeUnit;

/**
 * WebApkUpdateManager manages when to check for updates to the WebAPK's Web Manifest, and sends
 * an update request to the WebAPK Server when an update is needed.
 */
public class WebApkUpdateManager implements ManifestUpgradeDetector.Callback {
    private static final String TAG = "WebApkUpdateManager";

    /** Number of milliseconds between checks for whether the WebAPK's Web Manifest has changed. */
    private static final long FULL_CHECK_UPDATE_INTERVAL = TimeUnit.DAYS.toMillis(3L);

    /**
     * Number of milliseconds to wait before re-requesting an updated WebAPK from the WebAPK
     * server if the previous update attempt failed.
     */
    private static final long RETRY_UPDATE_DURATION = TimeUnit.HOURS.toMillis(12L);

    /**
     * Id of WebAPK data in WebappDataStorage
     */
    private String mId;

    /** Version number of //chrome/android/webapk/shell_apk code. */
    private int mShellApkVersion;

    /** Android version code of WebAPK. */
    private int mVersionCode;

    /**
     * Whether a request to upgrade the WebAPK should be sent regardless of whether the Web Manifest
     * has changed.
     */
    private boolean mForceUpgrade;

    private ManifestUpgradeDetector mUpgradeDetector;

    /**
     * Checks whether the WebAPK's Web Manifest has changed. Requests an updated WebAPK if the
     * Web Manifest has changed. Skips the check if the check was done recently.
     * @param tab  The tab of the WebAPK.
     * @param info The WebappInfo of the WebAPK.
     */
    public void updateIfNeeded(Tab tab, WebappInfo info) {
        PackageInfo packageInfo = readPackageInfoFromAndroidManifest(info.webApkPackageName());
        if (packageInfo == null) {
            return;
        }

        mId = info.id();
        mVersionCode = packageInfo.versionCode;
        final Bundle metadata = packageInfo.applicationInfo.metaData;
        mShellApkVersion =
                IntentUtils.safeGetInt(metadata, WebApkMetaDataKeys.SHELL_APK_VERSION, 0);

        mUpgradeDetector = new ManifestUpgradeDetector(tab, info, metadata, this);

        WebappDataStorage storage = WebappRegistry.getInstance().getWebappDataStorage(info.id());
        if (forceUpgrade(storage)) mForceUpgrade = true;

        // TODO(pkotwicz|hanxi): Request upgrade if ShellAPK version changes if
        // ManifestUpgradeDetector cannot fetch the Web Manifest. For instance, the web developer
        // may have removed the Web Manifest from their site. (http://crbug.com/639536)

        long sinceLastCheckDuration = System.currentTimeMillis()
                - storage.getLastCheckForWebManifestUpdateTime();
        if (sinceLastCheckDuration > FULL_CHECK_UPDATE_INTERVAL || mForceUpgrade) {
            if (mUpgradeDetector.start()) {
                // crbug.com/636525. The timestamp of the last manifest update check should be
                // updated after the detector finds the manifest, not when the detector is started.
                storage.updateTimeOfLastCheckForUpdatedWebManifest();
            }
        }
    }

    @Override
    public void onUpgradeNeededCheckFinished(boolean needsUpgrade,
            ManifestUpgradeDetector.FetchedManifestData data) {
        needsUpgrade = (needsUpgrade || mForceUpgrade);
        Log.v(TAG, "WebAPK upgrade needed: " + needsUpgrade);
        if (needsUpgrade) {
            updateAsync(data);
        }
        if (mUpgradeDetector != null) {
            mUpgradeDetector.destroy();
        }
        mUpgradeDetector = null;
    }

    /**
     * Sends request to WebAPK Server to update WebAPK.
     */
    public void updateAsync(ManifestUpgradeDetector.FetchedManifestData data) {
        String packageName = mUpgradeDetector.getWebApkPackageName();
        nativeUpdateAsync(mId, data.startUrl, data.scopeUrl, data.name, data.shortName,
                data.iconUrl, data.iconMurmur2Hash, data.icon, data.displayMode, data.orientation,
                data.themeColor, data.backgroundColor, mUpgradeDetector.getManifestUrl(),
                packageName, mVersionCode);
    }

    public void destroy() {
        if (mUpgradeDetector != null) {
            mUpgradeDetector.destroy();
        }
        mUpgradeDetector = null;
    }

    /**
     * Reads the WebAPK's PackageInfo from the Android Manifest.
     */
    private PackageInfo readPackageInfoFromAndroidManifest(String webApkPackage) {
        try {
            PackageManager packageManager =
                    ContextUtils.getApplicationContext().getPackageManager();
            return packageManager.getPackageInfo(webApkPackage, PackageManager.GET_META_DATA);
        } catch (PackageManager.NameNotFoundException e) {
            e.printStackTrace();
        }
        return null;
    }

    /**
     * Returns whether an upgrade should be requested regardless of whether the Web Manifest has
     * changed.
     */
    private boolean forceUpgrade(WebappDataStorage storage) {
        if (CommandLine.getInstance().hasSwitch(
                    ChromeSwitches.CHECK_FOR_WEB_MANIFEST_UPDATE_ON_STARTUP)) {
            return true;
        }

        long sinceLastUpdateRequestDuration =
                System.currentTimeMillis() - storage.getLastWebApkUpdateRequestCompletionTime();
        if (sinceLastUpdateRequestDuration <= RETRY_UPDATE_DURATION) {
            return false;
        }

        return mShellApkVersion < WebApkVersion.CURRENT_SHELL_APK_VERSION
                || !storage.getDidLastWebApkUpdateRequestSucceed();
    }

    /**
     * Called after either a request to update the WebAPK has been sent or the update process
     * fails.
     */
    @CalledByNative
    private static void onBuiltWebApk(String id, final boolean success) {
        WebappDataStorage storage = WebappRegistry.getInstance().getWebappDataStorage(id);
        // Update the request time and result together. It prevents getting a correct request time
        // but a result from the previous request.
        storage.updateTimeOfLastWebApkUpdateRequestCompletion();
        storage.updateDidLastWebApkUpdateRequestSucceed(success);
    }

    private static native void nativeUpdateAsync(String id, String startUrl, String scope,
            String name, String shortName, String iconUrl, String iconMurmur2Hash, Bitmap icon,
            int displayMode, int orientation, long themeColor, long backgroundColor,
            String manifestUrl, String webApkPackage, int webApkVersion);
}
