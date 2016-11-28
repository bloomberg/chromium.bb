// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.text.TextUtils;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.UrlUtilities;

import java.util.Map;

/**
 * This class checks whether the WebAPK needs to be re-installed and sends a request to re-install
 * the WebAPK if it needs to be re-installed.
 */
public class ManifestUpgradeDetector implements ManifestUpgradeDetectorFetcher.Callback {
    /** ManifestUpgradeDetector callback. */
    public interface Callback {
        /**
         * Called when the Web Manifest for the initial URL load has been fetched (successfully or
         * unsuccessfully).
         * TODO(pkotwicz): Add calls to {@link #onFinishedFetchingWebManifestForInitialUrl()}.
         * @param needsUpgrade Whether the WebAPK should be updated because the Web Manifest has
         *                     changed. False if the Web Manifest could not be fetched.
         * @param info         The fetched Web Manifest data. Null if the initial URL does not point
         *                     to a Web Manifest.
         * @param bestIconUrl  The icon URL in {@link WebApkInfo#iconUrlToMurmur2HashMap()} best
         *                     suited for use as the launcher icon on this device.
         */
        void onFinishedFetchingWebManifestForInitialUrl(
                boolean needsUpgrade, WebApkInfo info, String bestIconUrl);

        /**
         * Called when the Web Manifest has been successfully fetched (including on the initial URL
         * load).
         * @param needsUpgrade Whether the WebAPK should be updated because the Web Manifest has
         *                     changed.
         * @param info         The fetched Web Manifest data.
         * @param bestIconUrl  The icon URL in {@link WebApkInfo#iconUrlToMurmur2HashMap()} best
         *                     suited for use as the launcher icon on this device.
         */
        void onGotManifestData(boolean needsUpgrade, WebApkInfo info, String bestIconUrl);
    }

    private static final String TAG = "cr_UpgradeDetector";

    /** The WebAPK's tab. */
    private final Tab mTab;

    /**
     * Web Manifest data at time that the WebAPK was generated.
     */
    private WebApkInfo mInfo;

    /**
     * Fetches the WebAPK's Web Manifest from the web.
     */
    private ManifestUpgradeDetectorFetcher mFetcher;
    private Callback mCallback;

    /**
     * Creates an instance of {@link ManifestUpgradeDetector}.
     *
     * @param tab WebAPK's tab.
     * @param webappInfo Parameters used for generating the WebAPK. Extracted from WebAPK's Android
     *                   manifest.
     * @param info       Web Manifest data at the time that the WebAPK was generated.
     * @param callback   Called once it has been determined whether the WebAPK needs to be upgraded.
     */
    public ManifestUpgradeDetector(Tab tab, WebApkInfo info, Callback callback) {
        mTab = tab;
        mInfo = info;
        mCallback = callback;
    }

    /**
     * Starts fetching the web manifest resources.
     */
    public boolean start() {
        if (mFetcher != null) return false;

        mFetcher = createFetcher();
        return mFetcher.start(mTab, mInfo, this);
    }

    /**
     * Creates ManifestUpgradeDataFetcher.
     */
    protected ManifestUpgradeDetectorFetcher createFetcher() {
        return new ManifestUpgradeDetectorFetcher();
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
    public void onGotManifestData(WebApkInfo fetchedInfo, String bestIconUrl) {
        mFetcher.destroy();
        mFetcher = null;

        // TODO(hanxi): crbug.com/627824. Validate whether the new fetched data is
        // WebAPK-compatible.
        boolean upgrade = needsUpgrade(fetchedInfo, bestIconUrl);
        mCallback.onGotManifestData(upgrade, fetchedInfo, bestIconUrl);
    }

    /**
     * Checks whether the WebAPK needs to be upgraded provided the fetched manifest data.
     */
    private boolean needsUpgrade(WebApkInfo fetchedInfo, String bestIconUrl) {
        // We should have computed the Murmur2 hash for the bitmap at the best icon URL for
        // {@link fetchedInfo} (but not the other icon URLs.)
        String fetchedBestIconMurmur2Hash = fetchedInfo.iconUrlToMurmur2HashMap().get(bestIconUrl);
        String bestIconMurmur2Hash =
                findMurmur2HashForUrlIgnoringFragment(mInfo.iconUrlToMurmur2HashMap(), bestIconUrl);

        return !TextUtils.equals(bestIconMurmur2Hash, fetchedBestIconMurmur2Hash)
                || !urlsMatchIgnoringFragments(
                           mInfo.scopeUri().toString(), fetchedInfo.scopeUri().toString())
                || !urlsMatchIgnoringFragments(
                           mInfo.manifestStartUrl(), fetchedInfo.manifestStartUrl())
                || !TextUtils.equals(mInfo.shortName(), fetchedInfo.shortName())
                || !TextUtils.equals(mInfo.name(), fetchedInfo.name())
                || mInfo.backgroundColor() != fetchedInfo.backgroundColor()
                || mInfo.themeColor() != fetchedInfo.themeColor()
                || mInfo.orientation() != fetchedInfo.orientation()
                || mInfo.displayMode() != fetchedInfo.displayMode();
    }

    /**
     * Returns the Murmur2 hash for entry in {@link iconUrlToMurmur2HashMap} whose canonical
     * representation, ignoring fragments, matches {@link iconUrlToMatch}.
     */
    private String findMurmur2HashForUrlIgnoringFragment(
            Map<String, String> iconUrlToMurmur2HashMap, String iconUrlToMatch) {
        for (Map.Entry<String, String> entry : iconUrlToMurmur2HashMap.entrySet()) {
            if (urlsMatchIgnoringFragments(entry.getKey(), iconUrlToMatch)) {
                return entry.getValue();
            }
        }
        return null;
    }

    /**
     * Returns whether the URLs match ignoring fragments. Canonicalizes the URLs prior to doing the
     * comparison.
     */
    protected boolean urlsMatchIgnoringFragments(String url1, String url2) {
        return UrlUtilities.urlsMatchIgnoringFragments(url1, url2);
    }
}
