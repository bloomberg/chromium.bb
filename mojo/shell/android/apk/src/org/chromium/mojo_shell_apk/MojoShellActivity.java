// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo_shell_apk;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.widget.EditText;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.ProcessInitException;

/**
 * Activity for managing the Mojo Shell.
 */
public class MojoShellActivity extends Activity {
    private static final String TAG = "MojoShellActivity";

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        try {
            LibraryLoader.ensureInitialized();
        } catch (ProcessInitException e) {
            Log.e(TAG, "libmojo_shell initialization failed.", e);
            finish();
            return;
        }

        MojoMain.ensureInitialized(this);

        String appUrl = getUrlFromIntent(getIntent());
        if (appUrl == null) {
            Log.i(TAG, "No URL provided via intent, prompting user...");
            AlertDialog.Builder alert = new AlertDialog.Builder(this);
            alert.setTitle("Enter a URL");
            alert.setMessage("Enter a URL");
            final EditText input = new EditText(this);
            alert.setView(input);
            alert.setPositiveButton("Load", new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int button) {
                    String url = input.getText().toString();
                    startWithURL(url);
                }
            });
            alert.show();
        } else {
            startWithURL(appUrl);
        }
    }

    private static String getUrlFromIntent(Intent intent) {
        return intent != null ? intent.getDataString() : null;
    }

    private void startWithURL(String url) {
        MojoMain.start(url);
        Log.i(TAG, "Mojo started: " + url);
    }
}
