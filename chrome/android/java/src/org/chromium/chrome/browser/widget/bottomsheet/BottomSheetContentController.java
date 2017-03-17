// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import android.content.Context;
import android.content.res.Resources;
import android.support.design.internal.BottomNavigationItemView;
import android.support.design.internal.BottomNavigationMenuView;
import android.support.design.widget.BottomNavigationView;
import android.support.design.widget.BottomNavigationView.OnNavigationItemSelectedListener;
import android.util.AttributeSet;
import android.view.MenuItem;
import android.view.View;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkSheetContent;
import org.chromium.chrome.browser.download.DownloadSheetContent;
import org.chromium.chrome.browser.history.HistorySheetContent;
import org.chromium.chrome.browser.suggestions.SuggestionsBottomSheetContent;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet.BottomSheetContent;

import java.lang.reflect.Field;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.Map.Entry;

/**
 * Displays and controls a {@link BottomNavigationView} fixed to the bottom of the
 * {@link BottomSheet}. Also manages {@link BottomSheetContent} displayed in the BottomSheet.
 */
public class BottomSheetContentController extends BottomNavigationView
        implements BottomSheetObserver, OnNavigationItemSelectedListener {
    private BottomSheet mBottomSheet;
    private TabModelSelector mTabModelSelector;
    private float mDistanceBelowToolbarPx;
    private int mSelectedItemId;
    private boolean mDefaultContentInitialized;

    private final Map<Integer, BottomSheetContent> mBottomSheetContents = new HashMap<>();

    public BottomSheetContentController(Context context, AttributeSet atts) {
        super(context, atts);
    }

    /**
     * Initializes the {@link BottomSheetContentController}.
     * @param bottomSheet The {@link BottomSheet} associated with this bottom nav.
     * @param controlContainerHeight The height of the control container in px.
     * @param tabModelSelector The {@link TabModelSelector} for the application.
     */
    public void init(BottomSheet bottomSheet, int controlContainerHeight,
            TabModelSelector tabModelSelector) {
        mBottomSheet = bottomSheet;
        mBottomSheet.addObserver(this);
        mTabModelSelector = tabModelSelector;

        Resources res = getContext().getResources();
        mDistanceBelowToolbarPx = controlContainerHeight
                + res.getDimensionPixelOffset(R.dimen.bottom_nav_space_from_toolbar);

        setOnNavigationItemSelectedListener(this);
        disableShiftingMode();
    }

    /**
     * Initialize the default {@link BottomSheetContent}.
     */
    public void initializeDefaultContent() {
        if (mDefaultContentInitialized) return;
        showBottomSheetContent(R.id.action_home);
        mDefaultContentInitialized = true;
    }

    @Override
    public boolean onNavigationItemSelected(MenuItem item) {
        if (mSelectedItemId == item.getItemId()) return false;

        showBottomSheetContent(item.getItemId());
        return true;
    }

    @Override
    public void onTransitionPeekToHalf(float transitionFraction) {
        float offsetY = (mBottomSheet.getMinOffset() - mBottomSheet.getSheetOffsetFromBottom())
                + mDistanceBelowToolbarPx;
        setTranslationY((int) Math.max(offsetY, 0f));
        setVisibility(MathUtils.areFloatsEqual(transitionFraction, 0f) ? View.GONE : View.VISIBLE);
    }

    @Override
    public void onSheetOpened() {
        if (!mDefaultContentInitialized && mTabModelSelector.getCurrentTab() != null) {
            initializeDefaultContent();
        }
    }

    @Override
    public void onSheetClosed() {
        Iterator<Entry<Integer, BottomSheetContent>> contentIterator =
                mBottomSheetContents.entrySet().iterator();
        while (contentIterator.hasNext()) {
            Entry<Integer, BottomSheetContent> entry = contentIterator.next();
            if (entry.getKey() == R.id.action_home) continue;

            entry.getValue().destroy();
            contentIterator.remove();
        }
        // TODO(twellington): determine a policy for destroying the SuggestionsBottomSheetContent.

        if (mSelectedItemId == 0 || mSelectedItemId == R.id.action_home) return;

        showBottomSheetContent(R.id.action_home);
    }

    @Override
    public void onLoadUrl(String url) {}

    @Override
    public void onSheetOffsetChanged(float heightFraction) {}

    // TODO(twellington): remove this once the support library is updated to allow disabling
    //                    shifting mode or determines shifting mode based on the width of the
    //                    child views.
    private void disableShiftingMode() {
        BottomNavigationMenuView menuView = (BottomNavigationMenuView) getChildAt(0);
        try {
            Field shiftingMode = menuView.getClass().getDeclaredField("mShiftingMode");
            shiftingMode.setAccessible(true);
            shiftingMode.setBoolean(menuView, false);
            shiftingMode.setAccessible(false);
            for (int i = 0; i < menuView.getChildCount(); i++) {
                BottomNavigationItemView item = (BottomNavigationItemView) menuView.getChildAt(i);
                item.setShiftingMode(false);
                // Set the checked value so that the view will be updated.
                item.setChecked(item.getItemData().isChecked());
            }
        } catch (NoSuchFieldException | IllegalAccessException e) {
            // Do nothing if reflection fails.
        }
    }

    private BottomSheetContent getSheetContentForId(int navItemId) {
        BottomSheetContent content = mBottomSheetContents.get(navItemId);
        if (content != null) return content;

        if (navItemId == R.id.action_home) {
            content = new SuggestionsBottomSheetContent(
                    mTabModelSelector.getCurrentTab().getActivity(), mBottomSheet,
                    mTabModelSelector);
        } else if (navItemId == R.id.action_downloads) {
            content = new DownloadSheetContent(mTabModelSelector.getCurrentTab().getActivity(),
                    mTabModelSelector.getCurrentModel().isIncognito());
        } else if (navItemId == R.id.action_bookmarks) {
            content = new BookmarkSheetContent(mTabModelSelector.getCurrentTab().getActivity());
        } else if (navItemId == R.id.action_history) {
            content = new HistorySheetContent(mTabModelSelector.getCurrentTab().getActivity());
        }
        mBottomSheetContents.put(navItemId, content);
        return content;
    }

    private void showBottomSheetContent(int navItemId) {
        // There are some bugs related to programatically selecting menu items that are fixed in
        // newer support library versions.
        // TODO(twellington): remove this after the support library is rolled.
        if (mSelectedItemId != 0) getMenu().findItem(mSelectedItemId).setChecked(false);
        mSelectedItemId = navItemId;
        getMenu().findItem(mSelectedItemId).setChecked(true);

        mBottomSheet.showContent(getSheetContentForId(mSelectedItemId));
    }
}
