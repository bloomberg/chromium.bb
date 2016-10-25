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
    public static final long FULL_CHECK_UPDATE_INTERVAL = TimeUnit.DAYS.toMillis(3L);

    /**
     * Number of milliseconds to wait before re-requesting an updated WebAPK from the WebAPK
     * server if the previous update attempt failed.
     */
    public static final long RETRY_UPDATE_DURATION = TimeUnit.HOURS.toMillis(12L);

    /**
     * Id of WebAPK data in WebappDataStorage
     */
    private String mId;

    /** WebAPK package name. */
    private String mWebApkPackageName;

    /** Android version code of WebAPK. */
    private int mVersionCode;

    /**
     * Whether the previous WebAPK update succeeded. True if there has not been any update attempts.
     */
    private boolean mPreviousUpdateSucceeded;

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

        mWebApkPackageName = info.webApkPackageName();
        mId = info.id();
        mVersionCode = packageInfo.versionCode;
        final Bundle metadata = packageInfo.applicationInfo.metaData;
        int shellApkVersion =
                IntentUtils.safeGetInt(metadata, WebApkMetaDataKeys.SHELL_APK_VERSION, 0);

        WebappDataStorage storage = WebappRegistry.getInstance().getWebappDataStorage(mId);
        mPreviousUpdateSucceeded = didPreviousUpdateSucceed(storage);

        // TODO(pkotwicz|hanxi): Request upgrade if ShellAPK version changes if
        // ManifestUpgradeDetector cannot fetch the Web Manifest. For instance, the web developer
        // may have removed the Web Manifest from their site. (http://crbug.com/639536)

        if (!shouldCheckIfWebManifestUpdated(storage, shellApkVersion, mPreviousUpdateSucceeded)) {
            return;
        }

        mUpgradeDetector = buildManifestUpgradeDetector(tab, info, metadata);
        if (!mUpgradeDetector.start()) return;

        // crbug.com/636525. The timestamp of the last manifest update check should be updated after
        // the detector finds the manifest, not when the detector is started.
        storage.updateTimeOfLastCheckForUpdatedWebManifest();
    }

    @Override
    public void onUpgradeNeededCheckFinished(boolean needsUpgrade,
            ManifestUpgradeDetector.FetchedManifestData data) {
        String manifestUrl = mUpgradeDetector.getManifestUrl();

        mUpgradeDetector.destroy();
        mUpgradeDetector = null;

        Log.v(TAG, "WebAPK upgrade needed: " + needsUpgrade);

        if (!needsUpgrade) {
            if (!mPreviousUpdateSucceeded) {
                recordUpdateInWebappDataStorage(mId, true);
            }
            return;
        }

        // Set WebAPK update as having failed in case that Chrome is killed prior to
        // {@link onBuiltWebApk} being called.
        recordUpdateInWebappDataStorage(mId, false);
        updateAsync(manifestUrl, data);
    }

    /**
     * Builds {@link ManifestUpgradeDetector}. In a separate function for the sake of tests.
     */
    protected ManifestUpgradeDetector buildManifestUpgradeDetector(
            Tab tab, WebappInfo info, Bundle metaData) {
        return new ManifestUpgradeDetector(tab, info, metaData, this);
    }

    /**
     * Sends request to WebAPK Server to update WebAPK.
     */
    public void updateAsync(String manifestUrl, ManifestUpgradeDetector.FetchedManifestData data) {
        nativeUpdateAsync(mId, data.startUrl, data.scopeUrl, data.name, data.shortName,
                data.iconUrl, data.iconMurmur2Hash, data.icon, data.displayMode, data.orientation,
                data.themeColor, data.backgroundColor, manifestUrl, mWebApkPackageName,
                mVersionCode);
    }

    public void destroy() {
        if (mUpgradeDetector != null) {
            mUpgradeDetector.destroy();
        }
        mUpgradeDetector = null;
    }

    /** Returns the current time. In a separate function for the sake of testing. */
    protected long currentTimeMillis() {
        return System.currentTimeMillis();
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
     * Returns whether the previous WebAPK update attempt succeeded. Returns true if there has not
     * been any update attempts.
     */
    private static boolean didPreviousUpdateSucceed(WebappDataStorage storage) {
        long lastUpdateCompletionTime = storage.getLastWebApkUpdateRequestCompletionTime();
        if (lastUpdateCompletionTime == WebappDataStorage.LAST_USED_INVALID
                || lastUpdateCompletionTime == WebappDataStorage.LAST_USED_UNSET) {
            return true;
        }
        return storage.getDidLastWebApkUpdateRequestSucceed();
    }

    /**
     * Returns whether the Web Manifest should be refetched to check whether it has been updated.
     * TODO: Make this method static once there is a static global clock class.
     * @param storage WebappDataStorage with the WebAPK's cached data.
     * @param shellApkVersion Version number of //chrome/android/webapk/shell_apk code.
     * @param previousUpdateSucceeded Whether the previous update attempt succeeded.
     * True if there has not been any update attempts.
     */
    private boolean shouldCheckIfWebManifestUpdated(
            WebappDataStorage storage, int shellApkVersion, boolean previousUpdateSucceeded) {
        if (CommandLine.getInstance().hasSwitch(
                    ChromeSwitches.CHECK_FOR_WEB_MANIFEST_UPDATE_ON_STARTUP)) {
            return true;
        }

        if (shellApkVersion < WebApkVersion.CURRENT_SHELL_APK_VERSION) return true;

        long now = currentTimeMillis();
        long sinceLastCheckDurationMs = now - storage.getLastCheckForWebManifestUpdateTime();
        if (sinceLastCheckDurationMs >= FULL_CHECK_UPDATE_INTERVAL) return true;

        long sinceLastUpdateRequestDurationMs =
                now - storage.getLastWebApkUpdateRequestCompletionTime();
        return sinceLastUpdateRequestDurationMs >= RETRY_UPDATE_DURATION
                && !previousUpdateSucceeded;
    }

    /**
     * Updates {@link WebappDataStorage} with the time of the latest WebAPK update and whether the
     * WebAPK update succeeded.
     */
    private static void recordUpdateInWebappDataStorage(String id, boolean success) {
        WebappDataStorage storage = WebappRegistry.getInstance().getWebappDataStorage(id);
        // Update the request time and result together. It prevents getting a correct request time
        // but a result from the previous request.
        storage.updateTimeOfLastWebApkUpdateRequestCompletion();
        storage.updateDidLastWebApkUpdateRequestSucceed(success);
    }

    /**
     * Called after either a request to update the WebAPK has been sent or the update process
     * fails.
     */
    @CalledByNative
    private static void onBuiltWebApk(String id, boolean success) {
        recordUpdateInWebappDataStorage(id, success);
    }

    private static native void nativeUpdateAsync(String id, String startUrl, String scope,
            String name, String shortName, String iconUrl, String iconMurmur2Hash, Bitmap icon,
            int displayMode, int orientation, long themeColor, long backgroundColor,
            String manifestUrl, String webApkPackage, int webApkVersion);
}
