// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.usage_stats;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.LoadUrlParams;

/**
 * Represents the suspension page presented when a user tries to visit a site whose fully-qualified
 * domain name (FQDN) has been suspended via Digital Wellbeing.
 */
public class SuspendedTab extends EmptyTabObserver {
    private static final String DIGITAL_WELLBEING_DASHBOARD_ACTION =
            "com.google.android.apps.wellbeing.action.APP_USAGE_DASHBOARD";

    private final Tab mTab;
    private View mView;

    public static SuspendedTab create(Tab tab) {
        return new SuspendedTab(tab);
    }

    private SuspendedTab(Tab tab) {
        mTab = tab;
        mTab.addObserver(this);
    }

    /**
     * Show the suspended tab UI within the root view of the associated tab. This will stop loading
     * of mTab so that the page is not also rendered.
     */
    public void show() {
        if (mTab.getWebContents() == null) return;

        mTab.stopLoading();
        attachView();
    }

    private View createView() {
        Context context = mTab.getContext();
        LayoutInflater inflater = LayoutInflater.from(context);

        String fqdn = Uri.parse(mTab.getUrl()).getHost();
        View suspendedTabView = inflater.inflate(R.layout.suspended_tab, null);
        TextView explanationText =
                (TextView) suspendedTabView.findViewById(R.id.suspended_tab_explanation);
        explanationText.setText(
                context.getString(R.string.usage_stats_site_paused_explanation, fqdn));

        View settingsLink = suspendedTabView.findViewById(R.id.suspended_tab_settings_button);
        settingsLink.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent intent = new Intent(DIGITAL_WELLBEING_DASHBOARD_ACTION);
                intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                context.startActivity(intent);
            }
        });

        return suspendedTabView;
    }

    private void attachView() {
        assert mView == null;

        ViewGroup parent = mTab.getContentView();
        mView = createView();
        parent.addView(mView,
                new LinearLayout.LayoutParams(
                        LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
    }

    private void removeIfPresent() {
        removeViewIfPresent();

        mTab.removeObserver(this);
        mView = null;
    }

    private void removeViewIfPresent() {
        if (isShowing()) {
            mTab.getContentView().removeView(mView);
        }
    }

    private boolean isShowing() {
        return mView != null && mView.getParent() == mTab.getContentView();
    }

    // TabObserver implementation.
    @Override
    public void onLoadUrl(Tab tab, LoadUrlParams params, int loadType) {
        removeIfPresent();
    }

    @Override
    public void onPageLoadStarted(Tab tab, String url) {
        removeIfPresent();
    }

    @Override
    public void onDestroyed(Tab tab) {
        removeIfPresent();
    }

    // TODO(pnoland): Add integration tests for SuspendedTab that exercise this multi-window logic.
    @Override
    public void onActivityAttachmentChanged(Tab tab, boolean isAttached) {
        if (!isAttached) {
            removeViewIfPresent();
        } else {
            attachView();
        }
    }
}
