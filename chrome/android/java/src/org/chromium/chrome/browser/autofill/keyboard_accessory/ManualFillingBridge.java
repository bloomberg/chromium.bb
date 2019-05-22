// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import android.graphics.Bitmap;
import android.support.annotation.Px;

import org.chromium.base.Callback;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.autofill.keyboard_accessory.data.KeyboardAccessoryData.AccessorySheetData;
import org.chromium.chrome.browser.autofill.keyboard_accessory.data.KeyboardAccessoryData.Action;
import org.chromium.chrome.browser.autofill.keyboard_accessory.data.KeyboardAccessoryData.FooterCommand;
import org.chromium.chrome.browser.autofill.keyboard_accessory.data.KeyboardAccessoryData.UserInfo;
import org.chromium.chrome.browser.autofill.keyboard_accessory.data.PropertyProvider;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

class ManualFillingBridge {
    private final PropertyProvider<AccessorySheetData> mSheetDataProvider =
            new PropertyProvider<>();
    private final PropertyProvider<Action[]> mActionProvider =
            new PropertyProvider<>(AccessoryAction.GENERATE_PASSWORD_AUTOMATIC);
    private final ManualFillingComponent mManualFillingComponent;
    private final ChromeActivity mActivity;
    private long mNativeView;

    private ManualFillingBridge(long nativeView, WindowAndroid windowAndroid) {
        mNativeView = nativeView;
        mActivity = (ChromeActivity) windowAndroid.getActivity().get();
        mManualFillingComponent = mActivity.getManualFillingComponent();
        mManualFillingComponent.registerPasswordProvider(mSheetDataProvider);
        mManualFillingComponent.registerCreditCardProvider();
        mManualFillingComponent.registerActionProvider(mActionProvider);
    }

    @CalledByNative
    private static ManualFillingBridge create(long nativeView, WindowAndroid windowAndroid) {
        return new ManualFillingBridge(nativeView, windowAndroid);
    }

    @CalledByNative
    private void onItemsAvailable(Object objAccessorySheetData) {
        mSheetDataProvider.notifyObservers((AccessorySheetData) objAccessorySheetData);
    }

    @CalledByNative
    private void onAutomaticGenerationStatusChanged(boolean available) {
        final Action[] generationAction;
        if (available) {
            // This is meant to suppress the warning that the short string is not used.
            // TODO(crbug.com/855581): Switch between strings based on whether they fit on the
            // screen or not.
            boolean useLongString = true;
            String caption = useLongString
                    ? mActivity.getString(R.string.password_generation_accessory_button)
                    : mActivity.getString(R.string.password_generation_accessory_button_short);
            generationAction = new Action[] {
                    new Action(caption, AccessoryAction.GENERATE_PASSWORD_AUTOMATIC, (action) -> {
                        assert mNativeView
                                != 0
                            : "Controller has been destroyed but the bridge wasn't cleaned up!";
                        ManualFillingMetricsRecorder.recordActionSelected(
                                AccessoryAction.GENERATE_PASSWORD_AUTOMATIC);
                        mManualFillingComponent.dismiss();
                        nativeOnGenerationRequested(mNativeView);
                    })};
        } else {
            generationAction = new Action[0];
        }
        mActionProvider.notifyObservers(generationAction);
    }

    @CalledByNative
    void showWhenKeyboardIsVisible() {
        mManualFillingComponent.showWhenKeyboardIsVisible();
    }

    @CalledByNative
    void hide() {
        mManualFillingComponent.hide();
    }

    @CalledByNative
    private void closeAccessorySheet() {
        mManualFillingComponent.closeAccessorySheet();
    }

    @CalledByNative
    private void swapSheetWithKeyboard() {
        mManualFillingComponent.swapSheetWithKeyboard();
    }

    @CalledByNative
    private void destroy() {
        mSheetDataProvider.notifyObservers(null);
        mNativeView = 0;
    }

    @CalledByNative
    private static Object createAccessorySheetData(@FallbackSheetType int type, String title) {
        return new AccessorySheetData(type, title);
    }

    @CalledByNative
    private Object addUserInfoToAccessorySheetData(Object objAccessorySheetData) {
        UserInfo userInfo = new UserInfo(this::fetchFavicon);
        ((AccessorySheetData) objAccessorySheetData).getUserInfoList().add(userInfo);
        return userInfo;
    }

    @CalledByNative
    private void addFieldToUserInfo(Object objUserInfo, String displayText, String a11yDescription,
            boolean isObfuscated, boolean selectable) {
        Callback<UserInfo.Field> callback = null;
        if (selectable) {
            callback = (field) -> {
                assert mNativeView != 0 : "Controller was destroyed but the bridge wasn't!";
                ManualFillingMetricsRecorder.recordSuggestionSelected(AccessoryTabType.PASSWORDS,
                        field.isObfuscated() ? AccessorySuggestionType.PASSWORD
                                             : AccessorySuggestionType.USERNAME);
                nativeOnFillingTriggered(mNativeView, field.isObfuscated(), field.getDisplayText());
            };
        }
        ((UserInfo) objUserInfo)
                .getFields()
                .add(new UserInfo.Field(displayText, a11yDescription, isObfuscated, callback));
    }

    @CalledByNative
    private void addFooterCommandToAccessorySheetData(
            Object objAccessorySheetData, String displayText) {
        ((AccessorySheetData) objAccessorySheetData)
                .getFooterCommands()
                .add(new FooterCommand(displayText, (footerCommand) -> {
                    assert mNativeView != 0 : "Controller was destroyed but the bridge wasn't!";
                    nativeOnOptionSelected(mNativeView, footerCommand.getDisplayText());
                }));
    }

    public void fetchFavicon(@Px int desiredSize, Callback<Bitmap> faviconCallback) {
        assert mNativeView != 0 : "Favicon was requested after the bridge was destroyed!";
        nativeOnFaviconRequested(mNativeView, desiredSize, faviconCallback);
    }

    @VisibleForTesting
    public static void cachePasswordSheetData(
            WebContents webContents, String[] userNames, String[] passwords) {
        nativeCachePasswordSheetDataForTesting(webContents, userNames, passwords);
    }

    private native void nativeOnFaviconRequested(long nativeManualFillingViewAndroid,
            int desiredSizeInPx, Callback<Bitmap> faviconCallback);
    private native void nativeOnFillingTriggered(
            long nativeManualFillingViewAndroid, boolean isObfuscated, String textToFill);
    private native void nativeOnOptionSelected(
            long nativeManualFillingViewAndroid, String selectedOption);
    private native void nativeOnGenerationRequested(long nativeManualFillingViewAndroid);

    private static native void nativeCachePasswordSheetDataForTesting(
            WebContents webContents, String[] userNames, String[] passwords);
}