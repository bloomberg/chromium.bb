// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import android.content.Context;
import android.support.annotation.Nullable;
import android.support.v7.content.res.AppCompatResources;
import android.support.v7.widget.RecyclerView;

import org.chromium.chrome.browser.keyboard_accessory.AccessoryTabType;
import org.chromium.chrome.browser.keyboard_accessory.R;

/**
 * This component is a tab that can be added to the ManualFillingCoordinator. This tab
 * allows selecting credit card information from a sheet below the keyboard accessory.
 */
public class CreditCardAccessorySheetCoordinator extends AccessorySheetTabCoordinator {
    /**
     * Creates the credit cards tab.
     * @param context The {@link Context} containing resources like icons and layouts for this tab.
     * @param scrollListener An optional listener that will be bound to the inflated recycler view.
     */
    public CreditCardAccessorySheetCoordinator(
            Context context, @Nullable RecyclerView.OnScrollListener scrollListener) {
        super( // TODO(crbug.com/926365): Add an appropriate title, icon, and restructure this class
               // to use an Icon Provider with a static instance of the resource.
                context.getString(R.string.autofill_payment_methods),
                AppCompatResources.getDrawable(context, R.drawable.ic_info_outline_grey),
                // TODO(crbug.com/926365): Add strings and resources properly.
                "Open credit card sheet", "Credit card sheet is open",
                // TODO(crbug.com/926365): Add a new layout, or generalize the existing one.
                R.layout.password_accessory_sheet, AccessoryTabType.CREDIT_CARDS, scrollListener);
    }

    @Override
    public void onTabShown() {}
}
