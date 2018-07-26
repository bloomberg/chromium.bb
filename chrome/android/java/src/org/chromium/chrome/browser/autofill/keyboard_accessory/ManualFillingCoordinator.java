// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import android.view.ViewStub;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Provider;
import org.chromium.ui.base.WindowAndroid;

/**
 * Handles requests to the manual UI for filling passwords, payments and other user data. Ideally,
 * the caller has no access to Keyboard accessory or sheet and is only interacting with this
 * component.
 * For that, it facilitates the communication between {@link KeyboardAccessoryCoordinator} and
 * {@link AccessorySheetCoordinator} to add and trigger surfaces that may assist users while filling
 * fields.
 */
public class ManualFillingCoordinator {
    private final ManualFillingMediator mMediator = new ManualFillingMediator();

    /**
     * Creates a the manual filling controller.
     * @param windowAndroid The window needed to set up the sub components.
     * @param keyboardAccessoryStub The view stub for keyboard accessory bar.
     * @param accessorySheetStub The view stub for the keyboard accessory bottom sheet.
     */
    public ManualFillingCoordinator(WindowAndroid windowAndroid, ViewStub keyboardAccessoryStub,
            ViewStub accessorySheetStub) {
        assert windowAndroid.getActivity().get() != null;
        KeyboardAccessoryCoordinator keyboardAccessory =
                new KeyboardAccessoryCoordinator(windowAndroid, keyboardAccessoryStub, mMediator);
        AccessorySheetCoordinator accessorySheet = new AccessorySheetCoordinator(
                accessorySheetStub, keyboardAccessory::getPageChangeListener);
        mMediator.initialize(keyboardAccessory, accessorySheet,
                (ChromeActivity) windowAndroid.getActivity().get());
    }

    /**
     * Cleans up the manual UI by destroying the accessory bar and its bottom sheet.
     */
    public void destroy() {
        mMediator.destroy();
    }

    /**
     * Handles tapping on the Android back button.
     * @return Whether tapping the back button dismissed the accessory sheet or not.
     */
    public boolean handleBackPress() {
        return mMediator.handleBackPress();
    }

    /**
     * Requests to close the active tab in the keyboard accessory. If there is no active tab, this
     * is a NoOp.
     */
    public void closeAccessorySheet() {
        mMediator.getKeyboardAccessory().closeActiveTab();
    }

    /**
     * Tries to reopen the keyboard which will implicitly show the keyboard accessory bar again.
     */
    public void openKeyboard() {
        mMediator.onOpenKeyboard();
    }

    void registerActionProvider(Provider<KeyboardAccessoryData.Action> actionProvider) {
        mMediator.registerActionProvider(actionProvider);
    }

    void registerPasswordProvider(Provider<KeyboardAccessoryData.Item> itemProvider) {
        mMediator.registerPasswordProvider(itemProvider);
    }

    // TODO(fhorschig): Should be @VisibleForTesting.
    /**
     * Allows access to the keyboard accessory. This can be used to explicitly modify the the bar of
     * the keyboard accessory (e.g. by providing suggestions or actions).
     * @return The coordinator of the Keyboard accessory component.
     */
    public KeyboardAccessoryCoordinator getKeyboardAccessory() {
        return mMediator.getKeyboardAccessory();
    }

    @VisibleForTesting
    ManualFillingMediator getMediatorForTesting() {
        return mMediator;
    }
}