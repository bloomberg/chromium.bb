// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

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
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.ui.widget.Toast;

import java.lang.reflect.Field;

/**
 * Displays and controls a {@link BottomNavigationView} fixed to the bottom of the
 * {@link BottomSheet}.
 */
public class BottomSheetContentController extends BottomNavigationView
        implements BottomSheetObserver, OnNavigationItemSelectedListener {
    private BottomSheet mBottomSheet;
    private float mDistanceBelowToolbarPx;

    public BottomSheetContentController(Context context, AttributeSet atts) {
        super(context, atts);
    }

    /**
     * Initializes the {@link BottomSheetContentController}.
     * @param bottomSheet The {@link BottomSheet} associated with this bottom nav.
     * @param controlContainerHeight The height of the control container in px.
     */
    public void init(BottomSheet bottomSheet, int controlContainerHeight) {
        mBottomSheet = bottomSheet;
        mBottomSheet.addObserver(this);

        Resources res = getContext().getResources();
        mDistanceBelowToolbarPx = controlContainerHeight
                + res.getDimensionPixelOffset(R.dimen.bottom_nav_space_from_toolbar);

        setOnNavigationItemSelectedListener(this);
        disableShiftingMode();
    }

    @Override
    public boolean onNavigationItemSelected(MenuItem item) {
        Toast.makeText(getContext(), "Not implemented", Toast.LENGTH_SHORT).show();
        return false;
    }

    @Override
    public void onTransitionPeekToHalf(float transitionFraction) {
        float offsetY = (mBottomSheet.getMinOffset() - mBottomSheet.getSheetOffsetFromBottom())
                + mDistanceBelowToolbarPx;
        setTranslationY((int) Math.max(offsetY, 0f));
        setVisibility(MathUtils.areFloatsEqual(transitionFraction, 0f) ? View.GONE : View.VISIBLE);
    }

    @Override
    public void onSheetOpened() {}

    @Override
    public void onSheetClosed() {}

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
}
