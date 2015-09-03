// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp;

import android.app.Activity;
import android.os.Bundle;

import org.chromium.base.Log;
import org.chromium.base.library_loader.ProcessInitException;

/**
 * The {@link Activity} for rendering the main Blimp client.  This loads the Blimp rendering stack
 * and displays it.
 */
public class BlimpRendererActivity extends Activity implements BlimpLibraryLoader.Callback {
    private static final String TAG = "cr.Blimp";
    private BlimpView mBlimpView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        try {
            BlimpLibraryLoader.startAsync(this, this);
        } catch (ProcessInitException e) {
            Log.e(TAG, "Native startup exception", e);
            System.exit(-1);
            return;
        }
    }

    @Override
    protected void onDestroy() {
        if (mBlimpView != null) {
            mBlimpView.destroyRenderer();
            mBlimpView = null;
        }

        super.onDestroy();
    }

    // BlimpLibraryLoader.Callback implementation.
    @Override
    public void onStartupComplete(boolean success) {
        if (!success) {
            Log.e(TAG, "Native startup failed");
            finish();
            return;
        }

        setContentView(R.layout.blimp_main);
        mBlimpView = (BlimpView) findViewById(R.id.renderer);
        mBlimpView.initializeRenderer();
    }
}
