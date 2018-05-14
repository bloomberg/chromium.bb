// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import android.view.View;
import android.view.ViewStub;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.modelutil.LazyViewBinderAdapter;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor;

/**
 * Creates and owns all elements which are part of the accessory sheet component.
 * It's part of the controller but will mainly forward events (like showing the sheet) and handle
 * communication with the {@link ManualFillingController} (e.g. add a tab to trigger the sheet).
 * to the {@link AccessorySheetMediator}.
 */
public class AccessorySheetCoordinator {
    private final AccessorySheetMediator mMediator;

    /**
     * Creates the sheet component by instantiating Model, View and Controller before wiring these
     * parts up.
     * @param viewStub The view stub that can be inflated into the accessory layout.
     */
    public AccessorySheetCoordinator(ViewStub viewStub) {
        LazyViewBinderAdapter.StubHolder<View> stubHolder =
                new LazyViewBinderAdapter.StubHolder<>(viewStub);
        AccessorySheetModel model = new AccessorySheetModel();
        model.addObserver(new PropertyModelChangeProcessor<>(
                model, stubHolder, new LazyViewBinderAdapter<>(new AccessorySheetViewBinder())));
        mMediator = new AccessorySheetMediator(model);
    }

    /**
     * Returns a {@link KeyboardAccessoryData.Tab} object that is used to display this bottom sheet.
     * @return Returns a {@link KeyboardAccessoryData.Tab}.
     */
    public KeyboardAccessoryData.Tab getTab() {
        return mMediator.getTab();
    }

    @VisibleForTesting
    AccessorySheetMediator getMediatorForTesting() {
        return mMediator;
    }
}