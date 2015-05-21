// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.content.Context;
import android.graphics.Canvas;
import android.view.LayoutInflater;
import android.view.View;

import com.google.android.apps.chrome.R;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.NativePage;
import org.chromium.chrome.browser.Tab;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.compositor.layouts.content.InvalidationAwareThumbnailProvider;
import org.chromium.chrome.browser.ntp.IncognitoNewTabPageView.IncognitoNewTabPageManager;
import org.chromium.content_public.browser.LoadUrlParams;

/**
 * Provides functionality when the user interacts with the Incognito NTP.
 */
public class IncognitoNewTabPage implements NativePage, InvalidationAwareThumbnailProvider {

    private static final String LEARN_MORE_INCOGNITO_LINK =
            "https://www.google.com/support/chrome/bin/answer.py?answer=95464";

    private final Tab mTab;

    private final String mTitle;
    private final int mBackgroundColor;
    private final IncognitoNewTabPageView mIncognitoNewTabPageView;

    private boolean mIsLoaded;

    private final IncognitoNewTabPageManager mIncognitoNewTabPageManager =
            new IncognitoNewTabPageManager() {
        @Override
        public void loadIncognitoLearnMore() {
            mTab.loadUrl(new LoadUrlParams(LEARN_MORE_INCOGNITO_LINK));
        }

        @Override
        public void onLoadingComplete() {
            mIsLoaded = true;
        }
    };

    /**
     * Constructs an Incognito NewTabPage.
     * @param context The context used to create the new tab page's View.
     * @param tab The Tab that is showing this new tab page.
     */
    public IncognitoNewTabPage(Context context, Tab tab) {
        mTab = tab;

        mTitle = context.getResources().getString(R.string.button_new_tab);
        mBackgroundColor = context.getResources().getColor(R.color.ntp_bg_incognito);

        LayoutInflater inflater = LayoutInflater.from(context);
        mIncognitoNewTabPageView =
                (IncognitoNewTabPageView) inflater.inflate(R.layout.new_tab_page_incognito, null);
        mIncognitoNewTabPageView.initialize(mIncognitoNewTabPageManager);
    }

    /**
     * @return Whether the NTP has finished loaded.
     */
    @VisibleForTesting
    public boolean isLoadedForTests() {
        return mIsLoaded;
    }

    // NativePage overrides

    @Override
    public void destroy() {
        assert getView().getParent() == null : "Destroy called before removed from window";
    }

    @Override
    public String getUrl() {
        return UrlConstants.NTP_URL;
    }

    @Override
    public String getTitle() {
        return mTitle;
    }

    @Override
    public int getBackgroundColor() {
        return mBackgroundColor;
    }

    @Override
    public View getView() {
        return mIncognitoNewTabPageView;
    }

    @Override
    public String getHost() {
        return UrlConstants.NTP_HOST;
    }

    @Override
    public void updateForUrl(String url) {
    }

    // InvalidationAwareThumbnailProvider

    @Override
    public boolean shouldCaptureThumbnail() {
        return mIncognitoNewTabPageView.shouldCaptureThumbnail();
    }

    @Override
    public void captureThumbnail(Canvas canvas) {
        mIncognitoNewTabPageView.captureThumbnail(canvas);
    }
}
