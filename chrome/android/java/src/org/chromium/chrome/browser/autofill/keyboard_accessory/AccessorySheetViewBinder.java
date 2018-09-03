// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import android.support.v4.view.ViewPager;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.browser.autofill.keyboard_accessory.AccessorySheetModel.PropertyKey;

/**
 * Observes {@link AccessorySheetModel} changes (like a newly available tab) and triggers the
 * {@link AccessorySheetViewBinder} which will modify the view accordingly.
 */
class AccessorySheetViewBinder {
    public static void bind(
            AccessorySheetModel model, ViewPager viewPager, PropertyKey propertyKey) {
        if (propertyKey == PropertyKey.TAB_LIST) {
            viewPager.setAdapter(
                    AccessorySheetCoordinator.createTabViewAdapter(model.getTabList(), viewPager));
        } else if (propertyKey == PropertyKey.VISIBLE) {
            viewPager.setVisibility(model.isVisible() ? View.VISIBLE : View.GONE);
        } else if (propertyKey == PropertyKey.HEIGHT) {
            ViewGroup.LayoutParams p = viewPager.getLayoutParams();
            p.height = model.getHeight();
            viewPager.setLayoutParams(p);
        } else if (propertyKey == PropertyKey.ACTIVE_TAB_INDEX) {
            if (model.getActiveTabIndex() != AccessorySheetModel.NO_ACTIVE_TAB) {
                viewPager.setCurrentItem(model.getActiveTabIndex());
            }
        } else {
            assert false : "Every possible property update needs to be handled!";
        }
    }
}
