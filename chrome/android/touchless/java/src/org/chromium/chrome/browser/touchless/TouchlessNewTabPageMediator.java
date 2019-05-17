// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.os.Bundle;
import android.os.Parcel;
import android.util.Base64;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.suggestions.SuggestionsMetrics;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.ui.modelutil.PropertyModel;

class TouchlessNewTabPageMediator extends EmptyTabObserver {
    private static final String NAVIGATION_ENTRY_SCROLL_POSITION_KEY = "TouchlessScrollPosition";
    private static final String NAVIGATION_ENTRY_FOCUS_KEY = "TouchlessFocus";

    private final PropertyModel mModel;
    private final Tab mTab;
    private long mLastShownTimeNs;

    private ScrollPositionInfo mScrollPosition;
    private Bundle mFocus;

    public TouchlessNewTabPageMediator(Tab tab) {
        mTab = tab;
        mTab.addObserver(this);

        ScrollPositionInfo initialScrollPosition = ScrollPositionInfo.deserialize(
                NewTabPage.getStringFromNavigationEntry(tab, NAVIGATION_ENTRY_SCROLL_POSITION_KEY));

        String serializedString =
                NewTabPage.getStringFromNavigationEntry(tab, NAVIGATION_ENTRY_FOCUS_KEY);
        Bundle initialFocus = null;
        if (serializedString != null) {
            Parcel parcel = Parcel.obtain();
            byte[] bytes = Base64.decode(serializedString, Base64.DEFAULT);
            parcel.unmarshall(bytes, 0, bytes.length);
            parcel.setDataPosition(0);
            initialFocus = Bundle.CREATOR.createFromParcel(parcel);
            parcel.recycle();
        }

        mModel = new PropertyModel.Builder(TouchlessNewTabPageProperties.ALL_KEYS)
                         .with(TouchlessNewTabPageProperties.FOCUS_CHANGE_CALLBACK,
                                 (newFocus) -> mFocus = newFocus)
                         .with(TouchlessNewTabPageProperties.INITIAL_FOCUS, initialFocus)
                         .with(TouchlessNewTabPageProperties.INITIAL_SCROLL_POSITION,
                                 initialScrollPosition)
                         .with(TouchlessNewTabPageProperties.SCROLL_POSITION_CALLBACK,
                                 (newScrollPosition) -> mScrollPosition = newScrollPosition)
                         .build();

        // TODO(dewittj): Track loading status of the tiles before we record shown and hidden
        // states.
        recordNTPShown();
    }

    @Override
    public void onShown(Tab tab, @TabSelectionType int type) {
        // TODO(dewittj): Track loading status of the tiles before we record shown and hidden
        // states.
        recordNTPShown();
    }

    @Override
    public void onHidden(Tab tab, @Tab.TabHidingType int type) {
        recordNTPHidden();
    }

    @Override
    public void onContentChanged(Tab tab) {
        // This is the main entry point for #saveStateAndRemoveObserver when loading a native page.
        // This is still called when loading renderer pages, but after the next navigation entry is
        // committed, see https://crbug.com/963584.
        saveStateAndRemoveObserver(tab);
    }

    @Override
    public void onPageLoadStarted(Tab tab, String url) {
        // This is the main entry point for #saveStateAndRemoveObserver when loading a renderer
        // page. This is still called when loading native pages, but after we should have completely
        // cleaned up ourselves and observers, see https://crbug.com/963584.
        saveStateAndRemoveObserver(tab);
    }

    public PropertyModel getModel() {
        return mModel;
    }

    void destroy() {
        if (!mTab.isHidden()) recordNTPHidden();
        // Should already have been removed from mTab's observer list.
    }

    /**
     * Records UMA for the NTP being shown. This includes a fresh page load or being brought to
     * the foreground.
     */
    private void recordNTPShown() {
        mLastShownTimeNs = System.nanoTime();
        NewTabPageUma.recordNTPImpression(NewTabPageUma.NTP_IMPRESSION_REGULAR);
        RecordUserAction.record("MobileNTPShown");
        SuggestionsMetrics.recordSurfaceVisible();
    }

    /** Records UMA for the NTP being hidden and the time spent on it. */
    private void recordNTPHidden() {
        NewTabPageUma.recordTimeSpentOnNtp(mLastShownTimeNs);
        SuggestionsMetrics.recordSurfaceHidden();
    }

    /**
     * Saves focus and scroll state to the current navigation entry.
     * @param tab The current tab.
     */
    private void saveStateAndRemoveObserver(Tab tab) {
        // This method ends up being called when the NTP is loaded, so ignore those cases.
        if (mScrollPosition == null && mFocus == null) {
            return;
        }

        if (mScrollPosition != null) {
            NewTabPage.saveStringToNavigationEntry(
                    tab, NAVIGATION_ENTRY_SCROLL_POSITION_KEY, mScrollPosition.serialize());
        }
        if (mFocus != null) {
            Parcel parcel = Parcel.obtain();
            mFocus.writeToParcel(parcel, 0);
            byte[] bytes = parcel.marshall();
            parcel.recycle();
            String serializedString = Base64.encodeToString(bytes, Base64.DEFAULT);
            NewTabPage.saveStringToNavigationEntry(
                    tab, NAVIGATION_ENTRY_FOCUS_KEY, serializedString);
        }

        // Observations need to be removed now to avoid saving state to the wrong entry when loading
        // a renderer page from on onContentChange notification, see https://crbug.com/963584.
        tab.removeObserver(this);
    }
}
