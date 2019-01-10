// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.usage_stats;

import android.app.Activity;
import android.net.Uri;
import android.webkit.URLUtil;

import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;

/**
 * Class that observes url and tab changes in order to track when browsing stops and starts for each
 * visited fully-qualified domain name (FQDN).
 */
public class PageViewObserver {
    private final Activity mActivity;
    private final TabModelSelectorTabModelObserver mTabModelObserver;
    private final TabModelSelector mTabModelSelector;
    private final TabObserver mTabObserver;
    private final EventTracker mEventTracker;

    private Tab mCurrentTab;
    private String mLastFqdn;

    public PageViewObserver(
            Activity activity, TabModelSelector tabModelSelector, EventTracker eventTracker) {
        mActivity = activity;
        mTabModelSelector = tabModelSelector;
        mEventTracker = eventTracker;
        mTabObserver = new EmptyTabObserver() {
            @Override
            public void onShown(Tab tab, @TabSelectionType int type) {
                updateUrl(tab.getUrl());
            }

            @Override
            public void onHidden(Tab tab, @TabHidingType int type) {
                updateUrl(null);
            }

            @Override
            public void onUpdateUrl(Tab tab, String url) {
                assert tab == mCurrentTab;

                updateUrl(url);
            }
        };

        mTabModelObserver = new TabModelSelectorTabModelObserver(tabModelSelector) {
            @Override
            public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                assert tab != null;
                if (tab == mCurrentTab) return;

                switchObserverToTab(tab);
                updateUrl(tab.getUrl());
            }

            @Override
            public void willCloseTab(Tab tab, boolean animate) {
                assert tab != null;
                if (tab != mCurrentTab) return;

                updateUrl(null);
                switchObserverToTab(null);
            }

            @Override
            public void tabRemoved(Tab tab) {
                assert tab != null;
                if (tab != mCurrentTab) return;

                updateUrl(null);
                switchObserverToTab(null);
            }
        };

        switchObserverToTab(tabModelSelector.getCurrentTab());
    }

    private void updateUrl(String newUrl) {
        String newFqdn = newUrl == null ? "" : Uri.parse(newUrl).getHost();
        if (mLastFqdn != null && mLastFqdn.equals(newFqdn)) return;

        if (mLastFqdn != null) {
            mEventTracker.addWebsiteEvent(new WebsiteEvent(
                    System.currentTimeMillis(), mLastFqdn, WebsiteEvent.EventType.STOP));
            mLastFqdn = null;
        }

        if (!URLUtil.isHttpUrl(newUrl) && !URLUtil.isHttpsUrl(newUrl)) return;

        mLastFqdn = newFqdn;
        mEventTracker.addWebsiteEvent(new WebsiteEvent(
                System.currentTimeMillis(), mLastFqdn, WebsiteEvent.EventType.START));
    }

    private void switchObserverToTab(Tab tab) {
        if (mCurrentTab != tab && mCurrentTab != null) {
            mCurrentTab.removeObserver(mTabObserver);
        }

        mCurrentTab = tab;
        if (mCurrentTab != null) {
            mCurrentTab.addObserver(mTabObserver);
        }
    }
}
