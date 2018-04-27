// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import android.view.ViewStub;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.autofill.AutofillKeyboardSuggestions;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor;
import org.chromium.chrome.browser.modelutil.RecyclerViewAdapter;
import org.chromium.chrome.browser.modelutil.RecyclerViewModelChangeProcessor;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.WindowAndroid;

/**
 * Creates and owns all elements which are part of the keyboard accessory component.
 * It's part of the controller but will mainly forward events (like adding a tab,
 * or showing the accessory) to the {@link KeyboardAccessoryMediator}.
 */
public class KeyboardAccessoryCoordinator {
    private final KeyboardAccessoryModel mModel = new KeyboardAccessoryModel();
    private final KeyboardAccessoryMediator mMediator;
    private KeyboardAccessoryViewBinder.AccessoryViewHolder mViewHolder;
    private PropertyModelChangeProcessor<KeyboardAccessoryModel,
            KeyboardAccessoryViewBinder.AccessoryViewHolder, KeyboardAccessoryModel.PropertyKey>
            mModelChangeProcessor;

    /**
     * Initializes the component as soon as the native library is loaded by e.g. starting to listen
     * to keyboard visibility events.
     * @param windowAndroid The window connected to the activity this component lives in.
     * @param viewStub the stub that will become the accessory.
     */
    public KeyboardAccessoryCoordinator(WindowAndroid windowAndroid, ViewStub viewStub) {
        mMediator = new KeyboardAccessoryMediator(mModel, windowAndroid);
        mViewHolder = new KeyboardAccessoryViewBinder.AccessoryViewHolder(viewStub);
        mModelChangeProcessor = new PropertyModelChangeProcessor<>(
                mModel, mViewHolder, new KeyboardAccessoryViewBinder());
        mModel.addObserver(mModelChangeProcessor);
        mModel.addTabListObserver(new RecyclerViewModelChangeProcessor<>(
                new RecyclerViewAdapter<>(mModel.getTabList())));
        mModel.addActionListObserver(new RecyclerViewModelChangeProcessor<>(
                new RecyclerViewAdapter<>(mModel.getActionList())));
    }

    /**
     * Show the keyboard accessory. Independent from the keyboard.
     */
    public void show() {
        mMediator.show();
    }

    /**
     * A {@link KeyboardAccessoryData.Tab} passed into this function will be represented as item at
     * the start of the accessory. It is meant to trigger various bottom sheets.
     * @param tab The tab which contains representation data and links back to a bottom sheet.
     */
    public void addTab(KeyboardAccessoryData.Tab tab) {
        mMediator.addTab(tab);
    }

    /**
     * The {@link KeyboardAccessoryData.Tab} passed into this function will be completely removed
     * from the accessory.
     * @param tab The tab to be removed.
     */
    public void removeTab(KeyboardAccessoryData.Tab tab) {
        mMediator.removeTab(tab);
    }

    /**
     * Allows any {@link KeyboardAccessoryData.ActionListProvider} to communicate with the
     * {@link KeyboardAccessoryMediator} of this component.
     * @param provider The object providing action lists to observers in this component.
     */
    public void registerActionListProvider(KeyboardAccessoryData.ActionListProvider provider) {
        provider.addObserver(mMediator);
    }

    /**
     * Used to clean up the accessory by e.g. unregister listeners to keyboard visibility.
     */
    public void destroy() {
        mMediator.destroy();
    }

    /**
     * TODO(fhorschig): Remove this function. The suggestions bridge should become a provider.
     * Sets a View that will be displayed in a scroll view at the end of the accessory.
     * @param suggestions The suggestions to be rendered into the accessory.
     */
    public void setSuggestions(AutofillKeyboardSuggestions suggestions) {
        mMediator.setSuggestions(suggestions);
    }

    /**
     * TODO(fhorschig): Remove this function. The accessory probably shouldn't know this concept.
     * Dismisses the accessory by hiding it's view, clearing potentially left over suggestions and
     * hiding the keyboard.
     */
    public void dismiss() {
        mMediator.dismiss();
        // TODO(fhorschig): Move hiding to upcoming root controller, drop it or reassign to bauerb@.
        UiUtils.hideKeyboard(mViewHolder.getView());
    }

    @VisibleForTesting
    KeyboardAccessoryMediator getMediatorForTesting() {
        return mMediator;
    }
}
