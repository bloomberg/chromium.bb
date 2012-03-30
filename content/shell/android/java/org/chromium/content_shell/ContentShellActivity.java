// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_shell;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;

/**
 * This is the main activity for content_shell on android, gets invoked when the
 * app is invoked from the launcher. This activity hosts the main UI for content
 * shell and displays the web content via its child views.
 */
public class ContentShellActivity extends Activity {
    private final String TAG = "ContentShellActivity";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Log.i(TAG, "Content shell started");
    }
}
