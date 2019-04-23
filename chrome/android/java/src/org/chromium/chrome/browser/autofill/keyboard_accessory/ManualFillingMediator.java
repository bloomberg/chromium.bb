// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingProperties.KEYBOARD_EXTENSION_STATE;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingProperties.KeyboardExtensionState.EXTENDING_KEYBOARD;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingProperties.KeyboardExtensionState.FLOATING_BAR;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingProperties.KeyboardExtensionState.FLOATING_SHEET;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingProperties.KeyboardExtensionState.HIDDEN;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingProperties.KeyboardExtensionState.REPLACING_KEYBOARD;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingProperties.KeyboardExtensionState.WAITING_TO_REPLACE;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingProperties.PORTRAIT_ORIENTATION;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingProperties.SHOW_WHEN_VISIBLE;

import android.support.annotation.Nullable;
import android.support.annotation.Px;
import android.view.Surface;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.Supplier;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeKeyboardVisibilityDelegate;
import org.chromium.chrome.browser.ChromeWindow;
import org.chromium.chrome.browser.InsetObserverView;
import org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingProperties.KeyboardExtensionState;
import org.chromium.chrome.browser.autofill.keyboard_accessory.ManualFillingProperties.StateProperty;
import org.chromium.chrome.browser.autofill.keyboard_accessory.bar_component.KeyboardAccessoryCoordinator;
import org.chromium.chrome.browser.autofill.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.autofill.keyboard_accessory.data.KeyboardAccessoryData.Action;
import org.chromium.chrome.browser.autofill.keyboard_accessory.data.PropertyProvider;
import org.chromium.chrome.browser.autofill.keyboard_accessory.sheet_component.AccessorySheetCoordinator;
import org.chromium.chrome.browser.autofill.keyboard_accessory.sheet_tabs.CreditCardAccessorySheetCoordinator;
import org.chromium.chrome.browser.autofill.keyboard_accessory.sheet_tabs.PasswordAccessorySheetCoordinator;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.CompositorViewResizer;
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
    private final KeyboardExtensionViewResizer mKeyboardExtensionViewResizer =
            new KeyboardExtensionViewResizer();
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
        mModel.set(PORTRAIT_ORIENTATION, hasPortraitOrientation());
        mModel.addObserver(this::onPropertyChanged);
        mAccessorySheet = accessorySheet;
        mAccessorySheet.setOnPageChangeListener(mKeyboardAccessory.getOnPageChangeListener());
        mAccessorySheet.setHeight(3
                * mActivity.getResources().getDimensionPixelSize(
                        R.dimen.keyboard_accessory_suggestion_height));
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
                mObservedTabs.remove(tab);
                tab.removeObserver(mTabObserver); // Fails silently if observer isn't registered.
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
        if (mKeyboardAccessory.empty()) return; // Exit early to not affect the layout.
        if (!hasSufficientSpace()) {
            mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
            return;
        }
        if (hasPortraitOrientation() != mModel.get(PORTRAIT_ORIENTATION)) {
            mModel.set(PORTRAIT_ORIENTATION, hasPortraitOrientation());
            return;
        }
        restrictAccessorySheetHeight();
        if (!isSoftKeyboardShowing(view)) {
            if (is(WAITING_TO_REPLACE)) mModel.set(KEYBOARD_EXTENSION_STATE, REPLACING_KEYBOARD);
            if (is(EXTENDING_KEYBOARD)) mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
            // Layout changes when entering/resizing/leaving MultiWindow. Ensure a consistent state:
            updateKeyboard(mModel.get(KEYBOARD_EXTENSION_STATE));
            return;
        }
        if (is(WAITING_TO_REPLACE)) return;
        mModel.set(KEYBOARD_EXTENSION_STATE,
                mModel.get(SHOW_WHEN_VISIBLE) ? EXTENDING_KEYBOARD : HIDDEN);
    }

    private boolean hasPortraitOrientation() {
        return mWindowAndroid.getDisplay().getRotation() == Surface.ROTATION_0
                || mWindowAndroid.getDisplay().getRotation() == Surface.ROTATION_180;
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
        if (isInitialized()
                && (is(WAITING_TO_REPLACE) || is(REPLACING_KEYBOARD) || is(FLOATING_SHEET))) {
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
        if (!isInitialized() || mKeyboardAccessory.empty()) return;
        mModel.set(SHOW_WHEN_VISIBLE, true);
        if (is(HIDDEN)) mModel.set(KEYBOARD_EXTENSION_STATE, FLOATING_BAR);
    }

    void hide() {
        mModel.set(SHOW_WHEN_VISIBLE, false);
        pause();
    }

    void pause() {
        if (!isInitialized()) return;
        mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
    }

    private void onOrientationChange() {
        if (!isInitialized()) return;
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY)
                || is(REPLACING_KEYBOARD) || is(FLOATING_SHEET)) {
            mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
            // Autofill suggestions are invalidated on rotation. Dismissing all filling UI forces
            // the user to interact with the field they want to edit. This refreshes Autofill.
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY)) {
                hideSoftKeyboard();
            }
        }
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
        height += mKeyboardExtensionViewResizer.getHeight()
                / mWindowAndroid.getDisplay().getDipScale();
        return height >= MINIMAL_AVAILABLE_VERTICAL_SPACE
                && webContents.getWidth() >= MINIMAL_AVAILABLE_HORIZONTAL_SPACE;
    }

    CompositorViewResizer getKeyboardExtensionViewResizer() {
        return mKeyboardExtensionViewResizer;
    }

    private void onPropertyChanged(PropertyObservable<PropertyKey> source, PropertyKey property) {
        assert source == mModel;
        if (property == SHOW_WHEN_VISIBLE) {
            return;
        } else if (property == PORTRAIT_ORIENTATION) {
            onOrientationChange();
            return;
        } else if (property == KEYBOARD_EXTENSION_STATE) {
            transitionIntoState(mModel.get(KEYBOARD_EXTENSION_STATE));
            return;
        }
        throw new IllegalArgumentException("Unhandled property: " + property);
    }

    /**
     * If preconditions for a state are met, enforce the state's properties and trigger its effects.
     * @param extensionState The {@link KeyboardExtensionState} to transition into.
     */
    private void transitionIntoState(@KeyboardExtensionState int extensionState) {
        if (!meetsStatePreconditions(extensionState)) return;
        enforceStateProperties(extensionState);
        changeBottomControlSpaceForState(extensionState);
        updateKeyboard(extensionState);
    }

    /**
     * Checks preconditions for states and redirects to a different state if they are not met.
     * @param extensionState The {@link KeyboardExtensionState} to transition into.
     */
    private boolean meetsStatePreconditions(@KeyboardExtensionState int extensionState) {
        switch (extensionState) {
            case HIDDEN:
                return true;
            case FLOATING_BAR:
                if (isSoftKeyboardShowing(getContentView())) {
                    mModel.set(KEYBOARD_EXTENSION_STATE, EXTENDING_KEYBOARD);
                    return false;
                }
                // Intentional fallthrough.
            case EXTENDING_KEYBOARD:
                if (!canExtendKeyboard()) {
                    mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
                    return false;
                }
                return true;
            case FLOATING_SHEET:
                if (isSoftKeyboardShowing(getContentView())) {
                    mModel.set(KEYBOARD_EXTENSION_STATE, EXTENDING_KEYBOARD);
                    return false;
                }
                // Intentional fallthrough.
            case REPLACING_KEYBOARD:
                if (isSoftKeyboardShowing(getContentView())) {
                    mModel.set(KEYBOARD_EXTENSION_STATE, WAITING_TO_REPLACE);
                    return false; // Wait for the keyboard to disappear before replacing!
                }
                // Intentional fallthrough.
            case WAITING_TO_REPLACE:
                if (!hasSufficientSpace()) {
                    mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
                    return false;
                }
                return true;
        }
        throw new IllegalArgumentException(
                "Unhandled transition into state: " + mModel.get(KEYBOARD_EXTENSION_STATE));
    }

    private void enforceStateProperties(@KeyboardExtensionState int extensionState) {
        if (requiresVisibleBar(extensionState)) {
            mKeyboardAccessory.show();
        } else {
            mKeyboardAccessory.dismiss();
        }
        if (requiresVisibleSheet(extensionState)) {
            mAccessorySheet.show();
        } else if (requiresHiddenSheet(extensionState)) {
            mKeyboardAccessory.closeActiveTab();
            mAccessorySheet.hide();
            // The compositor should relayout the view when the sheet is hidden. This is necessary
            // to trigger events that rely on the relayout (like toggling the overview button):
            mActivity.getCompositorViewHolder().requestLayout();
        }
    }

    private void updateKeyboard(@KeyboardExtensionState int extensionState) {
        if (isFloating(extensionState)) {
            // Keyboard-bound states are always preferable over floating states. Therefore, trigger
            // a keyboard here. This also allows for smooth transitions, e.g. when closing a sheet:
            // the REPLACING state transitions into FLOATING_SHEET which triggers the keyboard which
            // transitions into the EXTENDING state as soon as the keyboard appeared.
            ViewGroup contentView = getContentView();
            if (contentView != null) getKeyboard().showKeyboard(contentView);
        } else if (extensionState == WAITING_TO_REPLACE) {
            // In order to give the keyboard time to disappear, hide the keyboard and enter the
            // REPLACING state.
            hideSoftKeyboard();
        }
    }

    private void hideSoftKeyboard() {
        // If there is a keyboard, update the accessory sheet's height and hide the keyboard.
        ViewGroup contentView = getContentView();
        if (contentView == null) return; // Apparently the tab was cleaned up already.
        View rootView = contentView.getRootView();
        if (rootView == null) return;
        mAccessorySheet.setHeight(calculateAccessorySheetHeight(rootView));
        getKeyboard().hideSoftKeyboardOnly(rootView);
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
        if (!isInitialized()) return;
        mAccessorySheet.setActiveTab(tabIndex);
        if (mPopup != null && mPopup.isShowing()) mPopup.dismiss();
        if (is(EXTENDING_KEYBOARD)) {
            mModel.set(KEYBOARD_EXTENSION_STATE, REPLACING_KEYBOARD);
        } else if (is(FLOATING_BAR)) {
            mModel.set(KEYBOARD_EXTENSION_STATE, FLOATING_SHEET);
        }
    }

    @Override
    public void onCloseAccessorySheet() {
        if (is(REPLACING_KEYBOARD) || is(WAITING_TO_REPLACE)) {
            mModel.set(KEYBOARD_EXTENSION_STATE, FLOATING_SHEET);
        } else if (is(FLOATING_SHEET)) {
            mModel.set(KEYBOARD_EXTENSION_STATE, FLOATING_BAR);
        }
    }

    /**
     * Opens the keyboard which implicitly dismisses the sheet. Without open sheet, this is a NoOp.
     */
    void swapSheetWithKeyboard() {
        if (isInitialized() && mAccessorySheet.isShown()) onCloseAccessorySheet();
    }

    private void changeBottomControlSpaceForState(int extensionState) {
        if (extensionState == WAITING_TO_REPLACE) return; // Don't change yet.
        int newControlsHeight = 0;
        int newControlsOffset = 0;
        if (requiresVisibleBar(extensionState)) {
            newControlsHeight = mActivity.getResources().getDimensionPixelSize(
                    R.dimen.keyboard_accessory_suggestion_height);
        }
        if (requiresVisibleSheet(extensionState)) {
            newControlsHeight += mAccessorySheet.getHeight();
            newControlsOffset += mAccessorySheet.getHeight();
        }
        mKeyboardAccessory.setBottomOffset(newControlsOffset);
        mKeyboardExtensionViewResizer.setKeyboardExtensionHeight(newControlsHeight);
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
        return mActivity.getActivityTabProvider().get();
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

    /**
     * Uses the keyboard (if available) to determine the height of the accessory sheet.
     * @param rootView Root view of the current content view -- used to estimate the height unless
     *                 the more reliable InsetObserver is available.
     * @return The estimated keyboard height or enough space to display at least three suggestions.
     */
    private @Px int calculateAccessorySheetHeight(View rootView) {
        InsetObserverView insetObserver = mInsetObserverViewSupplier.get();
        int minimalSheetHeight = 3
                * mActivity.getResources().getDimensionPixelSize(
                        R.dimen.keyboard_accessory_suggestion_height);
        int newSheetHeight = insetObserver != null
                ? insetObserver.getSystemWindowInsetsBottom()
                : getKeyboard().calculateKeyboardHeight(rootView);
        newSheetHeight = Math.max(minimalSheetHeight, newSheetHeight);
        return newSheetHeight;
    }

    /**
     * Double-checks that the accessory sheet height doesn't cover the whole page.
     */
    private void restrictAccessorySheetHeight() {
        if (!is(FLOATING_SHEET) && !is(REPLACING_KEYBOARD)) return;
        WebContents webContents = mActivity.getCurrentWebContents();
        if (webContents == null) return;
        float density = mWindowAndroid.getDisplay().getDipScale();
        // The maximal height for the sheet ensures a minimal amount of WebContents space.
        @Px
        int maxHeight = mKeyboardExtensionViewResizer.getHeight();
        maxHeight += Math.round(density * webContents.getHeight());
        maxHeight -= Math.round(density * MINIMAL_AVAILABLE_VERTICAL_SPACE);
        maxHeight -= calculateAccessoryBarHeight();
        if (mAccessorySheet.getHeight() <= maxHeight) return; // Sheet height needs no adjustment!
        mAccessorySheet.setHeight(maxHeight);
        changeBottomControlSpaceForState(mModel.get(KEYBOARD_EXTENSION_STATE));
    }

    private @Px int calculateAccessoryBarHeight() {
        if (!mKeyboardAccessory.isShown()) return 0;
        return mActivity.getResources().getDimensionPixelSize(
                R.dimen.keyboard_accessory_suggestion_height);
    }

    private void refreshTabs() {
        if (!isInitialized()) return;
        ManualFillingState state = mStateCache.getStateFor(mActivity.getCurrentWebContents());
        state.notifyObservers();
        KeyboardAccessoryData.Tab[] tabs = state.getTabs();
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
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.PASSWORDS_KEYBOARD_ACCESSORY)) {
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
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.PASSWORDS_KEYBOARD_ACCESSORY)) {
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

    private boolean isFloating(@KeyboardExtensionState int state) {
        return (state & StateProperty.FLOATING) != 0;
    }

    private boolean requiresVisibleBar(@KeyboardExtensionState int state) {
        return (state & StateProperty.BAR) != 0;
    }

    private boolean requiresVisibleSheet(@KeyboardExtensionState int state) {
        return (state & StateProperty.VISIBLE_SHEET) != 0;
    }

    private boolean requiresHiddenSheet(int state) {
        return (state & StateProperty.HIDDEN_SHEET) != 0;
    }

    private boolean is(@KeyboardExtensionState int state) {
        return mModel.get(KEYBOARD_EXTENSION_STATE) == state;
    }

    @VisibleForTesting
    void setInsetObserverViewSupplier(Supplier<InsetObserverView> insetObserverViewSupplier) {
        mInsetObserverViewSupplier = insetObserverViewSupplier;
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

    @VisibleForTesting
    PropertyModel getModelForTesting() {
        return mModel;
    }

    @VisibleForTesting
    KeyboardAccessoryCoordinator getKeyboardAccessory() {
        return mKeyboardAccessory;
    }
}
