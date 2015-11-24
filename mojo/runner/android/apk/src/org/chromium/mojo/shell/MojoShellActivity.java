// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.shell;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

import org.chromium.base.Log;

/**
 * Activity for managing the Mojo Shell.
 */
public class MojoShellActivity extends Activity {
    private static final String TAG = "MojoShellActivity";
    private static final String EXTRAS = "org.chromium.mojo.shell.extras";

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
                Log.i(TAG, "MojoShellActivity opening " + uri);
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

        // TODO(ppi): Gotcha - the call below will work only once per process lifetime, but the OS
        // has no obligation to kill the application process between destroying and restarting the
        // activity. If the application process is kept alive, initialization parameters sent with
        // the intent will be stale.
        ShellMain.ensureInitialized(this, parameters);
        ShellMain.start();
    }
}
