// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.content.Intent;
import android.os.Bundle;
import android.view.ViewGroup;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.SingleTabActivity;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;

/**
 * An Activity used to display the Dino Run game.
 */
public class DinoActivity extends SingleTabActivity {
    @Override
    public void finishNativeInitialization() {
        initializeCompositorContent(new LayoutManager(getCompositorViewHolder()), null /* urlBar */,
                (ViewGroup) findViewById(android.R.id.content), null /* controlContainer */);
        getFullscreenManager().setTab(getActivityTab());
        super.finishNativeInitialization();
    }

    @Override
    protected Tab createTab() {
        Tab tab = super.createTab();
        LoadUrlParams loadUrlParams = new LoadUrlParams(UrlConstants.CHROME_DINO_URL);
        loadUrlParams.setHasUserGesture(true);
        loadUrlParams.setTransitionType(PageTransition.LINK | PageTransition.FROM_API);
        tab.loadUrl(loadUrlParams);
        return tab;
    }

    @Override
    public void onNewIntentWithNative(Intent intent) {}

    @Override
    public @ChromeActivity.ActivityType int getActivityType() {
        return ChromeActivity.ActivityType.DINO;
    }

    @Override
    protected void initializeToolbar() {}

    @Override
    protected ChromeFullscreenManager createFullscreenManager() {
        return new ChromeFullscreenManager(this, ChromeFullscreenManager.ControlsPosition.NONE);
    }

    @Override
    protected TabState restoreTabState(Bundle savedInstanceState, int tabId) {
        return null;
    }
}
