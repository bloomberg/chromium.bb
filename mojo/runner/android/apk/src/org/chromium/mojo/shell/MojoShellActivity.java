// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.shell;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.JsonReader;

import org.chromium.base.Log;

import java.io.IOException;
import java.io.StringReader;
import java.util.ArrayList;
import java.util.List;

/**
 * Activity for managing the Mojo Shell.
 */
public class MojoShellActivity extends Activity {
    private static final String TAG = "MojoShellActivity";

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        String[] parameters = getParametersFromIntent(getIntent());
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
        // TODO(qsr): We should be passing application context here as required by
        // InitApplicationContext on the native side. Currently we can't, as PlatformViewportAndroid
        // relies on this being the activity context.
        ShellMain.ensureInitialized(this, parameters);

        // TODO(eseidel): ShellMain can fail, but we're ignoring the return.
        ShellMain.start();
    }

    private static String[] getParametersFromIntent(Intent intent) {
        if (intent == null) {
            return null;
        }
        String[] parameters = intent.getStringArrayExtra("parameters");
        if (parameters != null) {
            return parameters;
        }
        String encodedParameters = intent.getStringExtra("encodedParameters");
        if (encodedParameters != null) {
            JsonReader reader = new JsonReader(new StringReader(encodedParameters));
            List<String> parametersList = new ArrayList<String>();
            try {
                reader.beginArray();
                while (reader.hasNext()) {
                    parametersList.add(reader.nextString());
                }
                reader.endArray();
                reader.close();
                return parametersList.toArray(new String[parametersList.size()]);
            } catch (IOException e) {
                Log.w(TAG, e.getMessage(), e);
            }
        }
        return null;
    }
}
