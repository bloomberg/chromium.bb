// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static org.chromium.chrome.browser.autofill.keyboard_accessory.AccessorySheetTrigger.MANUAL_CLOSE;

import android.support.annotation.Nullable;
import android.support.design.widget.TabLayout;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.autofill.AutofillKeyboardSuggestions;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryCoordinator.VisibilityDelegate;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Action;
import org.chromium.chrome.browser.modelutil.ListObservable;
import org.chromium.chrome.browser.modelutil.PropertyObservable;
import org.chromium.ui.base.WindowAndroid;

/**
 * This is the second part of the controller of the keyboard accessory component.
 * It is responsible to update the {@link KeyboardAccessoryModel} based on Backend calls and notify
 * the Backend if the {@link KeyboardAccessoryModel} changes.
 * From the backend, it receives all actions that the accessory can perform (most prominently
 * generating passwords) and lets the {@link KeyboardAccessoryModel} know of these actions and which
 * callback to trigger when selecting them.
 */
class KeyboardAccessoryMediator
        implements WindowAndroid.KeyboardVisibilityListener, ListObservable.ListObserver<Void>,
                   PropertyObservable.PropertyObserver<KeyboardAccessoryModel.PropertyKey>,
                   KeyboardAccessoryData.Observer<KeyboardAccessoryData.Action>,
                   TabLayout.OnTabSelectedListener {
    private final KeyboardAccessoryModel mModel;
    private final WindowAndroid mWindowAndroid;
    private final VisibilityDelegate mVisibilityDelegate;

    // TODO(fhorschig): Look for stronger signals than |keyboardVisibilityChanged|.
    // This variable remembers the last state of |keyboardVisibilityChanged| which might not be
    // sufficient for edge cases like hardware keyboards, floating keyboards, etc.
    private boolean mIsKeyboardVisible;

    KeyboardAccessoryMediator(KeyboardAccessoryModel model, WindowAndroid windowAndroid,
            VisibilityDelegate visibilityDelegate) {
        mModel = model;
        mWindowAndroid = windowAndroid;
        mVisibilityDelegate = visibilityDelegate;
        windowAndroid.addKeyboardVisibilityListener(this);

        // Add mediator as observer so it can use model changes as signal for accessory visibility.
        mModel.addObserver(this);
        mModel.getTabList().addObserver(this);
        mModel.getActionList().addObserver(this);
        mModel.setTabSelectionCallbacks(this);
    }

    void destroy() {
        mWindowAndroid.removeKeyboardVisibilityListener(this);
    }

    @Override
    public void onItemsAvailable(KeyboardAccessoryData.Action[] actions) {
        mModel.setActions(actions);
    }

    @Override
    public void keyboardVisibilityChanged(boolean isShowing) {
        if (isShowing) closeActiveTab();
        mIsKeyboardVisible = isShowing;
        updateVisibility();
    }

    void addTab(KeyboardAccessoryData.Tab tab) {
        mModel.addTab(tab);
    }

    void removeTab(KeyboardAccessoryData.Tab tab) {
        mModel.removeTab(tab);
    }

    void setTabs(KeyboardAccessoryData.Tab[] tabs) {
        mModel.getTabList().set(tabs);
    }

    void setSuggestions(AutofillKeyboardSuggestions suggestions) {
        mModel.setAutofillSuggestions(suggestions);
    }

    void dismiss() {
        mModel.setActiveTab(null);
        if (mModel.getAutofillSuggestions() != null) {
            mModel.getAutofillSuggestions().dismiss();
            mModel.setAutofillSuggestions(null);
        }
        updateVisibility();
    }

    void closeActiveTab() {
        mModel.setActiveTab(null);
    }

    @VisibleForTesting
    KeyboardAccessoryModel getModelForTesting() {
        return mModel;
    }

    @Override
    public void onItemRangeInserted(ListObservable source, int index, int count) {
        assert source == mModel.getActionList() || source == mModel.getTabList();
        updateVisibility();
    }

    @Override
    public void onItemRangeRemoved(ListObservable source, int index, int count) {
        assert source == mModel.getActionList() || source == mModel.getTabList();
        updateVisibility();
    }

    @Override
    public void onItemRangeChanged(
            ListObservable source, int index, int count, @Nullable Void payload) {
        assert source == mModel.getActionList() || source == mModel.getTabList();
        assert payload == null;
        updateVisibility();
    }

    @Override
    public void onPropertyChanged(PropertyObservable<KeyboardAccessoryModel.PropertyKey> source,
            @Nullable KeyboardAccessoryModel.PropertyKey propertyKey) {
        // Update the visibility only if we haven't set it just now.
        if (propertyKey == KeyboardAccessoryModel.PropertyKey.VISIBLE) {
            // When the accessory just (dis)appeared, there should be no active tab.
            mModel.setActiveTab(null);
            if (!mModel.isVisible()) {
                mModel.setActions(new Action[0]);
            }
            return;
        }
        if (propertyKey == KeyboardAccessoryModel.PropertyKey.ACTIVE_TAB) {
            Integer activeTab = mModel.activeTab();
            if (activeTab == null) {
                mVisibilityDelegate.onCloseAccessorySheet();
                return;
            }
            mVisibilityDelegate.onChangeAccessorySheet(activeTab);
            mVisibilityDelegate.onOpenAccessorySheet();
            return;
        }
        if (propertyKey == KeyboardAccessoryModel.PropertyKey.TAB_SELECTION_CALLBACKS) {
            return;
        }
        if (propertyKey == KeyboardAccessoryModel.PropertyKey.SUGGESTIONS) {
            updateVisibility();
            return;
        }
        assert false : "Every property update needs to be handled explicitly!";
    }

    @Override
    public void onTabSelected(TabLayout.Tab tab) {
        mModel.setActiveTab(tab.getPosition());
    }

    @Override
    public void onTabUnselected(TabLayout.Tab tab) {}

    @Override
    public void onTabReselected(TabLayout.Tab tab) {
        if (mModel.activeTab() == null) {
            mModel.setActiveTab(tab.getPosition());
        } else {
            KeyboardAccessoryMetricsRecorder.recordSheetTrigger(
                    mModel.getTabList().get(mModel.activeTab()).getRecordingType(), MANUAL_CLOSE);
            mModel.setActiveTab(null);
            mVisibilityDelegate.onOpenKeyboard();
        }
    }

    private boolean shouldShowAccessory() {
        if (!mIsKeyboardVisible && mModel.activeTab() == null) return false;
        return mModel.getAutofillSuggestions() != null || mModel.getActionList().size() > 0
                || mModel.getTabList().size() > 0;
    }

    private void updateVisibility() {
        mModel.setVisible(shouldShowAccessory());
    }
}
