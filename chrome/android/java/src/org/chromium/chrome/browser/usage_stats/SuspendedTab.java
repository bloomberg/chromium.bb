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

import org.chromium.base.UserData;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.LoadUrlParams;

/**
 * Represents the suspension page presented when a user tries to visit a site whose fully-qualified
 * domain name (FQDN) has been suspended via Digital Wellbeing.
 */
public class SuspendedTab extends EmptyTabObserver implements UserData {
    private static final String DIGITAL_WELLBEING_DASHBOARD_ACTION =
            "com.google.android.apps.wellbeing.action.APP_USAGE_DASHBOARD";
    private static final Class<SuspendedTab> USER_DATA_KEY = SuspendedTab.class;

    public static SuspendedTab from(Tab tab) {
        SuspendedTab suspendedTab = get(tab);
        if (suspendedTab == null) {
            suspendedTab = tab.getUserDataHost().setUserData(USER_DATA_KEY, new SuspendedTab(tab));
        }
        return suspendedTab;
    }

    public static SuspendedTab get(Tab tab) {
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    private final Tab mTab;
    private View mView;
    private String mFqdn;

    private SuspendedTab(Tab tab) {
        mTab = tab;
    }

    /**
     * Show the suspended tab UI within the root view of the associated tab. This will stop loading
     * of mTab so that the page is not also rendered. If the suspended tab is already showing, this
     * will update its fqdn to the given one.
     */
    public void show(String fqdn) {
        mFqdn = fqdn;
        mTab.addObserver(this);
        mTab.stopLoading();
        if (isViewAttached()) {
            updateFqdnText();
        } else {
            attachView();
        }
    }

    /** Remove the suspended tab UI if it's currently being shown. */
    public void removeIfPresent() {
        removeViewIfPresent();

        mTab.removeObserver(this);
        mView = null;
        mFqdn = null;
    }

    /** @return the fqdn this SuspendedTab is currently showing for; null if not showing. */
    public String getFqdn() {
        return mFqdn;
    }

    /** @return Whether this SuspendedTab is currently showing. */
    public boolean isShowing() {
        return mFqdn != null;
    }

    private View createView() {
        Context context = mTab.getContext();
        LayoutInflater inflater = LayoutInflater.from(context);

        View suspendedTabView = inflater.inflate(R.layout.suspended_tab, null);
        TextView explanationText =
                (TextView) suspendedTabView.findViewById(R.id.suspended_tab_explanation);
        explanationText.setText(
                context.getString(R.string.usage_stats_site_paused_explanation, mFqdn));

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
        // getContentView() will return null if the tab doesn't have a WebContents, which is
        // possible in some situations, e.g. if the renderer crashes.
        if (parent == null) return;
        mView = createView();
        parent.addView(mView,
                new LinearLayout.LayoutParams(
                        LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
    }

    private boolean isViewAttached() {
        return mView != null && mView.getParent() == mTab.getContentView();
    }

    private void updateFqdnText() {
        Context context = mTab.getContext();
        TextView explanationText = (TextView) mView.findViewById(R.id.suspended_tab_explanation);
        explanationText.setText(
                context.getString(R.string.usage_stats_site_paused_explanation, mFqdn));
    }

    private void removeViewIfPresent() {
        if (isViewAttached()) {
            mTab.getContentView().removeView(mView);
        }
    }

    private void removeSelfIfFqdnChanged(String url) {
        String newFqdn = Uri.parse(url).getHost();
        if (newFqdn == null || !newFqdn.equals(mFqdn)) {
            removeIfPresent();
        }
    }

    // TabObserver implementation.
    @Override
    public void onLoadUrl(Tab tab, LoadUrlParams params, int loadType) {
        removeSelfIfFqdnChanged(params.getUrl());
    }

    @Override
    public void onPageLoadStarted(Tab tab, String url) {
        removeSelfIfFqdnChanged(url);
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

    // UserData implementation.
    @Override
    public void destroy() {
        mTab.removeObserver(this);
    }
}
