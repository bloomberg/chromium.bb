// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import static android.text.format.DateUtils.MINUTE_IN_MILLIS;
import static android.text.format.DateUtils.getRelativeTimeSpanString;

import android.content.Context;
import android.graphics.Bitmap;

import org.chromium.chrome.browser.favicon.LargeIconBridge;
import org.chromium.chrome.browser.history.BrowsingHistoryBridge;
import org.chromium.chrome.browser.history.HistoryItem;
import org.chromium.chrome.browser.history.HistoryProvider;
import org.chromium.chrome.browser.native_page.NativePageHost;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.chrome.browser.util.ViewUtils;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;
import org.chromium.chrome.touchless.R;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * Mediator used to look for history events and update the model accordingly.
 */
// TODO(crbug.com/948858): Add unit tests for this behavior.
class OpenLastTabMediator implements HistoryProvider.BrowsingHistoryObserver, FocusableComponent {
    private final Context mContext;
    private final Profile mProfile;
    private final NativePageHost mNativePageHost;

    private final PropertyModel mModel;

    private BrowsingHistoryBridge mHistoryBridge;
    private final RoundedIconGenerator mIconGenerator;
    private final LargeIconBridge mIconBridge;

    OpenLastTabMediator(Context context, Profile profile, NativePageHost nativePageHost,
            PropertyModel model, OpenLastTabView view) {
        mContext = context;
        mProfile = profile;
        mNativePageHost = nativePageHost;

        mModel = model;

        mHistoryBridge = new BrowsingHistoryBridge(false);
        mHistoryBridge.setObserver(this);
        mIconGenerator =
                ViewUtils.createDefaultRoundedIconGenerator(mContext.getResources(), false);
        mIconBridge = new LargeIconBridge(mProfile);

        // TODO(wylieb):Investigate adding an item limit to the API.
        // Query the history for everything (no API exists to only query for the most recent).
        mHistoryBridge.queryHistory("");
    }

    void destroy() {
        if (mHistoryBridge != null) {
            mHistoryBridge.destroy();
            mHistoryBridge = null;
        }
    }

    @Override
    public void requestFocus() {
        mModel.set(OpenLastTabProperties.SHOULD_FOCUS_VIEW, true);
    }

    @Override
    public void setOnFocusListener(Runnable listener) {
        mModel.set(OpenLastTabProperties.ON_FOCUS_CALLBACK, listener);
    }

    @Override
    public void onQueryHistoryComplete(List<HistoryItem> items, boolean hasMorePotentialMatches) {
        if (items.size() == 0) {
            // Consider the case where the history has nothing in it to be a failure.
            mModel.set(OpenLastTabProperties.OPEN_LAST_TAB_LOAD_SUCCESS, false);
            return;
        }

        // First item is most recent.
        HistoryItem item = items.get(0);

        String title = UrlUtilities.getDomainAndRegistry(item.getUrl(), false);
        // Default the timestamp to happening just now. If it happened over a minute ago, calculate
        // and set the relative timestamp string.
        String timestamp = mContext.getResources().getString(R.string.open_last_tab_just_now);
        long now = System.currentTimeMillis();
        if (now - item.getTimestamp() > MINUTE_IN_MILLIS) {
            timestamp = getRelativeTimeSpanString(item.getTimestamp(), now, MINUTE_IN_MILLIS)
                                .toString();
        }

        mModel.set(OpenLastTabProperties.OPEN_LAST_TAB_TITLE, title);
        mModel.set(OpenLastTabProperties.OPEN_LAST_TAB_TIMESTAMP, timestamp);
        boolean willReturnIcon = mIconBridge.getLargeIconForUrl(item.getUrl(),
                mContext.getResources().getDimensionPixelSize(R.dimen.open_last_tab_icon_size),
                (icon, fallbackColor, isFallbackColorDefault, iconType) -> {
                    setAndShowButton(item.getUrl(), icon, fallbackColor);
                });

        // False if icon bridge won't call us back.
        if (!willReturnIcon) {
            setAndShowButton(item.getUrl(), null, R.color.default_icon_color);
        }
        mModel.set(OpenLastTabProperties.OPEN_LAST_TAB_LOAD_SUCCESS, true);
    }

    @Override
    public void onHistoryDeleted() {}

    @Override
    public void hasOtherFormsOfBrowsingData(boolean hasOtherForms) {}

    private void setAndShowButton(String url, Bitmap icon, int fallbackColor) {
        mModel.set(OpenLastTabProperties.OPEN_LAST_TAB_ON_CLICK_LISTENER, view -> {
            mNativePageHost.loadUrl(new LoadUrlParams(url, PageTransition.AUTO_BOOKMARK),
                    /* Explore page is never off the record. */ false);
        });

        if (icon == null) {
            mIconGenerator.setBackgroundColor(fallbackColor);
            icon = mIconGenerator.generateIconForUrl(url);
        }
        mModel.set(OpenLastTabProperties.OPEN_LAST_TAB_FAVICON, icon);
    }
}
