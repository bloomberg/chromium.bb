// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.native_test;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;

// Android's NativeActivity is mostly useful for pure-native code.
// Our tests need to go up to our own java classes, which is not possible using
// the native activity class loader.
// We start a background thread in here to run the tests and avoid an ANR.
// TODO(bulach): watch out for tests that implicitly assume they run on the main
// thread.
public class ChromeNativeTestActivity extends Activity {
    private final String TAG = "ChromeNativeTestActivity";
    private final String LIBRARY = "native_tests";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        try {
            loadLibrary();
            new Thread() {
                @Override
                public void run() {
                    Log.d(TAG, ">>nativeRunTests");
                    nativeRunTests(getFilesDir().getAbsolutePath());
                    Log.d(TAG, "<<nativeRunTests");
                }
            }.start();
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "Unable to load libnative_tests.so: " + e);
            throw e;
        }
    }

    private void loadLibrary() throws UnsatisfiedLinkError {
        Log.i(TAG, "loading: " + LIBRARY);
        System.loadLibrary(LIBRARY);
        Log.i(TAG, "loaded: " + LIBRARY);
    }

    private native void nativeRunTests(String filesDir);
}
