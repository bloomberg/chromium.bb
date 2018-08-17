// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import android.support.annotation.Nullable;
import android.support.annotation.Px;
import android.view.View;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Action;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Provider;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.SceneChangeObserver;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.ui.DropdownPopupWindow;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.WindowAndroid;

import java.util.HashMap;
import java.util.Map;

/**
 * This part of the manual filling component manages the state of the manual filling flow depending
 * on the currently shown tab.
 */
class ManualFillingMediator
        extends EmptyTabObserver implements KeyboardAccessoryCoordinator.VisibilityDelegate {
    private WindowAndroid mWindowAndroid;
    private @Px int mPreviousControlHeight = 0;
    private final WindowAndroid.KeyboardVisibilityListener mVisibilityListener =
            this::onKeyboardVisibilityChanged;

    /**
     * Provides a cache for a given Provider which can repeat the last notification to all
     * observers.
     */
    private class ActionProviderCacheAdapter extends KeyboardAccessoryData.PropertyProvider<Action>
            implements KeyboardAccessoryData.Observer<Action> {
        private final Tab mTab;
        private Action[] mLastItems;

        /**
         * Creates an adapter that listens to the given |provider| and stores items provided by it.
         * If the observed provider serves a currently visible tab, the data is immediately sent on.
         * @param tab The {@link Tab} which the given Provider should affect immediately.
         * @param provider The {@link Provider} to observe and whose data to cache.
         * @param defaultItems The items to be notified about if the Provider hasn't provided any.
         */
        ActionProviderCacheAdapter(Tab tab, KeyboardAccessoryData.PropertyProvider<Action> provider,
                Action[] defaultItems) {
            super(provider.mType);
            mTab = tab;
            provider.addObserver(this);
            mLastItems = defaultItems;
        }

        /**
         * Calls {@link #onItemsAvailable} with the last used items again. If there haven't been
         * any calls, call it with an empty list to avoid putting observers in an undefined state.
         */
        void notifyAboutCachedItems() {
            notifyObservers(mLastItems);
        }

        @Override
        public void onItemsAvailable(int typeId, Action[] actions) {
            mLastItems = actions;
            // Update the contents immediately, if the adapter connects to an active element.
            if (mTab == mActiveBrowserTab) notifyObservers(actions);
        }
    }

    /**
     * This class holds all data that is necessary to restore the state of the Keyboard accessory
     * and its sheet for a given tab.
     */
    @VisibleForTesting
    static class AccessoryState {
        @Nullable
        ActionProviderCacheAdapter mActionsProvider;
        @Nullable
        PasswordAccessorySheetCoordinator mPasswordAccessorySheet;
    }

    // TODO(fhorschig): Do we need a MapObservable type? (This would be only observer though).
    private final Map<Tab, AccessoryState> mModel = new HashMap<>();
    private KeyboardAccessoryCoordinator mKeyboardAccessory;
    private AccessorySheetCoordinator mAccessorySheet;
    private ChromeActivity mActivity; // Used to control the keyboard.
    private TabModelSelectorTabModelObserver mTabModelObserver;
    private Tab mActiveBrowserTab;
    private DropdownPopupWindow mPopup;

    private final SceneChangeObserver mTabSwitcherObserver = new SceneChangeObserver() {
        @Override
        public void onTabSelectionHinted(int tabId) {}

        @Override
        public void onSceneChange(Layout layout) {
            // Includes events like side-swiping between tabs and triggering contextual search.
            mKeyboardAccessory.dismiss();
        }
    };

    private final TabObserver mTabObserver = new EmptyTabObserver() {
        @Override
        public void onHidden(Tab tab) {
            mKeyboardAccessory.dismiss();
        }

        @Override
        public void onDestroyed(Tab tab) {
            mModel.remove(tab); // Clears tab if still present.
            restoreCachedState(mActiveBrowserTab);
        }

        @Override
        public void onEnterFullscreenMode(Tab tab, FullscreenOptions options) {
            mKeyboardAccessory.dismiss();
        }
    };

    void initialize(KeyboardAccessoryCoordinator keyboardAccessory,
            AccessorySheetCoordinator accessorySheet, WindowAndroid windowAndroid) {
        assert windowAndroid.getActivity().get() != null;
        mWindowAndroid = windowAndroid;
        mKeyboardAccessory = keyboardAccessory;
        mAccessorySheet = accessorySheet;
        mActivity = (ChromeActivity) windowAndroid.getActivity().get();
        if (mActivity instanceof ChromeTabbedActivity) {
            // This object typically lives as long as the layout manager, so there is no need to
            // unsubscribe which would occasionally use an invalidated object.
            ((ChromeTabbedActivity) mActivity)
                    .getLayoutManager()
                    .addSceneChangeObserver(mTabSwitcherObserver);
        }
        windowAndroid.addKeyboardVisibilityListener(mVisibilityListener);
        mTabModelObserver = new TabModelSelectorTabModelObserver(mActivity.getTabModelSelector()) {
            @Override
            public void didSelectTab(Tab tab, @TabModel.TabSelectionType int type, int lastId) {
                mActiveBrowserTab = tab;
                mPreviousControlHeight = mActivity.getFullscreenManager().getBottomControlsHeight();
                restoreCachedState(tab);
            }

            @Override
            public void willCloseTab(Tab tab, boolean animate) {
                mModel.remove(tab);
                restoreCachedState(mActiveBrowserTab);
            }
        };
        Tab currentTab = mActivity.getTabModelSelector().getCurrentTab();
        if (currentTab != null) {
            mTabModelObserver.didSelectTab(
                    currentTab, TabModel.TabSelectionType.FROM_USER, Tab.INVALID_TAB_ID);
        }
    }

    // TODO(fhorschig): Look for stronger signals than |keyboardVisibilityChanged|.
    // This variable remembers the last state of |keyboardVisibilityChanged| which might not be
    // sufficient for edge cases like hardware keyboards, floating keyboards, etc.
    private void onKeyboardVisibilityChanged(boolean isShowing) {
        if (isShowing) {
            mKeyboardAccessory.requestShowing();
            mActivity.getFullscreenManager().setBottomControlsHeight(calculateAccessoryBarHeight());
            mKeyboardAccessory.closeActiveTab();
            mKeyboardAccessory.setBottomOffset(0);
            mAccessorySheet.hide();
        } else {
            mKeyboardAccessory.close();
            if (mKeyboardAccessory.hasActiveTab()) {
                mKeyboardAccessory.setBottomOffset(mAccessorySheet.getHeight());
                mAccessorySheet.show();
                onBottomControlSpaceChanged();
            } else {
                mActivity.getFullscreenManager().setBottomControlsHeight(mPreviousControlHeight);
            }
        }
    }

    void registerPasswordProvider(Provider<KeyboardAccessoryData.Item> itemProvider) {
        assert getPasswordAccessorySheet() != null : "No password sheet available!";
        getPasswordAccessorySheet().registerItemProvider(itemProvider);
    }

    void registerActionProvider(KeyboardAccessoryData.PropertyProvider<Action> actionProvider) {
        ActionProviderCacheAdapter adapter =
                new ActionProviderCacheAdapter(mActiveBrowserTab, actionProvider, new Action[0]);
        mModel.get(mActiveBrowserTab).mActionsProvider = adapter;
        getKeyboardAccessory().registerActionListProvider(adapter);
    }

    void destroy() {
        if (mWindowAndroid != null) {
            mWindowAndroid.removeKeyboardVisibilityListener(mVisibilityListener);
        }
        mTabModelObserver.destroy();
    }

    boolean handleBackPress() {
        if (mAccessorySheet.isShown()) {
            mKeyboardAccessory.dismiss();
            return true;
        }
        return false;
    }

    void dismiss() {
        mKeyboardAccessory.dismiss();
        UiUtils.hideKeyboard(mActivity.getCurrentFocus());
    }

    void notifyPopupOpened(DropdownPopupWindow popup) {
        mPopup = popup;
    }

    public void pause() {
        mKeyboardAccessory.dismiss();
    }

    void resume() {
        restoreCachedState(mActiveBrowserTab);
    }

    @Override
    public void onChangeAccessorySheet(int tabIndex) {
        assert mActivity != null : "ManualFillingMediator needs initialization.";
        mAccessorySheet.setActiveTab(tabIndex);
        if (mPopup != null && mPopup.isShowing()) mPopup.dismiss();
        // If there is a keyboard, update the accessory sheet's height and hide the keyboard.
        View focusedView = mActivity.getCurrentFocus();
        if (focusedView != null) {
            mAccessorySheet.setHeight(calculateAccessorySheetHeight(focusedView.getRootView()));
            UiUtils.hideKeyboard(focusedView);
        }
    }

    @Override
    public void onCloseAccessorySheet() {
        View focusedView = mActivity.getCurrentFocus();
        if (focusedView == null || UiUtils.isKeyboardShowing(mActivity, focusedView)) {
            return; // If the keyboard is showing or is starting to show, the sheet closes gently.
        }
        mActivity.getFullscreenManager().setBottomControlsHeight(mPreviousControlHeight);
        mKeyboardAccessory.closeActiveTab();
        mKeyboardAccessory.setBottomOffset(0);
        mAccessorySheet.hide();
    }

    @Override
    public void onOpenKeyboard() {
        assert mActivity != null : "ManualFillingMediator needs initialization.";
        mActivity.getFullscreenManager().setBottomControlsHeight(calculateAccessoryBarHeight());
        UiUtils.showKeyboard(mActivity.getCurrentFocus());
    }

    @Override
    public void onBottomControlSpaceChanged() {
        @Px
        int newControlsHeight = calculateAccessoryBarHeight();
        if (mAccessorySheet.isShown()) {
            newControlsHeight += mAccessorySheet.getHeight();
        }
        mActivity.getFullscreenManager().setBottomControlsHeight(newControlsHeight);
    }

    private AccessoryState getOrCreateAccessoryState(Tab tab) {
        AccessoryState state = mModel.get(tab);
        if (state != null) return state;
        state = new AccessoryState();
        mModel.put(tab, state);
        tab.addObserver(mTabObserver);
        return state;
    }

    private void restoreCachedState(Tab browserTab) {
        mKeyboardAccessory.dismiss();
        clearTabs();
        AccessoryState state = getOrCreateAccessoryState(browserTab);
        if (state.mPasswordAccessorySheet != null) {
            addTab(state.mPasswordAccessorySheet.getTab());
        }
        if (state.mActionsProvider != null) state.mActionsProvider.notifyAboutCachedItems();
    }

    private void clearTabs() {
        mKeyboardAccessory.setTabs(new KeyboardAccessoryData.Tab[0]);
        mAccessorySheet.setTabs(new KeyboardAccessoryData.Tab[0]);
    }

    private @Px int calculateAccessorySheetHeight(View rootView) {
        int accessorySheetSuggestionHeight = mActivity.getResources().getDimensionPixelSize(
                org.chromium.chrome.R.dimen.keyboard_accessory_suggestion_height);
        // Ensure that the minimum height is always sufficient to display a suggestion.
        return Math.max(accessorySheetSuggestionHeight,
                UiUtils.calculateKeyboardHeight(mActivity, rootView));
    }

    private @Px int calculateAccessoryBarHeight() {
        if (!mKeyboardAccessory.isShown()) return 0;
        return mActivity.getResources().getDimensionPixelSize(
                org.chromium.chrome.R.dimen.keyboard_accessory_suggestion_height);
    }

    @VisibleForTesting
    void addTab(KeyboardAccessoryData.Tab tab) {
        // TODO(fhorschig): This should add the tab only to the state. Sheet and accessory should be
        // using a |set| method or even observe the state.
        mKeyboardAccessory.addTab(tab);
        mAccessorySheet.addTab(tab);
    }

    @VisibleForTesting
    @Nullable
    PasswordAccessorySheetCoordinator getPasswordAccessorySheet() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.EXPERIMENTAL_UI)
                && !ChromeFeatureList.isEnabled(ChromeFeatureList.PASSWORDS_KEYBOARD_ACCESSORY)) {
            return null;
        }
        AccessoryState state = getOrCreateAccessoryState(mActiveBrowserTab);
        if (state.mPasswordAccessorySheet == null) {
            state.mPasswordAccessorySheet = new PasswordAccessorySheetCoordinator(mActivity);
            addTab(state.mPasswordAccessorySheet.getTab());
        }
        return state.mPasswordAccessorySheet;
    }

    // TODO(fhorschig): Should be @VisibleForTesting.
    KeyboardAccessoryCoordinator getKeyboardAccessory() {
        return mKeyboardAccessory;
    }

    @VisibleForTesting
    AccessorySheetCoordinator getAccessorySheet() {
        return mAccessorySheet;
    }

    @VisibleForTesting
    TabModelObserver getTabModelObserverForTesting() {
        return mTabModelObserver;
    }

    @VisibleForTesting
    TabObserver getTabObserverForTesting() {
        return mTabObserver;
    }

    @VisibleForTesting
    Map<Tab, AccessoryState> getModelForTesting() {
        return mModel;
    }
}
