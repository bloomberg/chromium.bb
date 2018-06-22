// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Item;
import org.chromium.ui.base.WindowAndroid;

class PasswordAccessoryBridge {
    private final KeyboardAccessoryData.PropertyProvider<Item> mItemProvider =
            new KeyboardAccessoryData.PropertyProvider<>();
    private final ManualFillingCoordinator mManualFillingCoordinator;
    private long mNativeView;

    private PasswordAccessoryBridge(long nativeView, WindowAndroid windowAndroid) {
        mNativeView = nativeView;
        ChromeActivity activity = (ChromeActivity) windowAndroid.getActivity().get();
        mManualFillingCoordinator = activity.getManualFillingController();
        mManualFillingCoordinator.registerPasswordProvider(mItemProvider);
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
        mItemProvider.notifyObservers(new Item[] {}); // There are no more items available!
        mNativeView = 0;
    }

    private Item[] convertToItems(
            String[] text, String[] description, int[] isPassword, int[] type) {
        Item[] items = new Item[text.length];
        for (int i = 0; i < text.length; i++) {
            switch (type[i]) {
                case ItemType.LABEL:
                    items[i] = Item.createLabel(text[i], description[i]);
                    continue;
                case ItemType.SUGGESTION:
                    items[i] = Item.createSuggestion(
                            text[i], description[i], isPassword[i] == 1, (item) -> {
                                assert mNativeView
                                        != 0 : "Controller was destroyed but the bridge wasn't!";
                                nativeOnFillingTriggered(
                                        mNativeView, item.isPassword(), item.getCaption());
                            });
                    continue;
                case ItemType.NON_INTERACTIVE_SUGGESTION:
                    items[i] = Item.createSuggestion(
                            text[i], description[i], isPassword[i] == 1, null);
                    continue;
                case ItemType.DIVIDER:
                    items[i] = Item.createDivider();
                    continue;
                case ItemType.OPTION:
                    items[i] = Item.createOption(text[i], description[i], (item) -> {
                        assert mNativeView != 0 : "Controller was destroyed but the bridge wasn't!";
                        nativeOnOptionSelected(mNativeView, item.getCaption());
                    });
                    continue;
            }
            assert false : "Cannot create item for type '" + type[i] + "'.";
        }
        return items;
    }

    private native void nativeOnFillingTriggered(
            long nativePasswordAccessoryViewAndroid, boolean isPassword, String textToFill);
    private native void nativeOnOptionSelected(
            long nativePasswordAccessoryViewAndroid, String selectedOption);
}