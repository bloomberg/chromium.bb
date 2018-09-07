// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.app.Activity;
import android.graphics.Color;
import android.support.design.widget.CoordinatorLayout;
import android.view.Gravity;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.HorizontalScrollView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.chrome.R;

import javax.annotation.Nullable;

/** Controller to interact with the bottom bar. */
class BottomBarController {
    private static final String SCRIPTS_STATUS_MESSAGE = "Scripts";

    private final Activity mActivity;
    private final LinearLayout mBottomBar;
    private ViewGroup mScriptsViewContainer;
    private TextView mStatusMessageView;

    /**
     * Constructs a bottom bar.
     *
     * @param activity The Activity
     */
    public BottomBarController(Activity activity) {
        mActivity = activity;

        mBottomBar = createBottomBar();
        ((ViewGroup) mActivity.findViewById(R.id.coordinator)).addView(mBottomBar);

        CoordinatorLayout.LayoutParams params =
                (CoordinatorLayout.LayoutParams) mBottomBar.getLayoutParams();
        params.gravity = Gravity.BOTTOM;

        showStatusMessage(SCRIPTS_STATUS_MESSAGE);
    }

    /**
     * Shows a message in the status bar.
     *
     * @param message Message to display.
     */
    public void showStatusMessage(@Nullable String message) {
        mStatusMessageView.setText(message);
    }

    /**
     * Updates the list of scripts in the bar.
     *
     * @param scripts List of scripts to show.
     */
    public void updateScripts(String[] scripts) {
        mScriptsViewContainer.removeAllViews();
        if (scripts.length == 0) {
            return;
        }

        for (String script : scripts) {
            mScriptsViewContainer.addView(createScriptView(script));
        }
    }

    private LinearLayout createBottomBar() {
        LinearLayout bottomBar = createContainer();
        mStatusMessageView = createTextView();
        bottomBar.addView(mStatusMessageView);
        bottomBar.addView(createScrollView());
        return bottomBar;
    }

    private LinearLayout createContainer() {
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        LinearLayout layout = createLinearLayout(params, LinearLayout.VERTICAL);
        layout.setBackgroundColor(Color.parseColor("#f0f0f0"));
        layout.setPadding(10, 10, 10, 10);
        return layout;
    }

    private TextView createTextView() {
        TextView textView = new TextView(mActivity);
        textView.setLayoutParams(
                new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));
        textView.setGravity(Gravity.CENTER_HORIZONTAL);
        return textView;
    }

    private HorizontalScrollView createScrollView() {
        HorizontalScrollView scrollView = new HorizontalScrollView(mActivity);
        scrollView.setLayoutParams(
                new LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        scrollView.setHorizontalScrollBarEnabled(false);
        mScriptsViewContainer = createLinearLayout(
                new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT),
                LinearLayout.HORIZONTAL);
        scrollView.addView(mScriptsViewContainer);
        return scrollView;
    }

    private LinearLayout createLinearLayout(LayoutParams layoutParams, int orientation) {
        LinearLayout layout = new LinearLayout(mActivity);
        layout.setLayoutParams(layoutParams);
        layout.setOrientation(orientation);
        return layout;
    }

    private TextView createScriptView(String text) {
        TextView scriptView = new TextView(mActivity);
        scriptView.setPadding(20, 20, 20, 20);
        LinearLayout.LayoutParams layoutParams = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        layoutParams.setMargins(10, 10, 10, 10);
        scriptView.setLayoutParams(layoutParams);
        scriptView.setMaxLines(1);
        scriptView.setText(text);
        scriptView.setBackgroundColor(Color.parseColor("#e0e0e0"));
        return scriptView;
    }
}
