// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static org.chromium.chrome.browser.autofill.keyboard_accessory.AccessorySheetTrigger.MANUAL_CLOSE;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryProperties.ACTIONS;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryProperties.BOTTOM_OFFSET_PX;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryProperties.KEYBOARD_TOGGLE_VISIBLE;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryProperties.SHOW_KEYBOARD_CALLBACK;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryProperties.VISIBLE;

import android.support.annotation.Nullable;
import android.support.annotation.Px;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryCoordinator.TabSwitchingDelegate;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryCoordinator.VisibilityDelegate;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Action;
import org.chromium.chrome.browser.modelutil.ListObservable;
import org.chromium.chrome.browser.modelutil.PropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.modelutil.PropertyObservable;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * This is the second part of the controller of the keyboard accessory component.
 * It is responsible for updating the model based on backend calls and notify the backend if the
 * model changes. From the backend, it receives all actions that the accessory can perform (most
 * prominently generating passwords) and lets the model know of these actions and which callback to
 * trigger when selecting them.
 */
class KeyboardAccessoryMediator
        implements ListObservable.ListObserver<Void>,
                   PropertyObservable.PropertyObserver<PropertyKey>,
                   KeyboardAccessoryData.Observer<KeyboardAccessoryData.Action[]>,
                   KeyboardAccessoryTabLayoutCoordinator.AccessoryTabObserver {
    private final PropertyModel mModel;
    private final VisibilityDelegate mVisibilityDelegate;
    private final TabSwitchingDelegate mTabSwitcher;

    private boolean mShowIfNotEmpty;

    KeyboardAccessoryMediator(PropertyModel model, VisibilityDelegate visibilityDelegate,
            TabSwitchingDelegate tabSwitcher) {
        mModel = model;
        mVisibilityDelegate = visibilityDelegate;
        mTabSwitcher = tabSwitcher;

        // Add mediator as observer so it can use model changes as signal for accessory visibility.
        mModel.set(SHOW_KEYBOARD_CALLBACK, this::closeSheet);
        mModel.get(ACTIONS).addObserver(this);
        mModel.addObserver(this);
    }

    @Override
    public void onItemAvailable(int typeId, KeyboardAccessoryData.Action[] actions) {
        assert typeId != DEFAULT_TYPE : "Did not specify which Action type has been updated.";
        // If there is a new list, retain all actions that are of a different type than the provided
        // actions.
        List<Action> retainedActions = new ArrayList<>();
        for (Action a : mModel.get(ACTIONS)) {
            if (a.getActionType() == typeId) continue;
            retainedActions.add(a);
        }
        // Append autofill suggestions to the end, right before the tab switcher.
        int insertPos = typeId == AccessoryAction.AUTOFILL_SUGGESTION ? retainedActions.size() : 0;
        retainedActions.addAll(insertPos, Arrays.asList(actions));
        mModel.get(ACTIONS).set(retainedActions);
    }

    void requestShowing() {
        mShowIfNotEmpty = true;
        updateVisibility();
    }

    void close() {
        mShowIfNotEmpty = false;
        updateVisibility();
    }

    void dismiss() {
        mTabSwitcher.closeActiveTab();
        close();
    }

    @VisibleForTesting
    PropertyModel getModelForTesting() {
        return mModel;
    }

    @Override
    public void onItemRangeInserted(ListObservable source, int index, int count) {
        assert source == mModel.get(ACTIONS);
        updateVisibility();
    }

    @Override
    public void onItemRangeRemoved(ListObservable source, int index, int count) {
        assert source == mModel.get(ACTIONS);
        updateVisibility();
    }

    @Override
    public void onItemRangeChanged(
            ListObservable source, int index, int count, @Nullable Void payload) {
        assert source == mModel.get(ACTIONS);
        assert payload == null;
        updateVisibility();
    }

    @Override
    public void onPropertyChanged(
            PropertyObservable<PropertyKey> source, @Nullable PropertyKey propertyKey) {
        // Update the visibility only if we haven't set it just now.
        if (propertyKey == VISIBLE) {
            // When the accessory just (dis)appeared, there should be no active tab.
            mTabSwitcher.closeActiveTab();
            mVisibilityDelegate.onBottomControlSpaceChanged();
            if (!mModel.get(VISIBLE)) {
                // TODO(fhorschig|ioanap): Maybe the generation bridge should take care of that.
                onItemAvailable(AccessoryAction.GENERATE_PASSWORD_AUTOMATIC, new Action[0]);
            }
            return;
        }
        if (propertyKey == BOTTOM_OFFSET_PX || propertyKey == SHOW_KEYBOARD_CALLBACK
                || propertyKey == KEYBOARD_TOGGLE_VISIBLE) {
            return;
        }
        assert false : "Every property update needs to be handled explicitly!";
    }

    @Override
    public void onActiveTabChanged(Integer activeTab) {
        mModel.set(KEYBOARD_TOGGLE_VISIBLE, activeTab != null);
        if (activeTab == null) {
            mVisibilityDelegate.onCloseAccessorySheet();
            updateVisibility();
            return;
        }
        mVisibilityDelegate.onChangeAccessorySheet(activeTab);
    }

    @Override
    public void onActiveTabReselected() {
        closeSheet();
    }

    @Override
    public void onTabsChanged() {
        updateVisibility();
    }

    private void closeSheet() {
        assert mTabSwitcher.getActiveTab() != null;
        KeyboardAccessoryMetricsRecorder.recordSheetTrigger(
                mTabSwitcher.getActiveTab().getRecordingType(), MANUAL_CLOSE);
        mModel.set(KEYBOARD_TOGGLE_VISIBLE, false);
        mVisibilityDelegate.onOpenKeyboard(); // This will close the active tab gently.
    }

    boolean hasContents() {
        return mModel.get(ACTIONS).size() > 0 || mTabSwitcher.hasTabs();
    }

    private boolean shouldShowAccessory() {
        if (!mShowIfNotEmpty && mTabSwitcher.getActiveTab() == null) return false;
        return hasContents();
    }

    private void updateVisibility() {
        mModel.set(VISIBLE, shouldShowAccessory());
    }

    public void setBottomOffset(@Px int bottomOffset) {
        mModel.set(BOTTOM_OFFSET_PX, bottomOffset);
    }

    public boolean isShown() {
        return mModel.get(VISIBLE);
    }

    public boolean hasActiveTab() {
        return mModel.get(VISIBLE) && mTabSwitcher.getActiveTab() != null;
    }
}
