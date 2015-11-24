// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser;

import android.text.TextUtils;

import junit.framework.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.Locale;
import java.util.concurrent.atomic.AtomicReference;

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
        final AtomicReference<String> failureReason = new AtomicReference<>();

        Criteria loadedCriteria = new Criteria() {
            @Override
            public boolean isSatisfied() {
                if (!mTabLoadStarted) {
                    failureReason.set("load started never called");
                    return false;
                }
                if (!mTabLoadStopped) {
                    failureReason.set("load stopped never called");
                    return false;
                }
                if (!mTab.isLoadingAndRenderingDone()) {
                    failureReason.set("load and rendering never completed");
                    return false;
                }

                String title = mTab.getTitle();
                if (mExpectedTitle != null && !TextUtils.equals(mExpectedTitle, title)) {
                    failureReason.set(String.format(
                            Locale.ENGLISH,
                            "title did not match -- expected: \"%s\", actual \"%s\"",
                            mExpectedTitle, title));
                    return false;
                }
                if (mExpectedScale != null) {
                    if (mTab.getContentViewCore() == null) {
                        failureReason.set("tab has no content view core");
                        return false;
                    }

                    float scale = mTab.getContentViewCore().getScale();
                    if (Math.abs(mExpectedScale - scale) >= FLOAT_EPSILON) {
                        failureReason.set(String.format(
                                Locale.ENGLISH,
                                "scale did not match with allowed epsilon -- "
                                + "expected: \"%f\", actual \"%f\"", mExpectedScale, scale));
                        return false;
                    }
                }
                failureReason.set(null);
                return true;
            }
        };

        boolean result = CriteriaHelper.pollForUIThreadCriteria(
                loadedCriteria, maxAllowedTime, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        if (!result) {
            Assert.fail("Tab not fully loaded because: " + failureReason.get());
        }
    }

    /**
     * Asserts that the page has loaded.
     */
    public void assertLoaded() throws InterruptedException {
        assertLoaded(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
    }
}
