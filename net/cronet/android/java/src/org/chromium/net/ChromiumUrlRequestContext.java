// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.util.Log;

/**
 * Provides context for the native HTTP operations.
 */
public class ChromiumUrlRequestContext extends UrlRequestContext {
    private static final Object sLock = new Object();

    private static final String TAG = "ChromiumNetwork";

    private static ChromiumUrlRequestContext sInstance;

    private ChromiumUrlRequestContext(Context context, String userAgent,
            int loggingLevel) {
        super(context, userAgent, loggingLevel);
    }

    public static ChromiumUrlRequestContext getInstance(
            Context context) {
        synchronized (sLock) {
            if (sInstance == null) {
                int loggingLevel;
                if (Log.isLoggable(TAG, Log.VERBOSE)) {
                    loggingLevel = LOG_VERBOSE;
                } else if (Log.isLoggable(TAG, Log.DEBUG)) {
                    loggingLevel = LOG_DEBUG;
                } else {
                    loggingLevel = LOG_NONE;
                }
                sInstance = new ChromiumUrlRequestContext(
                        context.getApplicationContext(),
                        UserAgent.from(context),
                        loggingLevel);
            }
        }
        return sInstance;
    }
}
