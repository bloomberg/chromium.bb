// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.support.v7.widget.RecyclerView;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Item;
import org.chromium.chrome.browser.autofill.keyboard_accessory.PasswordAccessorySheetViewBinder.TextViewHolder;
import org.chromium.chrome.browser.modelutil.RecyclerViewAdapter;
import org.chromium.chrome.browser.modelutil.RecyclerViewModelChangeProcessor;
import org.chromium.chrome.browser.modelutil.SimpleListObservable;

/**
 * This component is a tab that can be added to the {@link ManualFillingCoordinator} which shows it
 * as bottom sheet below the keyboard accessory.
 */
public class PasswordAccessorySheetCoordinator {
    private final Context mContext;
    private final SimpleListObservable<Item> mModel = new SimpleListObservable<>();
    private final KeyboardAccessoryData.Observer<Item> mMediator = mModel::set;

    /**
     * Creates the passwords tab.
     * @param context The {@link Context} containing resources like icons and layouts for this tab.
     */
    public PasswordAccessorySheetCoordinator(Context context) {
        mContext = context;
    }

    /**
     * Creates an adapter to an {@link PasswordAccessorySheetViewBinder} that is wired
     * up to the model change processor which listens to the {@link SimpleListObservable<Item>}.
     * @param model the {@link SimpleListObservable<Item>} the adapter gets its data from.
     * @return Returns a fully initialized and wired adapter to a PasswordAccessorySheetViewBinder.
     */
    static RecyclerViewAdapter<SimpleListObservable<Item>, TextViewHolder> createAdapter(
            SimpleListObservable<Item> model, PasswordAccessorySheetViewBinder viewBinder) {
        RecyclerViewAdapter<SimpleListObservable<Item>, TextViewHolder> adapter =
                new PasswordAccessorySheetViewAdapter<>(model, viewBinder);
        model.addObserver(new RecyclerViewModelChangeProcessor<>(adapter));
        return adapter;
    }

    /**
     * Registered item providers can replace the currently shown data in the password sheet.
     * @param actionProvider The provider this component will listen to.
     */
    public void registerItemProvider(KeyboardAccessoryData.Provider<Item> actionProvider) {
        actionProvider.addObserver(mMediator);
    }

    /**
     * Returns a new Tab object that describes the appearance of this class in the keyboard
     * keyboard accessory or its accessory sheet.
     * @return Returns a new {@link KeyboardAccessoryData.Tab} that is connected to this sheet.
     */
    public KeyboardAccessoryData.Tab createTab() {
        return new KeyboardAccessoryData.Tab() {
            @Override
            public Drawable getIcon() {
                return mContext.getResources().getDrawable(R.drawable.ic_vpn_key_grey);
            }

            @Override
            public String getContentDescription() {
                // TODO(fhorschig): Load resource from strings or via mediator from native side.
                return null;
            }

            @Override
            public int getTabLayout() {
                return R.layout.password_accessory_sheet;
            }

            @Override
            public Listener getListener() {
                final PasswordAccessorySheetViewBinder binder =
                        new PasswordAccessorySheetViewBinder();
                return view
                        -> binder.initializeView((RecyclerView) view,
                                PasswordAccessorySheetCoordinator.createAdapter(mModel, binder));
            }
        };
    }

    @VisibleForTesting
    SimpleListObservable<Item> getModelForTesting() {
        return mModel;
    }
}