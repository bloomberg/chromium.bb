// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static org.chromium.chrome.browser.autofill.keyboard_accessory.PasswordAccessorySheetProperties.CREDENTIALS;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.PasswordAccessorySheetProperties.PASSWORD_SHEET_DATA;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.PasswordAccessorySheetProperties.SCROLL_LISTENER;

import android.support.annotation.Nullable;
import android.support.v7.widget.RecyclerView;

import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.AccessorySheetData;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.FooterCommand;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Item;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.UserInfo;
import org.chromium.chrome.browser.autofill.keyboard_accessory.PasswordAccessorySheetProperties.AccessorySheetDataPiece;
import org.chromium.chrome.browser.autofill.keyboard_accessory.PasswordAccessorySheetProperties.AccessorySheetDataPiece.Type;
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
        mModel.get(PASSWORD_SHEET_DATA).set(splitIntoDataPieces(accessorySheetData));
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
                        field.isSelectable() ? (item -> field.triggerSelection()) : null,
                        userInfo.getFaviconProvider()));
            }
        }

        if (!accessorySheetData.getFooterCommands().isEmpty()) items.add(Item.createDivider());
        for (FooterCommand command : accessorySheetData.getFooterCommands()) {
            items.add(Item.createOption(
                    command.getDisplayText(), command.getDisplayText(), (i) -> command.execute()));
        }

        return items.toArray(new Item[0]);
    }

    private AccessorySheetDataPiece[] splitIntoDataPieces(AccessorySheetData accessorySheetData) {
        if (accessorySheetData == null) return new AccessorySheetDataPiece[0];

        List<AccessorySheetDataPiece> items = new ArrayList<>();
        items.add(new AccessorySheetDataPiece(accessorySheetData.getTitle(), Type.TITLE));
        for (UserInfo userInfo : accessorySheetData.getUserInfoList()) {
            items.add(new AccessorySheetDataPiece(userInfo, Type.PASSWORD_INFO));
        }
        for (FooterCommand command : accessorySheetData.getFooterCommands()) {
            items.add(new AccessorySheetDataPiece(command, Type.FOOTER_COMMAND));
        }

        return items.toArray(new AccessorySheetDataPiece[0]);
    }
}
