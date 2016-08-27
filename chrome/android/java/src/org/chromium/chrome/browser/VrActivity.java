// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.graphics.PixelFormat;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;

import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.vr_shell.VrShell;

/**
 * A subclass of AsyncInitializationActivity, used in Daydream VR mode.
 * TODO(bshe): This activity needs to access the same set of tabs as ChromeTabbedActivity.
 * See more detail in crbug.com/641038.
 *
 */
public class VrActivity extends AsyncInitializationActivity {
    private VrShell mVrShellView;

    @Override
    public void onResume() {
        super.onResume();
        mVrShellView.onResume();
    }

    @Override
    public void onPause() {
        super.onPause();
        mVrShellView.onPause();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (mVrShellView != null) {
            mVrShellView.shutdown();
            mVrShellView = null;
        }
    }

    @Override
    public void initializeCompositor() {
        super.initializeCompositor();
        mVrShellView.onNativeLibraryReady();
    }

    @Override
    public void setContentView() {
        setupVrModeWindowFlags();
        addVrViews();
    }

    private void addVrViews() {
        mVrShellView = new VrShell(this);
        WindowManager.LayoutParams params = new WindowManager.LayoutParams(
                WindowManager.LayoutParams.MATCH_PARENT, WindowManager.LayoutParams.MATCH_PARENT,
                WindowManager.LayoutParams.TYPE_APPLICATION, 0, PixelFormat.OPAQUE);
        setContentView(mVrShellView, params);
    }

    private void setupVrModeWindowFlags() {
        Window window = getWindow();
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        window.getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
    }
}
