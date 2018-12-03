// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static org.chromium.chrome.browser.autofill.keyboard_accessory.PasswordAccessorySheetProperties.CREDENTIALS;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.PasswordAccessorySheetProperties.SCROLL_LISTENER;

import android.support.annotation.Nullable;
import android.support.v7.widget.RecyclerView;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.AccessorySheetData;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Item;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.UserInfo;
import org.chromium.chrome.browser.modelutil.ListModel;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;

/**
 * This class contains all logic for the password accessory sheet component. Changes to its internal
 * {@link PropertyModel} are observed by a {@link PropertyModelChangeProcessor} and affect the
 * {@link PasswordAccessorySheetView}.
 */
class PasswordAccessorySheetMediator implements KeyboardAccessoryData.Observer<AccessorySheetData> {
    private final PropertyModel mModel;

    @Override
    public void onItemAvailable(int typeId, AccessorySheetData accessorySheetData) {
        mModel.get(CREDENTIALS).set(convertToItems(accessorySheetData));
    }

    PasswordAccessorySheetMediator(
            PropertyModel model, @Nullable RecyclerView.OnScrollListener scrollListener) {
        mModel = model;
        mModel.set(SCROLL_LISTENER, scrollListener);
    }

    void onTabShown() {
        KeyboardAccessoryMetricsRecorder.recordActionImpression(AccessoryAction.MANAGE_PASSWORDS);
        KeyboardAccessoryMetricsRecorder.recordSheetSuggestions(
                AccessoryTabType.PASSWORDS, mModel.get(CREDENTIALS));
    }

    private Item[] convertToItems(AccessorySheetData accessorySheetData) {
        if (accessorySheetData == null) return new Item[0];

        List<Item> items = new ArrayList<>();

        items.add(Item.createTopDivider());
        items.add(Item.createLabel(accessorySheetData.getTitle(), accessorySheetData.getTitle()));

        for (UserInfo userInfo : accessorySheetData.getUserInfoList()) {
            for (UserInfo.Field field : userInfo.getFields()) {
                items.add(Item.createSuggestion(field.getDisplayText(), field.getA11yDescription(),
                        field.isObfuscated(),
                        item -> field.triggerSelection(), userInfo.getFaviconProvider()));
            }
        }

        if (!accessorySheetData.getFooterCommands().isEmpty()) items.add(Item.createDivider());
        for (KeyboardAccessoryData.FooterCommand command : accessorySheetData.getFooterCommands()) {
            items.add(Item.createOption(
                    command.getDisplayText(), command.getDisplayText(), (i) -> command.execute()));
        }

        return items.toArray(new Item[0]);
    }

    @VisibleForTesting
    ListModel<Item> getModelForTesting() {
        return mModel.get(CREDENTIALS);
    }
}
