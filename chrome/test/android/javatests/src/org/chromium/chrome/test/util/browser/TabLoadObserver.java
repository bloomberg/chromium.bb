// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.EmptyTabObserver;
import org.chromium.chrome.browser.Tab;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content_public.browser.LoadUrlParams;

/**
 * Monitors that a Tab starts loading and stops loading a URL.
 */
public class TabLoadObserver extends EmptyTabObserver implements Criteria {
    private boolean mTabLoadStarted;
    private boolean mTabLoadStopped;

    public TabLoadObserver(final Tab tab, final String url) {
        tab.addObserver(this);
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                tab.loadUrl(new LoadUrlParams(url));
            }
        });
    }

    @Override
    public void onLoadStarted(Tab tab) {
        mTabLoadStarted = true;
    }

    @Override
    public void onLoadStopped(Tab tab) {
        mTabLoadStopped = true;
    }

    @Override
    public boolean isSatisfied() {
        return mTabLoadStarted && mTabLoadStopped;
    }
}