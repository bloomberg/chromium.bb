// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.physicalweb;

import android.Manifest;
import android.bluetooth.BluetoothAdapter;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.location.LocationManager;
import android.net.ConnectivityManager;
import android.os.Build;
import android.support.v4.content.PermissionChecker;
import android.text.Html;
import android.text.util.Linkify;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.NativePage;
import org.chromium.chrome.browser.UrlConstants;

import java.util.HashSet;
import java.util.Set;

/**
 * Provides diagnostic information about the Physical Web feature.
 */
public class PhysicalWebDiagnosticsPage implements NativePage {
    private final Context mContext;
    private final int mBackgroundColor;
    private final int mThemeColor;
    private final String mSuccessColor;
    private final String mFailureColor;
    private final View mPageView;
    private final Button mLaunchButton;
    private final TextView mDiagnosticsText;

    public PhysicalWebDiagnosticsPage(Context context) {
        mContext = context;

        Resources resources = mContext.getResources();
        mBackgroundColor = ApiCompatibilityUtils.getColor(resources, R.color.ntp_bg);
        mThemeColor = ApiCompatibilityUtils.getColor(resources, R.color.default_primary_color);
        mSuccessColor = colorToHexValue(ApiCompatibilityUtils.getColor(resources,
                R.color.physical_web_diags_success_color));
        mFailureColor = colorToHexValue(ApiCompatibilityUtils.getColor(resources,
                R.color.physical_web_diags_failure_color));

        LayoutInflater inflater = LayoutInflater.from(mContext);
        mPageView = inflater.inflate(R.layout.physical_web_diagnostics, null);

        mLaunchButton = (Button) mPageView.findViewById(R.id.physical_web_launch);
        mLaunchButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mContext.startActivity(createListUrlsIntent(mContext));
            }
        });

        mDiagnosticsText = (TextView) mPageView.findViewById(R.id.physical_web_diagnostics_text);
        mDiagnosticsText.setAutoLinkMask(Linkify.WEB_URLS);
        mDiagnosticsText.setText(Html.fromHtml(createDiagnosticsReportHtml()));
    }

    private void appendResult(StringBuilder sb, boolean success, String successMessage,
            String failureMessage) {
        sb.append(String.format("<font color=\"%s\">%s</font><br/>",
                success ? mSuccessColor : mFailureColor,
                success ? successMessage : failureMessage));
    }

    private boolean appendSdkVersionReport(StringBuilder sb) {
        boolean isAndroidKOrLater = (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT);

        sb.append("Android SDK version: ");
        appendResult(sb, isAndroidKOrLater, "Compatible", "Incompatible");
        return isAndroidKOrLater;
    }

    private boolean appendDataConnectivityReport(StringBuilder sb) {
        ConnectivityManager cm =
                (ConnectivityManager) mContext.getSystemService(Context.CONNECTIVITY_SERVICE);
        boolean isDataConnectionActive = (cm.getActiveNetworkInfo() != null
                && cm.getActiveNetworkInfo().isConnectedOrConnecting());

        sb.append("Data connection: ");
        appendResult(sb, isDataConnectionActive, "Connected", "Not connected");
        return isDataConnectionActive;
    }

    private boolean appendBluetoothReport(StringBuilder sb) {
        BluetoothAdapter bt = BluetoothAdapter.getDefaultAdapter();
        boolean isBluetoothEnabled = (bt != null && bt.isEnabled());

        sb.append("Bluetooth: ");
        appendResult(sb, isBluetoothEnabled, "Enabled", "Disabled");
        return isBluetoothEnabled;
    }

    private boolean appendLocationServicesReport(StringBuilder sb) {
        LocationManager lm = (LocationManager) mContext.getSystemService(Context.LOCATION_SERVICE);
        boolean isGpsProviderEnabled = lm.isProviderEnabled(LocationManager.GPS_PROVIDER);
        boolean isNetworkProviderEnabled = lm.isProviderEnabled(LocationManager.NETWORK_PROVIDER);
        boolean isLocationProviderAvailable = isGpsProviderEnabled || isNetworkProviderEnabled;

        sb.append("Location services: ");
        appendResult(sb, isLocationProviderAvailable, "Enabled", "Disabled");
        return isLocationProviderAvailable;
    }

    private boolean appendLocationAppPermissionReport(StringBuilder sb) {
        boolean isLocationPermissionGranted = false;
        isLocationPermissionGranted = PermissionChecker.checkSelfPermission(mContext,
                Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED;

        sb.append("Location app permission: ");
        appendResult(sb, isLocationPermissionGranted, "Granted", "Not granted");
        return isLocationPermissionGranted;
    }

    private boolean appendPhysicalWebPrivacyPreferenceReport(StringBuilder sb) {
        boolean isPreferenceEnabled = PhysicalWeb.isPhysicalWebPreferenceEnabled(mContext);
        boolean isOnboarding = PhysicalWeb.isOnboarding(mContext);

        String failureMessage = (isOnboarding ? "Default (off)" : "Off");

        sb.append("Physical Web privacy settings: ");
        appendResult(sb, isPreferenceEnabled, "On", failureMessage);
        return isPreferenceEnabled;
    }

    private void appendPrerequisitesReport(StringBuilder sb) {
        boolean isAndroidMOrLater = (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M);

        sb.append("<h2>Prerequisites</h2>");

        boolean isSdkVersionCorrect = appendSdkVersionReport(sb);
        boolean isDataConnectionActive = appendDataConnectivityReport(sb);
        boolean isBluetoothEnabled = appendBluetoothReport(sb);
        boolean isLocationProviderAvailable = appendLocationServicesReport(sb);
        boolean isLocationPermissionGranted = true;
        if (isAndroidMOrLater) {
            isLocationPermissionGranted = appendLocationAppPermissionReport(sb);
        }
        boolean isPreferenceEnabled = appendPhysicalWebPrivacyPreferenceReport(sb);

        boolean allSucceeded = isSdkVersionCorrect && isDataConnectionActive && isBluetoothEnabled
                && isLocationProviderAvailable && isLocationPermissionGranted
                && isPreferenceEnabled;

        sb.append("<br/>");
        if (allSucceeded) {
            sb.append("All prerequisite checks ");
        } else {
            sb.append("One or more prerequisite checks ");
        }
        appendResult(sb, allSucceeded, "SUCCEEDED", "FAILED");
    }

    private void appendUrlManagerReport(StringBuilder sb) {
        UrlManager urlManager = UrlManager.getInstance(mContext);

        Set<String> nearbyUrls = urlManager.getCachedNearbyUrls();
        Set<String> resolvedUrls = urlManager.getCachedResolvedUrls();
        Set<String> union = new HashSet<String>(nearbyUrls);
        union.addAll(resolvedUrls);

        sb.append("<h2>Nearby web pages</h2>");

        if (union.isEmpty()) {
            sb.append("No nearby web pages found<br/>");
        } else {
            for (String url : union) {
                boolean isNearby = nearbyUrls.contains(url);
                boolean isResolved = resolvedUrls.contains(url);

                sb.append(url);
                if (!isNearby) {
                    sb.append(" : LOST");
                } else if (!isResolved) {
                    sb.append(" : UNRESOLVED");
                }
                sb.append("<br/>");
            }
        }
    }

    private String createDiagnosticsReportHtml() {
        StringBuilder sb = new StringBuilder();
        appendPrerequisitesReport(sb);
        sb.append("<br/>");
        appendUrlManagerReport(sb);
        return sb.toString();
    }

    private static Intent createListUrlsIntent(Context context) {
        Intent intent = new Intent(context, ListUrlsActivity.class);
        intent.putExtra(ListUrlsActivity.REFERER_KEY, ListUrlsActivity.OPTIN_REFERER);
        return intent;
    }

    private static String colorToHexValue(int color) {
        return "#" + Integer.toHexString(color & 0x00ffffff);
    }

    // NativePage overrides

    @Override
    public void destroy() {
        // nothing to do
    }

    @Override
    public String getUrl() {
        return UrlConstants.PHYSICAL_WEB_URL;
    }

    @Override
    public String getTitle() {
        return "Physical Web Diagnostics";
    }

    @Override
    public int getBackgroundColor() {
        return mBackgroundColor;
    }

    @Override
    public int getThemeColor() {
        return mThemeColor;
    }

    @Override
    public View getView() {
        return mPageView;
    }

    @Override
    public String getHost() {
        return UrlConstants.PHYSICAL_WEB_HOST;
    }

    @Override
    public void updateForUrl(String url) {
        // nothing to do
    }
}