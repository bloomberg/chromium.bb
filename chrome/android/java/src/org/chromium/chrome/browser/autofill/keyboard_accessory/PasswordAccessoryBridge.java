// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Item;
import org.chromium.ui.base.WindowAndroid;

class PasswordAccessoryBridge {
    private final KeyboardAccessoryData.PropertyProvider<Item> mItemProvider =
            new KeyboardAccessoryData.PropertyProvider<>();
    private final KeyboardAccessoryData.Tab mTab;
    private final ManualFillingCoordinator mManualFillingCoordinator;
    private long mNativeView;

    private PasswordAccessoryBridge(long nativeView, WindowAndroid windowAndroid) {
        mNativeView = nativeView;
        ChromeActivity activity = (ChromeActivity) windowAndroid.getActivity().get();
        mManualFillingCoordinator = activity.getManualFillingController();
        // TODO(fhorschig): This belongs into the ManualFillingCoordinator.
        PasswordAccessorySheetCoordinator passwordAccessorySheet =
                new PasswordAccessorySheetCoordinator(activity);
        mTab = passwordAccessorySheet.createTab();
        passwordAccessorySheet.registerItemProvider(mItemProvider);
        mManualFillingCoordinator.addTab(mTab);
    }

    @CalledByNative
    private static PasswordAccessoryBridge create(long nativeView, WindowAndroid windowAndroid) {
        return new PasswordAccessoryBridge(nativeView, windowAndroid);
    }

    @CalledByNative
    private void onItemsAvailable(
            String[] text, String[] description, int[] isPassword, int[] itemType) {
        mItemProvider.notifyObservers(convertToItems(text, description, isPassword, itemType));
    }

    @CalledByNative
    private void destroy() {
        mNativeView = 0;
        mManualFillingCoordinator.removeTab(mTab);
    }

    private Item[] convertToItems(
            String[] text, String[] description, int[] isPassword, int[] itemType) {
        Item[] items = new Item[text.length];
        for (int i = 0; i < text.length; i++) {
            final String textToFill = text[i];
            final String contentDescription = description[i];
            final int type = itemType[i];
            final boolean password = isPassword[i] == 1;
            items[i] = new Item() {
                @Override
                public int getType() {
                    return type;
                }

                @Override
                public String getCaption() {
                    return textToFill;
                }

                @Override
                public String getContentDescription() {
                    return contentDescription;
                }

                @Override
                public boolean isPassword() {
                    return password;
                }

                @Override
                public Callback<Item> getItemSelectedCallback() {
                    return (item) -> {
                        assert mNativeView
                                != 0
                            : "Controller has been destroyed but the bridge wasn't cleaned up!";
                        nativeOnFillingTriggered(mNativeView, item.getCaption());
                    };
                }
            };
        }
        return items;
    }

    private native void nativeOnFillingTriggered(
            long nativePasswordAccessoryViewAndroid, String textToFill);
}