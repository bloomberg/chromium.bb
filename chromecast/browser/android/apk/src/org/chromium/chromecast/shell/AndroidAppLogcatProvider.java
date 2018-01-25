// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import org.chromium.base.Log;
import org.chromium.chromecast.base.CircularBuffer;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.Charset;
import java.nio.charset.UnsupportedCharsetException;

/**
 * Extracts logcat out of Android devices and elide PII sensitive info from it.
 *
 * <p>Elided information includes: Emails, IP address, MAC address, URL/domains as well as
 * Javascript console messages.
 */
class AndroidAppLogcatProvider extends ElidedLogcatProvider {
    private static final String TAG = "LogcatProvider";
    private static final long HALF_SECOND = 500;

    @Override
    protected void getRawLogcat(RawLogcatCallback callback) {
        CircularBuffer<String> rawLogcat = new CircularBuffer<>(BuildConfig.LOGCAT_SIZE);
        String logLn = null;
        Integer exitValue = null;

        try {
            Process p = Runtime.getRuntime().exec("logcat -d");
            try (BufferedReader bReader = new BufferedReader(
                         new InputStreamReader(p.getInputStream(), Charset.forName("UTF-8")))) {
                while (exitValue == null) {
                    while ((logLn = bReader.readLine()) != null) {
                        // Add each new string to the end of the buffer.
                        rawLogcat.add(logLn);
                    }
                    try {
                        exitValue = p.exitValue();
                    } catch (IllegalThreadStateException itse) {
                        Thread.sleep(HALF_SECOND);
                    }
                }
                if (exitValue != 0) {
                    String msg = "Logcat process exit value: " + exitValue;
                    Log.w(TAG, msg);
                }
            } catch (UnsupportedCharsetException e) {
                // Should never happen; all Java implementations are required to support UTF-8.
                Log.wtf(TAG, "UTF-8 not supported", e);
            } catch (InterruptedException e) {
                Log.e(TAG, "Logcat subprocess interrupted ", e);
            }
        } catch (IOException e) {
            Log.e(TAG, "Error occurred trying to upload crash dump", e);
        } finally {
            callback.onLogsDone(rawLogcat);
        }
    }
}
