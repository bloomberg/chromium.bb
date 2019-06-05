// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import android.content.Context;
import android.support.annotation.Nullable;
import android.support.v7.content.res.AppCompatResources;
import android.support.v7.widget.RecyclerView;
import android.view.ViewGroup;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryAction;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryTabType;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingMetricsRecorder;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabModel.AccessorySheetDataPiece;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabModel.AccessorySheetDataPiece.Type;
import org.chromium.ui.modelutil.RecyclerViewAdapter;
import org.chromium.ui.modelutil.SimpleRecyclerViewMcp;

/**
 * This component is a tab that can be added to the ManualFillingCoordinator which shows it
 * as bottom sheet below the keyboard accessory.
 */
public class AddressAccessorySheetCoordinator extends AccessorySheetTabCoordinator {
    private final AccessorySheetTabModel mModel = new AccessorySheetTabModel();
    private final AddressAccessorySheetMediator mMediator =
            new AddressAccessorySheetMediator(mModel);

    /**
     * This class contains all logic for the address accessory sheet component. Changes to its
     * internal
     * {@link PropertyModel} are observed by a {@link PropertyModelChangeProcessor} and affect the
     * address accessory sheet tab view.
     */
    private static class AddressAccessorySheetMediator extends AccessorySheetTabMediator {
        AddressAccessorySheetMediator(AccessorySheetTabModel model) {
            super(model, AccessoryTabType.ADDRESSES, Type.ADDRESS_INFO);
        }

        @Override
        void onTabShown() {
            super.onTabShown();

            // This is a compromise: we log an impression, even if the user didn't scroll down far
            // enough to see it. If we moved it into the view layer (i.e. when the actual button is
            // created and shown), we could record multiple impressions of the user scrolls up and
            // down repeatedly.
            ManualFillingMetricsRecorder.recordActionImpression(AccessoryAction.MANAGE_ADDRESSES);
        }
    }

    /**
     * Creates the address tab.
     * @param context The {@link Context} containing resources like icons and layouts for this tab.
     * @param scrollListener An optional listener that will be bound to the inflated recycler view.
     */
    public AddressAccessorySheetCoordinator(
            Context context, @Nullable RecyclerView.OnScrollListener scrollListener) {
        super(context.getString(R.string.address_accessory_sheet_title),
                AppCompatResources.getDrawable(context, R.drawable.permission_location),
                context.getString(R.string.address_accessory_sheet_toggle),
                context.getString(R.string.address_accessory_sheet_opened),
                R.layout.address_accessory_sheet, AccessoryTabType.ADDRESSES, scrollListener);
    }

    @Override
    public void onTabCreated(ViewGroup view) {
        super.onTabCreated(view);
        AddressAccessorySheetViewBinder.initializeView((RecyclerView) view, mModel);
    }

    @Override
    protected AccessorySheetTabMediator getMediator() {
        return mMediator;
    }

    /**
     * Creates an adapter to an {@link AddressAccessorySheetViewBinder} that is wired
     * up to a model change processor listening to the {@link AccessorySheetTabModel}.
     * @param model the {@link AccessorySheetTabModel} the adapter gets its data from.
     * @return Returns a fully initialized and wired adapter to a AddressAccessorySheetViewBinder.
     */
    static RecyclerViewAdapter<AccessorySheetTabViewBinder.ElementViewHolder, Void> createAdapter(
            AccessorySheetTabModel model) {
        return new RecyclerViewAdapter<>(
                new SimpleRecyclerViewMcp<>(model, AccessorySheetDataPiece::getType,
                        AccessorySheetTabViewBinder.ElementViewHolder::bind),
                AddressAccessorySheetViewBinder::create);
    }

    @VisibleForTesting
    AccessorySheetTabModel getSheetDataPiecesForTesting() {
        return mModel;
    }
}
