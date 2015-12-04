// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser;

import android.text.TextUtils;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.Locale;

/**
 * Monitors that a Tab starts loading and stops loading a URL.
 */
public class TabLoadObserver extends EmptyTabObserver {
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

    /**
     * Asserts the page has loaded.
     * @param maxAllowedTime The maximum time this will wait for the page to load.
     */
    public void assertLoaded(long maxAllowedTime) throws InterruptedException {
        Criteria loadedCriteria = new Criteria() {
            @Override
            public boolean isSatisfied() {
                if (!mTabLoadStarted) {
                    updateFailureReason("load started never called");
                    return false;
                }
                if (!mTabLoadStopped) {
                    updateFailureReason("load stopped never called");
                    return false;
                }
                if (!mTab.isLoadingAndRenderingDone()) {
                    updateFailureReason("load and rendering never completed");
                    return false;
                }

                String title = mTab.getTitle();
                if (mExpectedTitle != null && !TextUtils.equals(mExpectedTitle, title)) {
                    updateFailureReason(String.format(
                            Locale.ENGLISH,
                            "title did not match -- expected: \"%s\", actual \"%s\"",
                            mExpectedTitle, title));
                    return false;
                }
                if (mExpectedScale != null) {
                    if (mTab.getContentViewCore() == null) {
                        updateFailureReason("tab has no content view core");
                        return false;
                    }

                    float scale = mTab.getContentViewCore().getScale();
                    if (Math.abs(mExpectedScale - scale) >= FLOAT_EPSILON) {
                        updateFailureReason(String.format(
                                Locale.ENGLISH,
                                "scale did not match with allowed epsilon -- "
                                + "expected: \"%f\", actual \"%f\"", mExpectedScale, scale));
                        return false;
                    }
                }
                updateFailureReason(null);
                return true;
            }
        };

        CriteriaHelper.pollForUIThreadCriteria(
                loadedCriteria, maxAllowedTime, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Asserts that the page has loaded.
     */
    public void assertLoaded() throws InterruptedException {
        assertLoaded(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
    }
}
