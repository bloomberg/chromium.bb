// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingProperties.KEYBOARD_EXTENSION_STATE;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingProperties.KeyboardExtensionState.EXTENDING_KEYBOARD;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingProperties.KeyboardExtensionState.HIDDEN;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingProperties.KeyboardExtensionState.REPLACING_KEYBOARD;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingProperties.SHOW_WHEN_VISIBLE;

import android.support.annotation.Nullable;
import android.support.annotation.Px;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.Supplier;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeKeyboardVisibilityDelegate;
import org.chromium.chrome.browser.ChromeWindow;
import org.chromium.chrome.browser.InsetObserverView;
import org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingProperties.KeyboardExtensionState;
import org.chromium.chrome.browser.autofill.keyboard_accessory.bar_component.KeyboardAccessoryCoordinator;
import org.chromium.chrome.browser.autofill.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.autofill.keyboard_accessory.data.KeyboardAccessoryData.Action;
import org.chromium.chrome.browser.autofill.keyboard_accessory.data.PropertyProvider;
import org.chromium.chrome.browser.autofill.keyboard_accessory.sheet_component.AccessorySheetCoordinator;
import org.chromium.chrome.browser.autofill.keyboard_accessory.sheet_tabs.CreditCardAccessorySheetCoordinator;
import org.chromium.chrome.browser.autofill.keyboard_accessory.sheet_tabs.PasswordAccessorySheetCoordinator;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.SceneChangeObserver;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.components.autofill.AutofillDelegate;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.DropdownPopupWindow;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable;

import java.util.HashSet;

/**
 * This part of the manual filling component manages the state of the manual filling flow depending
 * on the currently shown tab.
 */
class ManualFillingMediator extends EmptyTabObserver
        implements KeyboardAccessoryCoordinator.VisibilityDelegate, View.OnLayoutChangeListener {
    static private final int MINIMAL_AVAILABLE_VERTICAL_SPACE = 80; // in DP.
    static private final int MINIMAL_AVAILABLE_HORIZONTAL_SPACE = 180; // in DP.

    private PropertyModel mModel = ManualFillingProperties.createFillingModel();
    private WindowAndroid mWindowAndroid;
    private Supplier<InsetObserverView> mInsetObserverViewSupplier;
    private final KeyboardExtensionSizeManager mKeyboardExtensionSizeManager =
            new KeyboardExtensionSizeManager();
    private final ManualFillingStateCache mStateCache = new ManualFillingStateCache();
    private final HashSet<Tab> mObservedTabs = new HashSet<>();
    private KeyboardAccessoryCoordinator mKeyboardAccessory;
    private AccessorySheetCoordinator mAccessorySheet;
    private ChromeActivity mActivity; // Used to control the keyboard.
    private TabModelSelectorTabModelObserver mTabModelObserver;
    private DropdownPopupWindow mPopup;

    private final SceneChangeObserver mTabSwitcherObserver = new SceneChangeObserver() {
        @Override
        public void onTabSelectionHinted(int tabId) {}

        @Override
        public void onSceneStartShowing(Layout layout) {
            // Includes events like side-swiping between tabs and triggering contextual search.
            pause();
        }

        @Override
        public void onSceneChange(Layout layout) {}
    };

    private final TabObserver mTabObserver = new EmptyTabObserver() {
        @Override
        public void onHidden(Tab tab, @TabHidingType int type) {
            pause();
        }

        @Override
        public void onDestroyed(Tab tab) {
            mStateCache.destroyStateFor(tab);
            pause();
            refreshTabs();
        }

        @Override
        public void onEnterFullscreenMode(Tab tab, FullscreenOptions options) {
            pause();
        }
    };

    void initialize(KeyboardAccessoryCoordinator keyboardAccessory,
            AccessorySheetCoordinator accessorySheet, WindowAndroid windowAndroid) {
        mActivity = (ChromeActivity) windowAndroid.getActivity().get();
        assert mActivity != null;
        mWindowAndroid = windowAndroid;
        mKeyboardAccessory = keyboardAccessory;
        mModel.addObserver(this::onPropertyChanged);
        mAccessorySheet = accessorySheet;
        mAccessorySheet.setOnPageChangeListener(mKeyboardAccessory.getOnPageChangeListener());
        setInsetObserverViewSupplier(mActivity::getInsetObserverView);
        LayoutManager manager = getLayoutManager();
        if (manager != null) manager.addSceneChangeObserver(mTabSwitcherObserver);
        mActivity.findViewById(android.R.id.content).addOnLayoutChangeListener(this);
        mTabModelObserver = new TabModelSelectorTabModelObserver(mActivity.getTabModelSelector()) {
            @Override
            public void didSelectTab(Tab tab, int type, int lastId) {
                ensureObserverRegistered(tab);
                refreshTabs();
            }

            @Override
            public void tabClosureCommitted(Tab tab) {
                super.tabClosureCommitted(tab);
                mStateCache.destroyStateFor(tab);
            }
        };
        ensureObserverRegistered(getActiveBrowserTab());
        refreshTabs();
    }

    boolean isInitialized() {
        return mWindowAndroid != null;
    }

    boolean isFillingViewShown(View view) {
        return isInitialized() && !isSoftKeyboardShowing(view) && mKeyboardAccessory.hasActiveTab();
    }

    @Override
    public void onLayoutChange(View view, int left, int top, int right, int bottom, int oldLeft,
            int oldTop, int oldRight, int oldBottom) {
        if (!isInitialized()) return; // Activity uninitialized or cleaned up already.
        if (!mKeyboardAccessory.hasContents()) return; // Exit early to not affect the layout.
        if (isSoftKeyboardShowing(view)) {
            mModel.set(KEYBOARD_EXTENSION_STATE, EXTENDING_KEYBOARD);
            return;
        }
        mKeyboardAccessory.requestClosing(); // Closing takes effect when keyboard and sheets hide.
        onBottomControlSpaceChanged();
        mModel.set(KEYBOARD_EXTENSION_STATE,
                mKeyboardAccessory.hasActiveTab() ? REPLACING_KEYBOARD : HIDDEN);
    }

    void registerPasswordProvider(
            PropertyProvider<KeyboardAccessoryData.AccessorySheetData> dataProvider) {
        ManualFillingState state = mStateCache.getStateFor(mActivity.getCurrentWebContents());

        state.wrapPasswordSheetDataProvider(dataProvider);
        PasswordAccessorySheetCoordinator accessorySheet = getOrCreatePasswordSheet();
        if (accessorySheet == null) return; // Not available or initialized yet.
        accessorySheet.registerDataProvider(state.getPasswordSheetDataProvider());
    }

    void registerCreditCardProvider() {
        CreditCardAccessorySheetCoordinator accessorySheet = getOrCreateCreditCardSheet();
        if (accessorySheet == null) return;
    }

    void registerAutofillProvider(
            PropertyProvider<AutofillSuggestion[]> autofillProvider, AutofillDelegate delegate) {
        if (mKeyboardAccessory == null) return;
        mKeyboardAccessory.registerAutofillProvider(autofillProvider, delegate);
    }

    void registerActionProvider(PropertyProvider<Action[]> actionProvider) {
        if (!isInitialized()) return;
        ManualFillingState state = mStateCache.getStateFor(mActivity.getCurrentWebContents());

        state.wrapActionsProvider(actionProvider, new Action[0]);
        mKeyboardAccessory.registerActionProvider(state.getActionsProvider());
    }

    void destroy() {
        if (!isInitialized()) return;
        pause();
        mActivity.findViewById(android.R.id.content).removeOnLayoutChangeListener(this);
        mTabModelObserver.destroy();
        mStateCache.destroy();
        for (Tab tab : mObservedTabs) tab.removeObserver(mTabObserver);
        mObservedTabs.clear();
        LayoutManager manager = getLayoutManager();
        if (manager != null) manager.removeSceneChangeObserver(mTabSwitcherObserver);
        mWindowAndroid = null;
        mActivity = null;
    }

    boolean handleBackPress() {
        if (isInitialized() && mAccessorySheet.isShown()) {
            pause();
            return true;
        }
        return false;
    }

    void dismiss() {
        if (!isInitialized()) return;
        pause();
        ViewGroup contentView = getContentView();
        if (contentView != null) getKeyboard().hideSoftKeyboardOnly(contentView);
    }

    void notifyPopupOpened(DropdownPopupWindow popup) {
        mPopup = popup;
    }

    void showWhenKeyboardIsVisible() {
        if (!isInitialized() || !mKeyboardAccessory.hasContents()) return;
        mModel.set(SHOW_WHEN_VISIBLE, true);
    }

    void hide() {
        mModel.set(SHOW_WHEN_VISIBLE, false);
    }

    void pause() {
        if (!isInitialized()) return;
        mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
    }

    void resume() {
        if (!isInitialized()) return;
        pause(); // Resuming dismisses the keyboard. Ensure the accessory doesn't linger.
        refreshTabs();
    }

    private boolean hasSufficientSpace() {
        if (mActivity == null) return false;
        WebContents webContents = mActivity.getCurrentWebContents();
        if (webContents == null) return false;
        float height = webContents.getHeight(); // getHeight actually returns dip, not Px!
        height += calculateAccessoryBarHeight() / mWindowAndroid.getDisplay().getDipScale();
        return height >= MINIMAL_AVAILABLE_VERTICAL_SPACE
                && webContents.getWidth() >= MINIMAL_AVAILABLE_HORIZONTAL_SPACE;
    }

    KeyboardExtensionSizeManager getKeyboardExtensionSizeManager() {
        return mKeyboardExtensionSizeManager;
    }

    private void onPropertyChanged(PropertyObservable<PropertyKey> source, PropertyKey property) {
        assert source == mModel;
        if (property == SHOW_WHEN_VISIBLE) {
            if (!mModel.get(SHOW_WHEN_VISIBLE)) {
                pause();
            } else if (isSoftKeyboardShowing(getContentView())) {
                mModel.set(KEYBOARD_EXTENSION_STATE, EXTENDING_KEYBOARD);
            }
            return;
        }
        if (property == KEYBOARD_EXTENSION_STATE) {
            transitionIntoState(mModel.get(KEYBOARD_EXTENSION_STATE));
            return;
        }
        throw new IllegalArgumentException("Unhandled property: " + property);
    }

    /**
     * Ensures the state reflects certain properties. It checks preconditions for states and
     * redirects to a different state if they are not met.
     * @param extensionState The {@link KeyboardExtensionState} to transition into.
     */
    private void transitionIntoState(@KeyboardExtensionState int extensionState) {
        switch (extensionState) {
            case HIDDEN:
                hideAccessoryComponents();
                return;
            case EXTENDING_KEYBOARD:
                if (!canExtendKeyboard()) {
                    mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
                    return;
                }
                extendKeyboardWithAccessoryComponents();
                return;
            case REPLACING_KEYBOARD:
                if (!hasSufficientSpace()) {
                    mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
                    return;
                }
                mAccessorySheet.show();
                return;
        }
        throw new IllegalArgumentException(
                "Unhandled transition into state: " + mModel.get(KEYBOARD_EXTENSION_STATE));
    }

    /**
     * Ensures all subcomponents are hidden and no space is consumed anymore.
     */
    private void hideAccessoryComponents() {
        mKeyboardExtensionSizeManager.setKeyboardExtensionHeight(0);
        mKeyboardAccessory.closeActiveTab();
        mAccessorySheet.hide();
        mKeyboardAccessory.dismiss();
    }

    /**
     * Ensure the accessory bar is the only subcomponent showing. Possibly open sheets are closed.
     */
    private void extendKeyboardWithAccessoryComponents() {
        mKeyboardAccessory.requestShowing();
        mKeyboardExtensionSizeManager.setKeyboardExtensionHeight(calculateAccessoryBarHeight());
        if (mAccessorySheet.isShown()) mKeyboardAccessory.closeActiveTab();
        mKeyboardAccessory.setBottomOffset(0);
        mAccessorySheet.hide();
    }

    /**
     * Returns whether the accessory bar can be shown.
     * @return True if the keyboard can (and should) be shown. False otherwise.
     */
    private boolean canExtendKeyboard() {
        if (!mModel.get(SHOW_WHEN_VISIBLE)) return false;

        // Don't open the accessory inside the contextual search panel.
        ContextualSearchManager contextualSearch = mActivity.getContextualSearchManager();
        if (contextualSearch != null && contextualSearch.isSearchPanelOpened()) return false;

        // If an accessory sheet was opened, the accessory bar must be visible.
        if (mAccessorySheet.isShown()) return true;

        return hasSufficientSpace(); // Only extend the keyboard, if there is enough space.
    }

    @Override
    public void onChangeAccessorySheet(int tabIndex) {
        if (mActivity == null) return; // Mediator not initialized or already destroyed.
        mAccessorySheet.setActiveTab(tabIndex);
        if (mPopup != null && mPopup.isShowing()) mPopup.dismiss();
        // If there is a keyboard, update the accessory sheet's height and hide the keyboard.
        ViewGroup contentView = getContentView();
        if (contentView == null) return; // Apparently the tab was cleaned up already.
        View rootView = contentView.getRootView();
        if (rootView == null) return;
        mAccessorySheet.setHeight(calculateAccessorySheetHeight(rootView));
        getKeyboard().hideSoftKeyboardOnly(contentView);
    }

    @Override
    public void onCloseAccessorySheet() {
        ViewGroup contentView = getContentView();
        if (contentView == null) return; // The tab was cleaned up already.
        if (isSoftKeyboardShowing(contentView)) {
            return; // If the keyboard is showing or is starting to show, the sheet closes gently.
        }
        mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
        mKeyboardAccessory.setBottomOffset(0);
        mActivity.getCompositorViewHolder().requestLayout(); // Request checks for keyboard changes.
    }

    /**
     * Opens the keyboard which implicitly dismisses the sheet. Without open sheet, this is a NoOp.
     */
    void swapSheetWithKeyboard() {
        if (isInitialized() && mAccessorySheet.isShown()) onOpenKeyboard();
    }

    @Override
    public void onOpenKeyboard() {
        assert mActivity != null : "ManualFillingMediator needs initialization.";
        mKeyboardExtensionSizeManager.setKeyboardExtensionHeight(calculateAccessoryBarHeight());
        if (mActivity.getCurrentFocus() != null) {
            getKeyboard().showKeyboard(mActivity.getCurrentFocus());
        }
    }

    @Override
    public void onBottomControlSpaceChanged() {
        int newControlsHeight = calculateAccessoryBarHeight();
        int newControlsOffset = 0;
        if (mKeyboardAccessory.hasActiveTab()) {
            newControlsHeight += mAccessorySheet.getHeight();
            newControlsOffset += mAccessorySheet.getHeight();
        }
        mKeyboardAccessory.setBottomOffset(newControlsOffset);
        mKeyboardExtensionSizeManager.setKeyboardExtensionHeight(
                mKeyboardAccessory.isShown() ? newControlsHeight : 0);
        mActivity.getFullscreenManager().updateViewportSize();
    }

    /**
     * When trying to get the content of the active tab, there are several cases where a component
     * can be null - usually use before initialization or after destruction.
     * This helper ensures that the IDE warns about unchecked use of the all Nullable methods and
     * provides a shorthand for checking that all components are ready to use.
     * @return The content {@link View} of the held {@link ChromeActivity} or null if any part of it
     *         isn't ready to use.
     */
    private @Nullable ViewGroup getContentView() {
        if (mActivity == null) return null;
        Tab tab = getActiveBrowserTab();
        if (tab == null) return null;
        return tab.getContentView();
    }

    /**
     * Shorthand to check whether there is a valid {@link LayoutManager} for the current activity.
     * If there isn't (e.g. before initialization or after destruction), return null.
     * @return {@code null} or a {@link LayoutManager}.
     */
    private @Nullable LayoutManager getLayoutManager() {
        if (mActivity == null) return null;
        CompositorViewHolder compositorViewHolder = mActivity.getCompositorViewHolder();
        if (compositorViewHolder == null) return null;
        return compositorViewHolder.getLayoutManager();
    }

    /**
     * Shorthand to get the activity tab.
     * @return The currently visible {@link Tab}, if any.
     */
    private @Nullable Tab getActiveBrowserTab() {
        return mActivity.getActivityTabProvider().getActivityTab();
    }

    /**
     * Registers a {@link TabObserver} to the given {@link Tab} if it hasn't been done yet.
     * Using this function avoid deleting and readding the observer (each O(N)) since the tab does
     * not report whether an observer is registered.
     * @param tab A {@link Tab}. May be the currently active tab which is allowed to be null.
     */
    private void ensureObserverRegistered(@Nullable Tab tab) {
        if (tab == null) return; // No tab given, no observer necessary.
        if (!mObservedTabs.add(tab)) return; // Observer already registered.
        tab.addObserver(mTabObserver);
    }

    private ChromeKeyboardVisibilityDelegate getKeyboard() {
        assert mWindowAndroid instanceof ChromeWindow;
        assert mWindowAndroid.getKeyboardDelegate() instanceof ChromeKeyboardVisibilityDelegate;
        return (ChromeKeyboardVisibilityDelegate) mWindowAndroid.getKeyboardDelegate();
    }

    private boolean isSoftKeyboardShowing(@Nullable View view) {
        return view != null && getKeyboard().isSoftKeyboardShowing(mActivity, view);
    }

    private @Px int calculateAccessorySheetHeight(View rootView) {
        InsetObserverView insetObserver = mInsetObserverViewSupplier.get();
        if (insetObserver != null) return insetObserver.getSystemWindowInsetsBottom();
        // Without known inset (which is keyboard + bottom soft keys), use the keyboard height.
        return Math.max(mActivity.getResources().getDimensionPixelSize(
                                org.chromium.chrome.R.dimen.keyboard_accessory_suggestion_height),
                getKeyboard().calculateKeyboardHeight(rootView));
    }

    private @Px int calculateAccessoryBarHeight() {
        if (!mKeyboardAccessory.isShown()) return 0;
        return mActivity.getResources().getDimensionPixelSize(
                org.chromium.chrome.R.dimen.keyboard_accessory_suggestion_height);
    }

    private void refreshTabs() {
        if (!isInitialized()) return;
        KeyboardAccessoryData.Tab[] tabs =
                mStateCache.getStateFor(mActivity.getCurrentWebContents()).getTabs();
        mKeyboardAccessory.setTabs(tabs);
        mAccessorySheet.setTabs(tabs);
    }

    /**
     * Returns the password sheet for the current WebContents or creates one if it doesn't exist.
     * @return A {@link PasswordAccessorySheetCoordinator} or null if unavailable.
     */
    @VisibleForTesting
    @Nullable
    PasswordAccessorySheetCoordinator getOrCreatePasswordSheet() {
        if (!isInitialized()) return null;
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.EXPERIMENTAL_UI)
                && !ChromeFeatureList.isEnabled(ChromeFeatureList.PASSWORDS_KEYBOARD_ACCESSORY)) {
            return null;
        }
        WebContents webContents = mActivity.getCurrentWebContents();
        if (webContents == null) return null; // There is no active tab or it's being destroyed.
        ManualFillingState state = mStateCache.getStateFor(webContents);
        if (state.getPasswordAccessorySheet() != null) return state.getPasswordAccessorySheet();

        PasswordAccessorySheetCoordinator passwordSheet = new PasswordAccessorySheetCoordinator(
                mActivity, mAccessorySheet.getScrollListener());
        state.setPasswordAccessorySheet(passwordSheet);
        if (state.getPasswordSheetDataProvider() != null) {
            passwordSheet.registerDataProvider(state.getPasswordSheetDataProvider());
        }
        refreshTabs();
        return passwordSheet;
    }

    /**
     * Returns the credit card sheet for the current WebContents or creates one if it doesn't exist.
     * @return A {@link CreditCardAccessorySheetCoordinator} or null if unavailable.
     */
    @VisibleForTesting
    @Nullable
    CreditCardAccessorySheetCoordinator getOrCreateCreditCardSheet() {
        if (!isInitialized()) return null;
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_MANUAL_FALLBACK_ANDROID)) {
            return null;
        }
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.EXPERIMENTAL_UI)
                && !ChromeFeatureList.isEnabled(ChromeFeatureList.PASSWORDS_KEYBOARD_ACCESSORY)) {
            return null;
        }
        WebContents webContents = mActivity.getCurrentWebContents();
        if (webContents == null) return null; // There is no active tab or it's being destroyed.
        ManualFillingState state = mStateCache.getStateFor(webContents);
        if (state.getCreditCardAccessorySheet() != null) return state.getCreditCardAccessorySheet();
        state.setCreditCardAccessorySheet(new CreditCardAccessorySheetCoordinator(
                mActivity, mAccessorySheet.getScrollListener()));
        refreshTabs();
        return state.getCreditCardAccessorySheet();
    }

    @VisibleForTesting
    void setInsetObserverViewSupplier(Supplier<InsetObserverView> insetObserverViewSupplier) {
        mInsetObserverViewSupplier = insetObserverViewSupplier;
    }

    @VisibleForTesting
    @Nullable
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
    ManualFillingStateCache getStateCacheForTesting() {
        return mStateCache;
    }
}
