// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.multidex;

import android.content.Context;
import android.os.Build;
import android.os.Process;
import android.support.multidex.MultiDex;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;

import java.lang.reflect.InvocationTargetException;

/**
 *  Performs multidex installation for non-isolated processes.
 */
public class ChromiumMultiDex {

    private static final String TAG = "base_multidex";

    /**
     *  Installs secondary dexes if possible.
     *
     *  Isolated processes (e.g. renderer processes) can't load secondary dex files on
     *  K and below, so we don't even try in that case.
     *
     *  @param context The application context.
     */
    @VisibleForTesting
    public static void install(Context context) {
        try {
            // TODO(jbudorick): Back out this version check once support for K & below works.
            // http://crbug.com/512357
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP && processIsIsolated()) {
                Log.i(TAG, "Skipping multidex installation: inside isolated process.");
            } else {
                MultiDex.install(context);
                Log.i(TAG, "Completed multidex installation.");
            }
        } catch (NoSuchMethodException e) {
            Log.wtf(TAG, "Failed multidex installation", e);
        } catch (IllegalAccessException e) {
            Log.wtf(TAG, "Failed multidex installation", e);
        } catch (InvocationTargetException e) {
            Log.wtf(TAG, "Failed multidex installation", e);
        }
    }

    // Calls Process.isIsolated, a private Android API.
    private static boolean processIsIsolated()
            throws NoSuchMethodException, IllegalAccessException, InvocationTargetException {
        return (boolean) Process.class.getMethod("isIsolated").invoke(null);
    }

}
