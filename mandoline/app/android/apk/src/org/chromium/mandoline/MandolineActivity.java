// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mandoline;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

import org.chromium.base.Log;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.mojo.shell.ShellMain;

/**
 * Activity hosting the Mojo View Manager for Mandoline.
 */
@JNINamespace("mandoline")
public class MandolineActivity extends Activity {
    private static final String TAG = "cr.MandolineActivity";
    private static final String EXTRAS = "org.chromium.mandoline.extras";

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        String[] parameters = getIntent().getStringArrayExtra(EXTRAS);
        if (parameters != null) {
            for (String s : parameters) {
                s = s.replace("\\,", ",");
            }
        }
        if (Intent.ACTION_VIEW.equals(getIntent().getAction())) {
            Uri uri = getIntent().getData();
            if (uri != null) {
                Log.i(TAG, "MandolineActivity opening " + uri);
                if (parameters == null) {
                    parameters = new String[] {uri.toString()};
                } else {
                    String[] newParameters = new String[parameters.length + 1];
                    System.arraycopy(parameters, 0, newParameters, 0, parameters.length);
                    newParameters[parameters.length] = uri.toString();
                    parameters = newParameters;
                }
            }
        }

        ShellMain.ensureInitialized(this, parameters);
        ShellMain.start();
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);

        if (Intent.ACTION_VIEW.equals(intent.getAction())) {
            Uri uri = intent.getData();
            if (uri != null) {
                Log.i(TAG, "MandolineActivity launching new intent for uri: " + uri);
                nativeLaunchURL(uri.toString());
            }
        }
    }

    private static native void nativeLaunchURL(String url);
}
