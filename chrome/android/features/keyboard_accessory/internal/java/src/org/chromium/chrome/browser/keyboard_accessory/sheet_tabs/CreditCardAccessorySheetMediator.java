// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryTabType;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.AccessorySheetData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.FooterCommand;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.UserInfo;
import org.chromium.chrome.browser.keyboard_accessory.data.Provider;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabModel.AccessorySheetDataPiece;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabModel.AccessorySheetDataPiece.Type;

import java.util.ArrayList;
import java.util.List;

class CreditCardAccessorySheetMediator implements Provider.Observer<AccessorySheetData> {
    private final AccessorySheetTabModel mModel;

    @Override
    public void onItemAvailable(int typeId, AccessorySheetData accessorySheetData) {
        List<AccessorySheetDataPiece> items = new ArrayList<>();

        if (accessorySheetData != null) {
            if (shouldShowTitle(accessorySheetData.getUserInfoList())) {
                items.add(new AccessorySheetDataPiece(accessorySheetData.getTitle(), Type.TITLE));
            }
            for (UserInfo info : accessorySheetData.getUserInfoList()) {
                items.add(new AccessorySheetDataPiece(info, Type.CREDIT_CARD_INFO));
            }
            for (FooterCommand command : accessorySheetData.getFooterCommands()) {
                items.add(new AccessorySheetDataPiece(command, Type.FOOTER_COMMAND));
            }
        }

        mModel.set(items.toArray(new AccessorySheetDataPiece[0]));
    }

    CreditCardAccessorySheetMediator(AccessorySheetTabModel model) {
        mModel = model;
    }

    void onTabShown() {
        AccessorySheetTabMetricsRecorder.recordSheetSuggestions(
                AccessoryTabType.CREDIT_CARDS, mModel);
    }

    private boolean shouldShowTitle(List<UserInfo> userInfoList) {
        return !ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY)
                || userInfoList.isEmpty();
    }
}
