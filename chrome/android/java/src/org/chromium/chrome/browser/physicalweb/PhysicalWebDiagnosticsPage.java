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
    private static final int RESULT_FAILURE = 0;
    private static final int RESULT_SUCCESS = 1;
    private static final int RESULT_INDETERMINATE = 2;

    private final Context mContext;
    private final int mBackgroundColor;
    private final int mThemeColor;
    private final String mSuccessColor;
    private final String mFailureColor;
    private final String mIndeterminateColor;
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
        mIndeterminateColor = colorToHexValue(ApiCompatibilityUtils.getColor(resources,
                R.color.physical_web_diags_indeterminate_color));

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
        int successValue = (success ? RESULT_SUCCESS : RESULT_FAILURE);
        appendResult(sb, successValue, successMessage, failureMessage, null);
    }

    private void appendResult(StringBuilder sb, int successValue, String successMessage,
            String failureMessage, String indeterminateMessage) {
        String color;
        String message;
        if (successValue == RESULT_SUCCESS) {
            color = mSuccessColor;
            message = successMessage;
        } else if (successValue == RESULT_FAILURE) {
            color = mFailureColor;
            message = failureMessage;
        } else if (successValue == RESULT_INDETERMINATE) {
            color = mIndeterminateColor;
            message = indeterminateMessage;
        } else {
            return;
        }

        sb.append(String.format("<font color=\"%s\">%s</font><br/>", color, message));
    }

    private void appendPrerequisitesReport(StringBuilder sb) {
        boolean isSdkVersionCorrect = isSdkVersionCorrect();
        boolean isDataConnectionActive = isDataConnectionActive();
        int bluetoothStatus = getBluetoothEnabledStatus();
        boolean isLocationServicesEnabled = isLocationServicesEnabled();
        boolean isLocationPermissionGranted = isLocationPermissionGranted();
        boolean isPreferenceEnabled = PhysicalWeb.isPhysicalWebPreferenceEnabled(mContext);
        boolean isOnboarding = PhysicalWeb.isOnboarding(mContext);

        int prerequisitesResult = RESULT_SUCCESS;
        if (!isSdkVersionCorrect || !isDataConnectionActive || (bluetoothStatus == RESULT_FAILURE)
                || !isLocationServicesEnabled || !isLocationPermissionGranted
                || !isPreferenceEnabled) {
            prerequisitesResult = RESULT_FAILURE;
        } else if (bluetoothStatus == RESULT_INDETERMINATE) {
            prerequisitesResult = RESULT_INDETERMINATE;
        }

        sb.append("<h2>Status</h2>");

        sb.append("Physical Web is ");
        appendResult(sb, prerequisitesResult != RESULT_FAILURE, "ON", "OFF");

        sb.append("<h2>Prerequisites</h2>");

        sb.append("Android SDK version: ");
        appendResult(sb, isSdkVersionCorrect, "Compatible", "Incompatible");

        sb.append("Data connection: ");
        appendResult(sb, isDataConnectionActive, "Connected", "Not connected");

        sb.append("Location services: ");
        appendResult(sb, isLocationServicesEnabled, "Enabled", "Disabled");

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            sb.append("Location app permission: ");
            appendResult(sb, isLocationPermissionGranted, "Granted", "Not granted");
        }

        sb.append("Physical Web privacy settings: ");
        String preferenceDisabledMessage = (isOnboarding ? "Default (off)" : "Off");
        appendResult(sb, isPreferenceEnabled, "On", preferenceDisabledMessage);

        sb.append("Bluetooth: ");
        appendResult(sb, bluetoothStatus, "Enabled", "Disabled", "Unknown");

        // Append instructions for how to verify Bluetooth is enabled when we are unable to check
        // programmatically.
        if (bluetoothStatus == RESULT_INDETERMINATE) {
            sb.append("<br/>To verify Bluetooth is enabled on this device, check that the "
                    + "Bluetooth icon is shown in the status bar.");
        }
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

    private boolean isSdkVersionCorrect() {
        return (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT);
    }

    private boolean isDataConnectionActive() {
        ConnectivityManager cm =
                (ConnectivityManager) mContext.getSystemService(Context.CONNECTIVITY_SERVICE);
        return (cm.getActiveNetworkInfo() != null
                && cm.getActiveNetworkInfo().isConnectedOrConnecting());
    }

    private boolean isBluetoothPermissionGranted() {
        return PermissionChecker.checkSelfPermission(mContext, Manifest.permission.BLUETOOTH)
                == PackageManager.PERMISSION_GRANTED;
    }

    private int getBluetoothEnabledStatus() {
        int statusResult = RESULT_INDETERMINATE;
        if (isBluetoothPermissionGranted()) {
            BluetoothAdapter bt = BluetoothAdapter.getDefaultAdapter();
            statusResult = (bt != null && bt.isEnabled()) ? RESULT_SUCCESS : RESULT_FAILURE;
        }
        return statusResult;
    }

    private boolean isLocationServicesEnabled() {
        LocationManager lm = (LocationManager) mContext.getSystemService(Context.LOCATION_SERVICE);
        boolean isGpsProviderEnabled = lm.isProviderEnabled(LocationManager.GPS_PROVIDER);
        boolean isNetworkProviderEnabled = lm.isProviderEnabled(LocationManager.NETWORK_PROVIDER);
        return isGpsProviderEnabled || isNetworkProviderEnabled;
    }

    private boolean isLocationPermissionGranted() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            return true;
        }
        return PermissionChecker.checkSelfPermission(mContext,
                Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED;
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
