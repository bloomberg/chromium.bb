// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
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
class LogcatExtractor {
    private static final String TAG = "LogcatExtractor";
    private static final long HALF_SECOND = 500;

    public static String getElidedLogcat() throws IOException, InterruptedException {
        return getElidedLogcat(getLogcat());
    }

    @VisibleForTesting
    static String getElidedLogcat(Iterable<String> rawLogcat) {
        StringBuilder builder = new StringBuilder();
        for (String line : rawLogcat) {
            builder.append(LogcatElision.elide(line));
            builder.append("\n");
        }
        return builder.toString();
    }

    private static Iterable<String> getLogcat() throws IOException, InterruptedException {
        CircularBuffer<String> rawLogcat = new CircularBuffer<>(BuildConfig.LOGCAT_SIZE);
        String logLn = null;
        Integer exitValue = null;

        Process p = Runtime.getRuntime().exec("logcat -d");
        BufferedReader bReader = null;
        try {
            bReader = new BufferedReader(
                    new InputStreamReader(p.getInputStream(), Charset.forName("UTF-8")));
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
                String msg = "Logcat failed: " + exitValue;
                Log.w(TAG, msg);
                throw new IOException(msg);
            }
            return rawLogcat;
        } catch (UnsupportedCharsetException e) {
            // Should never happen; all Java implementations are required to support UTF-8.
            Log.wtf(TAG, "UTF-8 not supported", e);
            return rawLogcat;
        } finally {
            if (bReader != null) {
                bReader.close();
            }
        }
    }
}
