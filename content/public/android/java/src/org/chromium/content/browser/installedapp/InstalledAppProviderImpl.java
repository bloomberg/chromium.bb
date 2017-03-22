// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.installedapp;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.Resources;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.installedapp.mojom.InstalledAppProvider;
import org.chromium.installedapp.mojom.RelatedApplication;
import org.chromium.mojo.system.MojoException;

import java.net.URI;
import java.net.URISyntaxException;
import java.util.ArrayList;

/**
 * Android implementation of the InstalledAppProvider service defined in
 * installed_app_provider.mojom
 */
public class InstalledAppProviderImpl implements InstalledAppProvider {
    @VisibleForTesting
    public static final String ASSET_STATEMENTS_KEY = "asset_statements";
    private static final String ASSET_STATEMENT_FIELD_TARGET = "target";
    private static final String ASSET_STATEMENT_FIELD_NAMESPACE = "namespace";
    private static final String ASSET_STATEMENT_FIELD_SITE = "site";
    @VisibleForTesting
    public static final String ASSET_STATEMENT_NAMESPACE_WEB = "web";
    @VisibleForTesting
    public static final String RELATED_APP_PLATFORM_ANDROID = "play";

    private static final String TAG = "InstalledAppProvider";

    private final FrameUrlDelegate mFrameUrlDelegate;
    private final Context mContext;

    /**
     * Small interface for dynamically getting the URL of the current frame.
     *
     * Abstract to allow for testing.
     */
    public static interface FrameUrlDelegate {
        /**
         * Gets the URL of the current frame. Can return null (if the frame has disappeared).
         */
        public URI getUrl();
    }

    public InstalledAppProviderImpl(FrameUrlDelegate frameUrlDelegate, Context context) {
        mFrameUrlDelegate = frameUrlDelegate;
        mContext = context;
    }

    @Override
    public void filterInstalledApps(
            RelatedApplication[] relatedApps, FilterInstalledAppsResponse callback) {
        URI frameUrl = mFrameUrlDelegate.getUrl();
        ArrayList<RelatedApplication> installedApps = new ArrayList<RelatedApplication>();
        PackageManager pm = mContext.getPackageManager();
        for (RelatedApplication app : relatedApps) {
            // If the package is of type "play", it is installed, and the origin is associated with
            // package, add the package to the list of valid packages.
            // NOTE: For security, it must not be possible to distinguish (from the response)
            // between the app not being installed and the origin not being associated with the app
            // (otherwise, arbitrary websites would be able to test whether un-associated apps are
            // installed on the user's device).
            if (app.platform.equals(RELATED_APP_PLATFORM_ANDROID) && app.id != null
                    && isAppInstalledAndAssociatedWithOrigin(app.id, frameUrl, pm)) {
                installedApps.add(app);
            }
        }
        RelatedApplication[] installedAppsArray = new RelatedApplication[installedApps.size()];
        installedApps.toArray(installedAppsArray);
        callback.call(installedAppsArray);
    }

    @Override
    public void close() {}

    @Override
    public void onConnectionError(MojoException e) {}

    /**
     * Determines whether a particular app is installed and matches the origin.
     *
     * @param packageName Name of the Android package to check if installed. Returns false if the
     *                    app is not installed.
     * @param frameUrl Returns false if the Android package does not declare association with the
     *                origin of this URL. Can be null.
     */
    private static boolean isAppInstalledAndAssociatedWithOrigin(
            String packageName, URI frameUrl, PackageManager pm) {
        if (frameUrl == null) return false;

        // Early-exit if the Android app is not installed.
        JSONArray statements;
        try {
            statements = getAssetStatements(packageName, pm);
        } catch (NameNotFoundException e) {
            return false;
        }

        // The installed Android app has provided us with a list of asset statements. If any one of
        // those statements is a web asset that matches the given origin, return true.
        for (int i = 0; i < statements.length(); i++) {
            JSONObject statement;
            try {
                statement = statements.getJSONObject(i);
            } catch (JSONException e) {
                // If an element is not an object, just ignore it.
                continue;
            }

            URI site = getSiteForWebAsset(statement);

            // The URI is considered equivalent if the scheme, host, and port match, according
            // to the DigitalAssetLinks v1 spec.
            if (site != null && statementTargetMatches(frameUrl, site)) {
                return true;
            }
        }

        // No asset matched the origin.
        return false;
    }

    /**
     * Gets the asset statements from an Android app's manifest.
     *
     * This retrieves the list of statements from the Android app's "asset_statements" manifest
     * resource, as specified in Digital Asset Links v1.
     *
     * @param packageName Name of the Android package to get statements from.
     * @return The list of asset statements, parsed from JSON.
     * @throws NameNotFoundException if the application is not installed.
     */
    private static JSONArray getAssetStatements(String packageName, PackageManager pm)
            throws NameNotFoundException {
        // Get the <meta-data> from this app's manifest.
        // Throws NameNotFoundException if the application is not installed.
        ApplicationInfo appInfo = pm.getApplicationInfo(packageName, PackageManager.GET_META_DATA);
        int identifier = appInfo.metaData.getInt(ASSET_STATEMENTS_KEY);
        if (identifier == 0) {
            return new JSONArray();
        }

        // Throws NameNotFoundException in the rare case that the application was uninstalled since
        // getting |appInfo| (or resources could not be loaded for some other reason).
        Resources resources = pm.getResourcesForApplication(appInfo);

        String statements;
        try {
            statements = resources.getString(identifier);
        } catch (Resources.NotFoundException e) {
            // This should never happen, but it could if there was a broken APK, so handle it
            // gracefully without crashing.
            Log.w(TAG,
                    "Android package " + packageName + " missing asset statements resource (0x"
                            + Integer.toHexString(identifier) + ").");
            return new JSONArray();
        }

        try {
            return new JSONArray(statements);
        } catch (JSONException e) {
            // If the JSON is invalid or not an array, assume it is empty.
            Log.w(TAG,
                    "Android package " + packageName
                            + " has JSON syntax error in asset statements resource (0x"
                            + Integer.toHexString(identifier) + ").");
            return new JSONArray();
        }
    }

    /**
     * Gets the "site" URI from an Android asset statement.
     *
     * @return The site, or null if the asset string was invalid or not related to a web site. This
     *         could be because: the JSON string was invalid, there was no "target" field, this was
     *         not a web asset, there was no "site" field, or the "site" field was invalid.
     */
    private static URI getSiteForWebAsset(JSONObject statement) {
        JSONObject target;
        try {
            // Ignore the "relation" field and allow an asset with any relation to this origin.
            // TODO(mgiuca): [Spec issue] Should we require a specific relation string, rather
            // than any or no relation?
            target = statement.getJSONObject(ASSET_STATEMENT_FIELD_TARGET);
        } catch (JSONException e) {
            return null;
        }

        // If it is not a web asset, skip it.
        if (!isAssetWeb(target)) {
            return null;
        }

        try {
            return new URI(target.getString(ASSET_STATEMENT_FIELD_SITE));
        } catch (JSONException | URISyntaxException e) {
            return null;
        }
    }

    /**
     * Determines whether an Android asset statement is for a website.
     *
     * @param target The "target" field of the asset statement.
     */
    private static boolean isAssetWeb(JSONObject target) {
        String namespace;
        try {
            namespace = target.getString(ASSET_STATEMENT_FIELD_NAMESPACE);
        } catch (JSONException e) {
            return false;
        }

        return namespace.equals(ASSET_STATEMENT_NAMESPACE_WEB);
    }

    private static boolean statementTargetMatches(URI frameUrl, URI assetUrl) {
        if (assetUrl.getScheme() == null || assetUrl.getAuthority() == null) {
            return false;
        }

        return assetUrl.getScheme().equals(frameUrl.getScheme())
                && assetUrl.getAuthority().equals(frameUrl.getAuthority());
    }
}
