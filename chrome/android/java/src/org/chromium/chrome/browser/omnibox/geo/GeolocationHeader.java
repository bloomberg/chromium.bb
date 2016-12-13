// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.geo;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageManager;
import android.location.Location;
import android.location.LocationManager;
import android.net.Uri;
import android.os.Build;
import android.os.Process;
import android.support.annotation.IntDef;
import android.util.Base64;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.preferences.website.ContentSetting;
import org.chromium.chrome.browser.preferences.website.GeolocationInfo;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.UrlUtilities;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;
import java.util.Locale;

/**
 * Provides methods for building the X-Geo HTTP header, which provides device location to a server
 * when making an HTTP request.
 *
 * X-Geo header spec: https://goto.google.com/xgeospec.
 */
public class GeolocationHeader {

    // Values for the histogram Geolocation.HeaderSentOrNot. Values 1, 5, 6, and 7 are defined in
    // histograms.xml and should not be used in other ways.
    public static final int UMA_LOCATION_DISABLED_FOR_GOOGLE_DOMAIN = 0;
    public static final int UMA_LOCATION_NOT_AVAILABLE = 2;
    public static final int UMA_LOCATION_STALE = 3;
    public static final int UMA_HEADER_SENT = 4;
    public static final int UMA_LOCATION_DISABLED_FOR_CHROME_APP = 5;
    public static final int UMA_MAX = 8;

    // Values for the histogram Geolocation.Header.PermissionState.
    // These are used to back an UMA histogram and so should be treated as append-only.
    //
    // In order to keep the names of these constants from being too long, the following were used:
    // UMA_PERM to indicate UMA location permission related metrics,
    // APP_YES (instead of APP_GRANTED) to indicate App permission granted,
    // DOMAIN_YES (instead of DOMAIN_GRANTED) to indicate Domain permission granted.
    public static final int UMA_PERM_UNKNOWN = 0;
    public static final int UMA_PERM_HIGH_ACCURACY_APP_YES_DOMAIN_YES_LOCATION = 1;
    public static final int UMA_PERM_HIGH_ACCURACY_APP_YES_DOMAIN_YES_NO_LOCATION = 2;
    public static final int UMA_PERM_HIGH_ACCURACY_APP_YES_DOMAIN_PROMPT_LOCATION = 3;
    public static final int UMA_PERM_HIGH_ACCURACY_APP_YES_DOMAIN_PROMPT_NO_LOCATION = 4;
    public static final int UMA_PERM_HIGH_ACCURACY_APP_YES_DOMAIN_BLOCKED = 5;
    public static final int UMA_PERM_HIGH_ACCURACY_APP_PROMPT_DOMAIN_YES = 6;
    public static final int UMA_PERM_HIGH_ACCURACY_APP_PROMPT_DOMAIN_PROMPT = 7;
    public static final int UMA_PERM_HIGH_ACCURACY_APP_PROMPT_DOMAIN_BLOCKED = 8;
    public static final int UMA_PERM_HIGH_ACCURACY_APP_BLOCKED_DOMAIN_YES = 9;
    public static final int UMA_PERM_HIGH_ACCURACY_APP_BLOCKED_DOMAIN_PROMPT = 10;
    public static final int UMA_PERM_HIGH_ACCURACY_APP_BLOCKED_DOMAIN_BLOCKED = 11;
    public static final int UMA_PERM_BATTERY_SAVING_APP_YES_DOMAIN_YES_LOCATION = 12;
    public static final int UMA_PERM_BATTERY_SAVING_APP_YES_DOMAIN_YES_NO_LOCATION = 13;
    public static final int UMA_PERM_BATTERY_SAVING_APP_YES_DOMAIN_PROMPT_LOCATION = 14;
    public static final int UMA_PERM_BATTERY_SAVING_APP_YES_DOMAIN_PROMPT_NO_LOCATION = 15;
    public static final int UMA_PERM_BATTERY_SAVING_APP_YES_DOMAIN_BLOCKED = 16;
    public static final int UMA_PERM_BATTERY_SAVING_APP_PROMPT_DOMAIN_YES = 17;
    public static final int UMA_PERM_BATTERY_SAVING_APP_PROMPT_DOMAIN_PROMPT = 18;
    public static final int UMA_PERM_BATTERY_SAVING_APP_PROMPT_DOMAIN_BLOCKED = 19;
    public static final int UMA_PERM_BATTERY_SAVING_APP_BLOCKED_DOMAIN_YES = 20;
    public static final int UMA_PERM_BATTERY_SAVING_APP_BLOCKED_DOMAIN_PROMPT = 21;
    public static final int UMA_PERM_BATTERY_SAVING_APP_BLOCKED_DOMAIN_BLOCKED = 22;
    public static final int UMA_PERM_GPS_ONLY_APP_YES_DOMAIN_YES_LOCATION = 23;
    public static final int UMA_PERM_GPS_ONLY_APP_YES_DOMAIN_YES_NO_LOCATION = 24;
    public static final int UMA_PERM_GPS_ONLY_APP_YES_DOMAIN_PROMPT_LOCATION = 25;
    public static final int UMA_PERM_GPS_ONLY_APP_YES_DOMAIN_PROMPT_NO_LOCATION = 26;
    public static final int UMA_PERM_GPS_ONLY_APP_YES_DOMAIN_BLOCKED = 27;
    public static final int UMA_PERM_GPS_ONLY_APP_PROMPT_DOMAIN_YES = 28;
    public static final int UMA_PERM_GPS_ONLY_APP_PROMPT_DOMAIN_PROMPT = 29;
    public static final int UMA_PERM_GPS_ONLY_APP_PROMPT_DOMAIN_BLOCKED = 30;
    public static final int UMA_PERM_GPS_ONLY_APP_BLOCKED_DOMAIN_YES = 31;
    public static final int UMA_PERM_GPS_ONLY_APP_BLOCKED_DOMAIN_PROMPT = 32;
    public static final int UMA_PERM_GPS_ONLY_APP_BLOCKED_DOMAIN_BLOCKED = 33;
    public static final int UMA_PERM_MASTER_OFF_APP_YES_DOMAIN_YES = 34;
    public static final int UMA_PERM_MASTER_OFF_APP_YES_DOMAIN_PROMPT = 35;
    public static final int UMA_PERM_MASTER_OFF_APP_YES_DOMAIN_BLOCKED = 36;
    public static final int UMA_PERM_MASTER_OFF_APP_PROMPT_DOMAIN_YES = 37;
    public static final int UMA_PERM_MASTER_OFF_APP_PROMPT_DOMAIN_PROMPT = 38;
    public static final int UMA_PERM_MASTER_OFF_APP_PROMPT_DOMAIN_BLOCKED = 39;
    public static final int UMA_PERM_MASTER_OFF_APP_BLOCKED_DOMAIN_YES = 40;
    public static final int UMA_PERM_MASTER_OFF_APP_BLOCKED_DOMAIN_PROMPT = 41;
    public static final int UMA_PERM_MASTER_OFF_APP_BLOCKED_DOMAIN_BLOCKED = 42;
    public static final int UMA_PERM_COUNT = 43;
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({UMA_PERM_HIGH_ACCURACY_APP_YES_DOMAIN_YES_LOCATION,
            UMA_PERM_HIGH_ACCURACY_APP_YES_DOMAIN_YES_NO_LOCATION,
            UMA_PERM_HIGH_ACCURACY_APP_YES_DOMAIN_PROMPT_LOCATION,
            UMA_PERM_HIGH_ACCURACY_APP_YES_DOMAIN_PROMPT_NO_LOCATION,
            UMA_PERM_HIGH_ACCURACY_APP_YES_DOMAIN_BLOCKED,
            UMA_PERM_HIGH_ACCURACY_APP_PROMPT_DOMAIN_YES,
            UMA_PERM_HIGH_ACCURACY_APP_PROMPT_DOMAIN_PROMPT,
            UMA_PERM_HIGH_ACCURACY_APP_PROMPT_DOMAIN_BLOCKED,
            UMA_PERM_HIGH_ACCURACY_APP_BLOCKED_DOMAIN_YES,
            UMA_PERM_HIGH_ACCURACY_APP_BLOCKED_DOMAIN_PROMPT,
            UMA_PERM_HIGH_ACCURACY_APP_BLOCKED_DOMAIN_BLOCKED,
            UMA_PERM_BATTERY_SAVING_APP_YES_DOMAIN_YES_LOCATION,
            UMA_PERM_BATTERY_SAVING_APP_YES_DOMAIN_YES_NO_LOCATION,
            UMA_PERM_BATTERY_SAVING_APP_YES_DOMAIN_PROMPT_LOCATION,
            UMA_PERM_BATTERY_SAVING_APP_YES_DOMAIN_PROMPT_NO_LOCATION,
            UMA_PERM_BATTERY_SAVING_APP_YES_DOMAIN_BLOCKED,
            UMA_PERM_BATTERY_SAVING_APP_PROMPT_DOMAIN_YES,
            UMA_PERM_BATTERY_SAVING_APP_PROMPT_DOMAIN_PROMPT,
            UMA_PERM_BATTERY_SAVING_APP_PROMPT_DOMAIN_BLOCKED,
            UMA_PERM_BATTERY_SAVING_APP_BLOCKED_DOMAIN_YES,
            UMA_PERM_BATTERY_SAVING_APP_BLOCKED_DOMAIN_PROMPT,
            UMA_PERM_BATTERY_SAVING_APP_BLOCKED_DOMAIN_BLOCKED,
            UMA_PERM_GPS_ONLY_APP_YES_DOMAIN_YES_LOCATION,
            UMA_PERM_GPS_ONLY_APP_YES_DOMAIN_YES_NO_LOCATION,
            UMA_PERM_GPS_ONLY_APP_YES_DOMAIN_PROMPT_LOCATION,
            UMA_PERM_GPS_ONLY_APP_YES_DOMAIN_PROMPT_NO_LOCATION,
            UMA_PERM_GPS_ONLY_APP_YES_DOMAIN_BLOCKED, UMA_PERM_GPS_ONLY_APP_PROMPT_DOMAIN_YES,
            UMA_PERM_GPS_ONLY_APP_PROMPT_DOMAIN_PROMPT, UMA_PERM_GPS_ONLY_APP_PROMPT_DOMAIN_BLOCKED,
            UMA_PERM_GPS_ONLY_APP_BLOCKED_DOMAIN_YES, UMA_PERM_GPS_ONLY_APP_BLOCKED_DOMAIN_PROMPT,
            UMA_PERM_GPS_ONLY_APP_BLOCKED_DOMAIN_BLOCKED, UMA_PERM_MASTER_OFF_APP_YES_DOMAIN_YES,
            UMA_PERM_MASTER_OFF_APP_YES_DOMAIN_PROMPT, UMA_PERM_MASTER_OFF_APP_YES_DOMAIN_BLOCKED,
            UMA_PERM_MASTER_OFF_APP_PROMPT_DOMAIN_YES, UMA_PERM_MASTER_OFF_APP_PROMPT_DOMAIN_PROMPT,
            UMA_PERM_MASTER_OFF_APP_PROMPT_DOMAIN_BLOCKED,
            UMA_PERM_MASTER_OFF_APP_BLOCKED_DOMAIN_YES,
            UMA_PERM_MASTER_OFF_APP_BLOCKED_DOMAIN_PROMPT,
            UMA_PERM_MASTER_OFF_APP_BLOCKED_DOMAIN_BLOCKED, UMA_PERM_UNKNOWN})
    public @interface UmaPermission {}

    private static final int LOCATION_SOURCE_HIGH_ACCURACY = 0;
    private static final int LOCATION_SOURCE_BATTERY_SAVING = 1;
    private static final int LOCATION_SOURCE_GPS_ONLY = 2;
    private static final int LOCATION_SOURCE_MASTER_OFF = 3;
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({LOCATION_SOURCE_HIGH_ACCURACY, LOCATION_SOURCE_BATTERY_SAVING,
            LOCATION_SOURCE_GPS_ONLY, LOCATION_SOURCE_MASTER_OFF})
    private @interface LocationSource {}

    private static final int PERMISSION_GRANTED = 0;
    private static final int PERMISSION_PROMPT = 1;
    private static final int PERMISSION_BLOCKED = 2;
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({PERMISSION_GRANTED, PERMISSION_PROMPT, PERMISSION_BLOCKED})
    private @interface Permission {}

    /** The maximum age in milliseconds of a location that we'll send in an X-Geo header. */
    private static final int MAX_LOCATION_AGE = 24 * 60 * 60 * 1000;  // 24 hours

    /** The maximum age in milliseconds of a location before we'll request a refresh. */
    private static final int REFRESH_LOCATION_AGE = 5 * 60 * 1000;  // 5 minutes

    private static final String HTTPS_SCHEME = "https";

    /**
     * Requests a location refresh so that a valid location will be available for constructing
     * an X-Geo header in the near future (i.e. within 5 minutes).
     *
     * @param context The Context used to get the device location.
     */
    public static void primeLocationForGeoHeader(Context context) {
        if (!hasGeolocationPermission(context)) return;

        GeolocationTracker.refreshLastKnownLocation(context, REFRESH_LOCATION_AGE);
    }

    /**
     * Returns whether the X-Geo header is allowed to be sent for the current URL.
     *
     * @param context The Context used to get the device location.
     * @param url The URL of the request with which this header will be sent.
     * @param isIncognito Whether the request will happen in an incognito tab.
     */
    public static boolean isGeoHeaderEnabledForUrl(Context context, String url,
            boolean isIncognito) {
        return isGeoHeaderEnabledForUrl(context, url, isIncognito, false);
    }

    private static boolean isGeoHeaderEnabledForUrl(Context context, String url,
            boolean isIncognito, boolean recordUma) {
        // Only send X-Geo in normal mode.
        if (isIncognito) return false;

        // Only send X-Geo header to Google domains.
        if (!UrlUtilities.nativeIsGoogleSearchUrl(url)) return false;

        Uri uri = Uri.parse(url);
        if (!HTTPS_SCHEME.equals(uri.getScheme())) return false;

        if (!hasGeolocationPermission(context)) {
            if (recordUma) recordHistogram(UMA_LOCATION_DISABLED_FOR_CHROME_APP);
            return false;
        }

        // Only send X-Geo header if the user hasn't disabled geolocation for url.
        if (isLocationDisabledForUrl(uri, isIncognito)) {
            if (recordUma) recordHistogram(UMA_LOCATION_DISABLED_FOR_GOOGLE_DOMAIN);
            return false;
        }

        return true;
    }

    /**
     * Returns an X-Geo HTTP header string if:
     *  1. The current mode is not incognito.
     *  2. The url is a google search URL (e.g. www.google.co.uk/search?q=cars), and
     *  3. The user has not disabled sharing location with this url, and
     *  4. There is a valid and recent location available.
     *
     * Returns null otherwise.
     *
     * @param context The Context used to get the device location.
     * @param url The URL of the request with which this header will be sent.
     * @param tab The Tab currently being accessed.
     * @return The X-Geo header string or null.
     */
    public static String getGeoHeader(Context context, String url, Tab tab) {
        boolean isIncognito = tab.isIncognito();
        boolean locationAttached = true;
        Location location = null;
        if (isGeoHeaderEnabledForUrl(context, url, isIncognito, true)) {
            // Only send X-Geo header if there's a fresh location available.
            location = GeolocationTracker.getLastKnownLocation(context);
            if (location == null) {
                recordHistogram(UMA_LOCATION_NOT_AVAILABLE);
                locationAttached = false;
            } else {
                if (GeolocationTracker.getLocationAge(location) > MAX_LOCATION_AGE) {
                    recordHistogram(UMA_LOCATION_STALE);
                    locationAttached = false;
                }
            }
        } else {
            locationAttached = false;
        }

        @LocationSource int locationSource = getLocationSource(context);
        @Permission int appPermission = getGeolocationPermission(context, tab);
        @Permission int domainPermission = getDomainPermission(url, isIncognito);

        // Record the permission state with a histogram.
        recordPermissionHistogram(
                locationSource, appPermission, domainPermission, locationAttached);

        // Note that strictly speaking "location == null" is not needed here as the
        // logic above prevents location being null when locationAttached is true.
        // It is here to prevent problems if the logic above is changed.
        if (!locationAttached || location == null) return null;

        recordHistogram(UMA_HEADER_SENT);

        // Timestamp in microseconds since the UNIX epoch.
        long timestamp = location.getTime() * 1000;
        // Latitude times 1e7.
        int latitude = (int) (location.getLatitude() * 10000000);
        // Longitude times 1e7.
        int longitude = (int) (location.getLongitude() * 10000000);
        // Radius of 68% accuracy in mm.
        int radius = (int) (location.getAccuracy() * 1000);

        // Encode location using ascii protobuf format followed by base64 encoding.
        // https://goto.google.com/partner_location_proto
        String locationAscii = String.format(Locale.US,
                "role:1 producer:12 timestamp:%d latlng{latitude_e7:%d longitude_e7:%d} radius:%d",
                timestamp, latitude, longitude, radius);
        String locationBase64 = new String(Base64.encode(locationAscii.getBytes(), Base64.NO_WRAP));

        return "X-Geo: a " + locationBase64;
    }

    @CalledByNative
    public static boolean hasGeolocationPermission() {
        Context context = ContextUtils.getApplicationContext();
        return hasGeolocationPermission(context);
    }

    static boolean hasGeolocationPermission(Context context) {
        int pid = Process.myPid();
        int uid = Process.myUid();
        if (ApiCompatibilityUtils.checkPermission(
                context, Manifest.permission.ACCESS_COARSE_LOCATION, pid, uid)
                != PackageManager.PERMISSION_GRANTED) {
            return false;
        }

        // Work around a bug in OnePlus2 devices running Lollipop, where the NETWORK_PROVIDER
        // incorrectly requires FINE_LOCATION permission (it should only require COARSE_LOCATION
        // permission). http://crbug.com/580733
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M
                && ApiCompatibilityUtils.checkPermission(
                        context, Manifest.permission.ACCESS_FINE_LOCATION, pid, uid)
                        != PackageManager.PERMISSION_GRANTED) {
            return false;
        }

        return true;
    }

    /**
     * Returns the app level geolocation permission.
     * This permission can be either granted, blocked or prompt.
     */
    @Permission
    static int getGeolocationPermission(Context context, Tab tab) {
        if (hasGeolocationPermission(context)) return PERMISSION_GRANTED;
        return tab.getWindowAndroid().canRequestPermission(
                       Manifest.permission.ACCESS_COARSE_LOCATION)
                ? PERMISSION_PROMPT
                : PERMISSION_BLOCKED;
    }

    /**
     * Returns true if the user has disabled sharing their location with url (e.g. via the
     * geolocation infobar). If the user has not chosen a preference for url and url uses the https
     * scheme, this considers the user's preference for url with the http scheme instead.
     */
    static boolean isLocationDisabledForUrl(Uri uri, boolean isIncognito) {
        return locationContentSettingForUrl(uri, isIncognito) == ContentSetting.BLOCK;
    }

    /**
     * Returns the location permission for sharing their location with url (e.g. via the
     * geolocation infobar). If the user has not chosen a preference for url and url uses the https
     * scheme, this returns the user's preference for url with the http scheme instead.
     */
    static ContentSetting locationContentSettingForUrl(Uri uri, boolean isIncognito) {
        GeolocationInfo locationSettings = new GeolocationInfo(uri.toString(), null, isIncognito);
        ContentSetting locationPermission = locationSettings.getContentSetting();

        // If no preference has been chosen and the scheme is https, fall back to the preference for
        // this same host over http with no explicit port number.
        if (locationPermission == null || locationPermission == ContentSetting.ASK) {
            String scheme = uri.getScheme();
            if (scheme != null && scheme.toLowerCase(Locale.US).equals("https")
                    && uri.getAuthority() != null && uri.getUserInfo() == null) {
                String urlWithHttp = "http://" + uri.getHost();
                locationSettings = new GeolocationInfo(urlWithHttp, null, isIncognito);
                locationPermission = locationSettings.getContentSetting();
            }
        }

        return locationPermission;
    }

    /** Records a data point for the Geolocation.HeaderSentOrNot histogram. */
    private static void recordHistogram(int result) {
        RecordHistogram.recordEnumeratedHistogram("Geolocation.HeaderSentOrNot", result, UMA_MAX);
    }

    /** Returns the location source. */
    @LocationSource
    private static int getLocationSource(Context context) {
        LocationManager locationManager =
                (LocationManager) context.getSystemService(Context.LOCATION_SERVICE);
        List<String> providers = locationManager.getProviders(true);
        boolean hasNetworkProvider = false;
        boolean hasGpsProvider = false;
        for (String provider : providers) {
            if (LocationManager.NETWORK_PROVIDER.equals(provider)) {
                hasNetworkProvider = true;
            } else if (LocationManager.GPS_PROVIDER.equals(provider)) {
                hasGpsProvider = true;
            }
        }
        return hasNetworkProvider
                ? (hasGpsProvider ? LOCATION_SOURCE_HIGH_ACCURACY : LOCATION_SOURCE_BATTERY_SAVING)
                : (hasGpsProvider ? LOCATION_SOURCE_GPS_ONLY : LOCATION_SOURCE_MASTER_OFF);
    }

    /**
     * Returns the domain permission as either granted, blocked or prompt.
     * This is based upon the location permission for sharing their location with url (e.g. via the
     * geolocation infobar). If the user has not chosen a preference for url and url uses the https
     * scheme, this returns the user's preference for url with the http scheme instead.
     */
    @Permission
    private static int getDomainPermission(String url, boolean isIncognito) {
        ContentSetting domainPermission = locationContentSettingForUrl(Uri.parse(url), isIncognito);
        switch (domainPermission) {
            case ALLOW:
                return PERMISSION_GRANTED;
            case ASK:
                return PERMISSION_PROMPT;
            default:
                return PERMISSION_BLOCKED;
        }
    }

    /**
     * Returns the enum to use in the Geolocation.Header.PermissionState histogram.
     * Unexpected input values return UMA_PERM_UNKNOWN.
     */
    @UmaPermission
    private static int getPermissionHistogramEnum(@LocationSource int locationSource,
            @Permission int appPermission, @Permission int domainPermission,
            boolean locationAttached) {
        if (locationSource == LOCATION_SOURCE_HIGH_ACCURACY) {
            if (appPermission == PERMISSION_GRANTED) {
                if (domainPermission == PERMISSION_GRANTED) {
                    return locationAttached ? UMA_PERM_HIGH_ACCURACY_APP_YES_DOMAIN_YES_LOCATION
                                            : UMA_PERM_HIGH_ACCURACY_APP_YES_DOMAIN_YES_NO_LOCATION;
                } else if (domainPermission == PERMISSION_PROMPT) {
                    return locationAttached
                            ? UMA_PERM_HIGH_ACCURACY_APP_YES_DOMAIN_PROMPT_LOCATION
                            : UMA_PERM_HIGH_ACCURACY_APP_YES_DOMAIN_PROMPT_NO_LOCATION;
                } else if (domainPermission == PERMISSION_BLOCKED) {
                    return UMA_PERM_HIGH_ACCURACY_APP_YES_DOMAIN_BLOCKED;
                }
            } else if (appPermission == PERMISSION_PROMPT) {
                if (domainPermission == PERMISSION_GRANTED) {
                    return UMA_PERM_HIGH_ACCURACY_APP_PROMPT_DOMAIN_YES;
                } else if (domainPermission == PERMISSION_PROMPT) {
                    return UMA_PERM_HIGH_ACCURACY_APP_PROMPT_DOMAIN_PROMPT;
                } else if (domainPermission == PERMISSION_BLOCKED) {
                    return UMA_PERM_HIGH_ACCURACY_APP_PROMPT_DOMAIN_BLOCKED;
                }
            } else if (appPermission == PERMISSION_BLOCKED) {
                if (domainPermission == PERMISSION_GRANTED) {
                    return UMA_PERM_HIGH_ACCURACY_APP_BLOCKED_DOMAIN_YES;
                } else if (domainPermission == PERMISSION_PROMPT) {
                    return UMA_PERM_HIGH_ACCURACY_APP_BLOCKED_DOMAIN_PROMPT;
                } else if (domainPermission == PERMISSION_BLOCKED) {
                    return UMA_PERM_HIGH_ACCURACY_APP_BLOCKED_DOMAIN_BLOCKED;
                }
            }
        } else if (locationSource == LOCATION_SOURCE_BATTERY_SAVING) {
            if (appPermission == PERMISSION_GRANTED) {
                if (domainPermission == PERMISSION_GRANTED) {
                    return locationAttached
                            ? UMA_PERM_BATTERY_SAVING_APP_YES_DOMAIN_YES_LOCATION
                            : UMA_PERM_BATTERY_SAVING_APP_YES_DOMAIN_YES_NO_LOCATION;
                } else if (domainPermission == PERMISSION_PROMPT) {
                    return locationAttached
                            ? UMA_PERM_BATTERY_SAVING_APP_YES_DOMAIN_PROMPT_LOCATION
                            : UMA_PERM_BATTERY_SAVING_APP_YES_DOMAIN_PROMPT_NO_LOCATION;
                } else if (domainPermission == PERMISSION_BLOCKED) {
                    return UMA_PERM_BATTERY_SAVING_APP_YES_DOMAIN_BLOCKED;
                }
            } else if (appPermission == PERMISSION_PROMPT) {
                if (domainPermission == PERMISSION_GRANTED) {
                    return UMA_PERM_BATTERY_SAVING_APP_PROMPT_DOMAIN_YES;
                } else if (domainPermission == PERMISSION_PROMPT) {
                    return UMA_PERM_BATTERY_SAVING_APP_PROMPT_DOMAIN_PROMPT;
                } else if (domainPermission == PERMISSION_BLOCKED) {
                    return UMA_PERM_BATTERY_SAVING_APP_PROMPT_DOMAIN_BLOCKED;
                }
            } else if (appPermission == PERMISSION_BLOCKED) {
                if (domainPermission == PERMISSION_GRANTED) {
                    return UMA_PERM_BATTERY_SAVING_APP_BLOCKED_DOMAIN_YES;
                } else if (domainPermission == PERMISSION_PROMPT) {
                    return UMA_PERM_BATTERY_SAVING_APP_BLOCKED_DOMAIN_PROMPT;
                } else if (domainPermission == PERMISSION_BLOCKED) {
                    return UMA_PERM_BATTERY_SAVING_APP_BLOCKED_DOMAIN_BLOCKED;
                }
            }
        } else if (locationSource == LOCATION_SOURCE_GPS_ONLY) {
            if (appPermission == PERMISSION_GRANTED) {
                if (domainPermission == PERMISSION_GRANTED) {
                    return locationAttached ? UMA_PERM_GPS_ONLY_APP_YES_DOMAIN_YES_LOCATION
                                            : UMA_PERM_GPS_ONLY_APP_YES_DOMAIN_YES_NO_LOCATION;
                } else if (domainPermission == PERMISSION_PROMPT) {
                    return locationAttached ? UMA_PERM_GPS_ONLY_APP_YES_DOMAIN_PROMPT_LOCATION
                                            : UMA_PERM_GPS_ONLY_APP_YES_DOMAIN_PROMPT_NO_LOCATION;
                } else if (domainPermission == PERMISSION_BLOCKED) {
                    return UMA_PERM_GPS_ONLY_APP_YES_DOMAIN_BLOCKED;
                }
            } else if (appPermission == PERMISSION_PROMPT) {
                if (domainPermission == PERMISSION_GRANTED) {
                    return UMA_PERM_GPS_ONLY_APP_PROMPT_DOMAIN_YES;
                } else if (domainPermission == PERMISSION_PROMPT) {
                    return UMA_PERM_GPS_ONLY_APP_PROMPT_DOMAIN_PROMPT;
                } else if (domainPermission == PERMISSION_BLOCKED) {
                    return UMA_PERM_GPS_ONLY_APP_PROMPT_DOMAIN_BLOCKED;
                }
            } else if (appPermission == PERMISSION_BLOCKED) {
                if (domainPermission == PERMISSION_GRANTED) {
                    return UMA_PERM_GPS_ONLY_APP_BLOCKED_DOMAIN_YES;
                } else if (domainPermission == PERMISSION_PROMPT) {
                    return UMA_PERM_GPS_ONLY_APP_BLOCKED_DOMAIN_PROMPT;
                } else if (domainPermission == PERMISSION_BLOCKED) {
                    return UMA_PERM_GPS_ONLY_APP_BLOCKED_DOMAIN_BLOCKED;
                }
            }
        } else if (locationSource == LOCATION_SOURCE_MASTER_OFF) {
            if (appPermission == PERMISSION_GRANTED) {
                if (domainPermission == PERMISSION_GRANTED) {
                    return UMA_PERM_MASTER_OFF_APP_YES_DOMAIN_YES;
                } else if (domainPermission == PERMISSION_PROMPT) {
                    return UMA_PERM_MASTER_OFF_APP_YES_DOMAIN_PROMPT;
                } else if (domainPermission == PERMISSION_BLOCKED) {
                    return UMA_PERM_MASTER_OFF_APP_YES_DOMAIN_BLOCKED;
                }
            } else if (appPermission == PERMISSION_PROMPT) {
                if (domainPermission == PERMISSION_GRANTED) {
                    return UMA_PERM_MASTER_OFF_APP_PROMPT_DOMAIN_YES;
                } else if (domainPermission == PERMISSION_PROMPT) {
                    return UMA_PERM_MASTER_OFF_APP_PROMPT_DOMAIN_PROMPT;
                } else if (domainPermission == PERMISSION_BLOCKED) {
                    return UMA_PERM_MASTER_OFF_APP_PROMPT_DOMAIN_BLOCKED;
                }
            } else if (appPermission == PERMISSION_BLOCKED) {
                if (domainPermission == PERMISSION_GRANTED) {
                    return UMA_PERM_MASTER_OFF_APP_BLOCKED_DOMAIN_YES;
                } else if (domainPermission == PERMISSION_PROMPT) {
                    return UMA_PERM_MASTER_OFF_APP_BLOCKED_DOMAIN_PROMPT;
                } else if (domainPermission == PERMISSION_BLOCKED) {
                    return UMA_PERM_MASTER_OFF_APP_BLOCKED_DOMAIN_BLOCKED;
                }
            }
        }
        return UMA_PERM_UNKNOWN;
    }

    /** Records a data point for the Geolocation.Header.PermissionState histogram. */
    private static void recordPermissionHistogram(@LocationSource int locationSource,
            @Permission int appPermission, @Permission int domainPermission,
            boolean locationAttached) {
        @UmaPermission
        int result = getPermissionHistogramEnum(
                locationSource, appPermission, domainPermission, locationAttached);
        RecordHistogram.recordEnumeratedHistogram(
                "Geolocation.Header.PermissionState", result, UMA_PERM_COUNT);
    }
}
