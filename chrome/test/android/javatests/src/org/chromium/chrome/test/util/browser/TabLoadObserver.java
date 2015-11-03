// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser;

import android.text.TextUtils;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content_public.browser.LoadUrlParams;

/**
 * Monitors that a Tab starts loading and stops loading a URL.
 */
public class TabLoadObserver extends EmptyTabObserver implements Criteria {
    private static final float FLOAT_EPSILON = 0.001f;

    private final Tab mTab;
    private final String mExpectedTitle;
    private final Float mExpectedScale;

    private boolean mTabLoadStarted;
    private boolean mTabLoadStopped;

    public TabLoadObserver(Tab tab, final String url) {
        this(tab, url, null, null);
    }

    public TabLoadObserver(Tab tab, final String url, String expectedTitle, Float expectedScale) {
        mTab = tab;
        mExpectedTitle = expectedTitle;
        mExpectedScale = expectedScale;

        mTab.addObserver(this);
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mTab.loadUrl(new LoadUrlParams(url));
            }
        });
    }

    @Override
    public void onLoadStarted(Tab tab, boolean toDifferentDocument) {
        mTabLoadStarted = true;
    }

    @Override
    public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
        mTabLoadStopped = true;
    }

    @Override
    public boolean isSatisfied() {
        if (!mTabLoadStarted) return false;
        if (!mTabLoadStopped) return false;
        if (!mTab.isLoadingAndRenderingDone()) return false;

        if (mExpectedTitle != null && !TextUtils.equals(mExpectedTitle, mTab.getTitle())) {
            return false;
        }
        if (mExpectedScale != null) {
            if (mTab.getContentViewCore() == null) return false;
            if (Math.abs(mExpectedScale - mTab.getContentViewCore().getScale()) >= FLOAT_EPSILON) {
                return false;
            }
        }
        return true;
    }
}
