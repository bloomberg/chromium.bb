// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static android.support.v7.widget.RecyclerView.OnScrollListener;

import android.support.annotation.IntDef;
import android.support.v7.widget.RecyclerView;

import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.AccessorySheetData;
import org.chromium.chrome.browser.autofill.keyboard_accessory.PasswordAccessorySheetViewBinder.ItemViewHolder;
import org.chromium.chrome.browser.modelutil.ListModel;
import org.chromium.chrome.browser.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

// TODO(crbug.com/902425): Drop the "Password-" as soon as its used for other sheets.
/**
 * This class holds properties that are used to build a model for the password accessory sheet
 * component. These properties store the state of the {@link PasswordAccessorySheetView} which is
 * bound to model using the {@link PasswordAccessorySheetViewBinder}.
 * It is built in the {@link PasswordAccessorySheetCoordinator} and modified by the
 * {@link PasswordAccessorySheetMediator}.
 */
class PasswordAccessorySheetProperties {
    static final ReadableObjectPropertyKey<ListModel<KeyboardAccessoryData.Item>> CREDENTIALS =
            new ReadableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<ListModel<AccessorySheetDataPiece>> PASSWORD_SHEET_DATA =
            new ReadableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<OnScrollListener> SCROLL_LISTENER =
            new WritableObjectPropertyKey<>();

    /**
     * The {@link AccessorySheetData} has to be mapped to single items in a {@link RecyclerView}.
     * This class allows to wrap the {@link AccessorySheetData} into small chunks that are organized
     * in a {@link ListModel}. A specific {@link ItemViewHolder}s is defined for each piece.
     */
    static class AccessorySheetDataPiece {
        @IntDef({Type.TITLE, Type.PASSWORD_INFO, Type.FOOTER_COMMAND})
        @Retention(RetentionPolicy.SOURCE)
        public @interface Type {
            /**
             * An item in title style used to display text. Non-interactive.
             */
            int TITLE = 1;
            /**
             * A section with user credentials.
             */
            int PASSWORD_INFO = 2;
            /**
             * A command at the end of the accessory sheet tab.
             */
            int FOOTER_COMMAND = 3;
        }

        private Object mDataPiece;
        private @Type int mType;

        AccessorySheetDataPiece(Object dataPiece, @Type int type) {
            mDataPiece = dataPiece;
            mType = type;
        }

        public static int getType(AccessorySheetDataPiece accessorySheetDataPiece) {
            return accessorySheetDataPiece.mType;
        }

        public Object getDataPiece() {
            return mDataPiece;
        }
    }

    private PasswordAccessorySheetProperties() {}
}
