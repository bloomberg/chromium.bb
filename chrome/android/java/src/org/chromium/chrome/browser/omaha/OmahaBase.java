// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Build;

import org.chromium.base.StreamUtil;
import org.chromium.chrome.browser.ChromeVersionInfo;

import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.net.HttpURLConnection;

/**
 * Logic used for communicating with the Omaha server.
 * TODO(dfalcantara): Move everything from OmahaClient over.
 */
public abstract class OmahaBase {

    // Flags for retrieving the OmahaClient's state after it's written to disk.
    // The PREF_PACKAGE doesn't match the current OmahaClient package for historical reasons.
    static final String PREF_PACKAGE = "com.google.android.apps.chrome.omaha";
    static final String PREF_INSTALL_SOURCE = "installSource";
    static final String PREF_LATEST_VERSION = "latestVersion";
    static final String PREF_MARKET_URL = "marketURL";
    static final String PREF_PERSISTED_REQUEST_ID = "persistedRequestID";
    static final String PREF_SEND_INSTALL_EVENT = "sendInstallEvent";
    static final String PREF_TIMESTAMP_FOR_NEW_REQUEST = "timestampForNewRequest";
    static final String PREF_TIMESTAMP_FOR_NEXT_POST_ATTEMPT = "timestampForNextPostAttempt";
    static final String PREF_TIMESTAMP_OF_INSTALL = "timestampOfInstall";
    static final String PREF_TIMESTAMP_OF_REQUEST = "timestampOfRequest";

    /** Whether or not the Omaha server should really be contacted. */
    private static boolean sIsDisabled;

    /** See {@link #sIsDisabled}. */
    public static void setIsDisabledForTesting(boolean state) {
        sIsDisabled = state;
    }

    /** See {@link #sIsDisabled}. */
    static boolean isDisabled() {
        return sIsDisabled;
    }

    /** Begin communicating with the Omaha Update Server. */
    public static void onForegroundSessionStart(Context context) {
        if (!ChromeVersionInfo.isOfficialBuild() || isDisabled()) return;

        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.N) {
            OmahaClient.startService(context);
        }
    }

    /** Checks whether Chrome has ever tried contacting Omaha before. */
    public static boolean isProbablyFreshInstall(Context context) {
        SharedPreferences prefs = getSharedPreferences(context);
        return prefs.getLong(PREF_TIMESTAMP_OF_INSTALL, -1) == -1;
    }

    /** Sends the request to the server and returns the response. */
    static String sendRequestToServer(HttpURLConnection urlConnection, String request)
            throws RequestFailureException {
        try {
            OutputStream out = new BufferedOutputStream(urlConnection.getOutputStream());
            OutputStreamWriter writer = new OutputStreamWriter(out);
            writer.write(request, 0, request.length());
            StreamUtil.closeQuietly(writer);
            checkServerResponseCode(urlConnection);
        } catch (IOException e) {
            throw new RequestFailureException("Failed to write request to server: ", e);
        }

        try {
            InputStreamReader reader = new InputStreamReader(urlConnection.getInputStream());
            BufferedReader in = new BufferedReader(reader);
            try {
                StringBuilder response = new StringBuilder();
                for (String line = in.readLine(); line != null; line = in.readLine()) {
                    response.append(line);
                }
                checkServerResponseCode(urlConnection);
                return response.toString();
            } finally {
                StreamUtil.closeQuietly(in);
            }
        } catch (IOException e) {
            throw new RequestFailureException("Failed when reading response from server: ", e);
        }
    }

    /** Confirms that the Omaha server sent back an "OK" code. */
    private static void checkServerResponseCode(HttpURLConnection urlConnection)
            throws RequestFailureException {
        try {
            if (urlConnection.getResponseCode() != 200) {
                throw new RequestFailureException(
                        "Received " + urlConnection.getResponseCode()
                        + " code instead of 200 (OK) from the server.  Aborting.");
            }
        } catch (IOException e) {
            throw new RequestFailureException("Failed to read response code from server: ", e);
        }
    }

    /** Returns the Omaha SharedPreferences. */
    static SharedPreferences getSharedPreferences(Context context) {
        return context.getSharedPreferences(PREF_PACKAGE, Context.MODE_PRIVATE);
    }
}
