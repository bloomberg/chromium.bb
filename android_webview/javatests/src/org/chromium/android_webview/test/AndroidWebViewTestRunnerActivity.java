// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.app.Activity;
import android.content.ContentUris;
import android.content.res.Configuration;
import android.os.Bundle;
import android.util.Log;
import android.view.ViewGroup.LayoutParams;
import android.view.WindowManager;
import android.widget.LinearLayout;

import org.chromium.content.browser.ContentView;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/*
 * This is a lightweight activity for tests that only require ContentView functionality.
 */
public class AndroidWebViewTestRunnerActivity extends Activity {

    private LinearLayout mLinearLayout;
    private List<ContentView> mContentViews = new ArrayList<ContentView>();

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // TODO(joth): When SW-renderer is available, we'll want to enable this on a per-test
        // basis.
        // BUG=http://b/5996811
        boolean hardwareAccelerated = true;
        Log.i("AndroidWebViewTestRunnerActivity", "Is " + (hardwareAccelerated ? "" : "NOT ")
                + "hardware accelerated");

        if (hardwareAccelerated) {
            getWindow().setFlags(
                    WindowManager.LayoutParams.FLAG_HARDWARE_ACCELERATED,
                    WindowManager.LayoutParams.FLAG_HARDWARE_ACCELERATED);
        }

        mLinearLayout = new LinearLayout(this);
        mLinearLayout.setOrientation(LinearLayout.VERTICAL);
        mLinearLayout.setShowDividers(LinearLayout.SHOW_DIVIDER_MIDDLE);
        mLinearLayout.setLayoutParams(new LayoutParams(LayoutParams.WRAP_CONTENT,
                LayoutParams.WRAP_CONTENT));

        setContentView(mLinearLayout);
    }

    /**
     * Set the ContentViews to be added to the current LinearLayout.
     *
     * This will destroy the ContentView instances that are being removed from the LinearLayout, so
     * care must be taken to not reference those instances.
     */
    public void setContentViewList(Collection<ContentView> contentViews) {
        mLinearLayout.removeAllViews();

        Set<ContentView> toBeDeleted = new HashSet<ContentView>(mContentViews);
        toBeDeleted.removeAll(contentViews);
        for (ContentView c : toBeDeleted) {
            c.destroy();
        }

        for (ContentView c : contentViews) {
            c.setLayoutParams(new LinearLayout.LayoutParams(
                    LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT, 1f));
            mLinearLayout.addView(c);
        }
        mContentViews.clear();
        mContentViews.addAll(contentViews);
    }

    public void setContentViews(ContentView... contentViews) {
        setContentViewList(Arrays.asList(contentViews));
    }

    public int getNumberOfContentViews() {
        return mContentViews.size();
    }

    public ContentView getContentView() {
        return mContentViews.get(0);
    }
}
