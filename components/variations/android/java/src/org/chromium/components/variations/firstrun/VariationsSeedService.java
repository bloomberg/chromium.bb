// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.variations.firstrun;

import android.app.IntentService;
import android.content.Intent;

import org.chromium.base.Log;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;

/**
 * Background service that fetches the variations seed before the actual first run of Chrome.
 */
public class VariationsSeedService extends IntentService {
    private static final String TAG = "VariationsSeedServ";
    private static final String VARIATIONS_SERVER_URL =
            "https://clients4.google.com/chrome-variations/seed?osname=android";
    private static final int BUFFER_SIZE = 4096;
    private static final int READ_TIMEOUT = 10000; // time in ms
    private static final int REQUEST_TIMEOUT = 15000; // time in ms

    public VariationsSeedService() {
        super(TAG);
    }

    @Override
    public void onHandleIntent(Intent intent) {
        try {
            downloadContent(new URL(VARIATIONS_SERVER_URL));
        } catch (MalformedURLException e) {
            Log.w(TAG, "Variations server URL is malformed.", e);
        }
    }

    private boolean downloadContent(URL variationsServerUrl) {
        HttpURLConnection connection = null;
        try {
            connection = (HttpURLConnection) variationsServerUrl.openConnection();
            connection.setReadTimeout(READ_TIMEOUT);
            connection.setConnectTimeout(REQUEST_TIMEOUT);
            connection.setDoInput(true);
            // TODO(agulenko): add gzip compression support.
            // connection.setRequestProperty("A-IM", "gzip");
            connection.connect();
            int responseCode = connection.getResponseCode();
            if (responseCode != HttpURLConnection.HTTP_OK) {
                Log.w(TAG, "Non-OK response code = %d", responseCode);
                return false;
            }

            // Convert the InputStream into a byte array.
            byte[] rawSeed = getRawSeed(connection);
            String signature = connection.getHeaderField("X-Seed-Signature");
            String country = connection.getHeaderField("X-Country");
            VariationsSeedBridge.setVariationsFirstRunSeed(
                    getApplicationContext(), rawSeed, signature, country);
            return true;
        } catch (IOException e) {
            Log.w(TAG, "IOException fetching first run seed: ", e);
            return false;
        } finally {
            if (connection != null) {
                connection.disconnect();
            }
        }
    }

    private byte[] getRawSeed(HttpURLConnection connection) throws IOException {
        InputStream inputStream = null;
        try {
            inputStream = connection.getInputStream();
            return convertInputStreamToByteArray(inputStream);
        } finally {
            if (inputStream != null) {
                inputStream.close();
            }
        }
    }

    private byte[] convertInputStreamToByteArray(InputStream inputStream) throws IOException {
        ByteArrayOutputStream byteBuffer = new ByteArrayOutputStream();
        byte[] buffer = new byte[BUFFER_SIZE];
        int charactersReadCount = 0;
        while ((charactersReadCount = inputStream.read(buffer)) != -1) {
            byteBuffer.write(buffer, 0, charactersReadCount);
        }
        return byteBuffer.toByteArray();
    }
}