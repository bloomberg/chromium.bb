// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.not;
import static org.hamcrest.CoreMatchers.notNullValue;
import static org.hamcrest.CoreMatchers.nullValue;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.autofill.keyboard_accessory.AccessoryAction.GENERATE_PASSWORD_AUTOMATIC;
import static org.chromium.chrome.browser.tab.Tab.INVALID_TAB_ID;
import static org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType.FROM_BROWSER_ACTIONS;
import static org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType.FROM_CLOSE;
import static org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType.FROM_NEW;
import static org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType.FROM_USER;

import android.graphics.drawable.Drawable;
import android.support.annotation.Nullable;
import android.view.ViewStub;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Action;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Item;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.PropertyProvider;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Provider;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.modelutil.ListModel;
import org.chromium.chrome.browser.modelutil.ListObservable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;
import java.util.Map;

/**
 * Controller tests for the root controller for interactions with the manual filling UI.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.PASSWORDS_KEYBOARD_ACCESSORY)
@DisableFeatures(ChromeFeatureList.EXPERIMENTAL_UI)
public class ManualFillingControllerTest {
    @Mock
    private WindowAndroid mMockWindow;
    @Mock
    private ChromeActivity mMockActivity;
    @Mock
    private ViewStub mMockViewStub;
    @Mock
    private KeyboardAccessoryView mMockView;
    @Mock
    private ListObservable.ListObserver<Void> mMockTabListObserver;
    @Mock
    private ListObservable.ListObserver<Void> mMockItemListObserver;
    @Mock
    private TabModelSelector mMockTabModelSelector;
    private ChromeFullscreenManager mFullScreenManager;
    @Mock
    private Drawable mMockIcon;

    @Rule
    public Features.JUnitProcessor mFeaturesProcessor = new Features.JUnitProcessor();

    private ManualFillingCoordinator mController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mMockViewStub.inflate()).thenReturn(mMockView);
        when(mMockWindow.getActivity()).thenReturn(new WeakReference<>(mMockActivity));
        when(mMockActivity.getTabModelSelector()).thenReturn(mMockTabModelSelector);
        mFullScreenManager = new ChromeFullscreenManager(mMockActivity, 0);
        when(mMockActivity.getFullscreenManager()).thenReturn(mFullScreenManager);
        PasswordAccessorySheetCoordinator.IconProvider.getInstance().setIconForTesting(mMockIcon);
        mController = new ManualFillingCoordinator(mMockWindow, mMockViewStub, mMockViewStub);
    }

    @Test
    public void testCreatesValidSubComponents() {
        assertThat(mController, is(notNullValue()));
        assertThat(mController.getMediatorForTesting(), is(notNullValue()));
        assertThat(mController.getMediatorForTesting().getModelForTesting(), is(notNullValue()));
        assertThat(mController.getKeyboardAccessory(), is(notNullValue()));
        assertThat(mController.getMediatorForTesting().getAccessorySheet(), is(notNullValue()));
    }

    @Test
    public void testAddingNewTabIsAddedToAccessoryAndSheet() {
        KeyboardAccessoryModel keyboardAccessoryModel =
                mController.getKeyboardAccessory().getMediatorForTesting().getModelForTesting();
        keyboardAccessoryModel.addTabListObserver(mMockTabListObserver);
        AccessorySheetModel accessorySheetModel = mController.getMediatorForTesting()
                                                          .getAccessorySheet()
                                                          .getMediatorForTesting()
                                                          .getModelForTesting();
        accessorySheetModel.getTabList().addObserver(mMockTabListObserver);

        assertThat(keyboardAccessoryModel.getTabList().size(), is(0));
        mController.getMediatorForTesting().addTab(
                new KeyboardAccessoryData.Tab(null, null, 0, 0, null));

        verify(mMockTabListObserver).onItemRangeInserted(keyboardAccessoryModel.getTabList(), 0, 1);
        verify(mMockTabListObserver).onItemRangeInserted(accessorySheetModel.getTabList(), 0, 1);
        assertThat(keyboardAccessoryModel.getTabList().size(), is(1));
        assertThat(accessorySheetModel.getTabList().size(), is(1));
    }

    @Test
    public void testAddingBrowserTabsCreatesValidAccessoryState() {
        ManualFillingMediator mediator = mController.getMediatorForTesting();
        Map<Tab, ManualFillingMediator.AccessoryState> model = mediator.getModelForTesting();
        assertThat(model.size(), is(0));

        // Emulate adding a tab. Expect the model to have another entry.
        Tab firstTab = addTab(mediator, 1111, null);
        assertThat(model.size(), is(1));
        assertThat(model.get(firstTab), notNullValue());

        // Emulate adding a second tab. Expect the model to have another entry.
        Tab secondTab = addTab(mediator, 2222, firstTab);
        assertThat(model.size(), is(2));
        assertThat(model.get(secondTab), notNullValue());

        assertThat(model.get(firstTab), not(equalTo(model.get(secondTab))));
    }

    @Test
    public void testPasswordItemsPersistAfterSwitchingBrowserTabs() {
        ManualFillingMediator mediator = mController.getMediatorForTesting();
        Provider<Item> firstTabProvider = new PropertyProvider<>();
        Provider<Item> secondTabProvider = new PropertyProvider<>();

        // Simulate opening a new tab which automatically triggers the registration:
        Tab firstTab = addTab(mediator, 1111, null);
        mController.registerPasswordProvider(firstTabProvider);
        firstTabProvider.notifyObservers(new Item[] {
                Item.createSuggestion("FirstPassword", "FirstPassword", true, result -> {}, null)});
        assertThat(mediator.getPasswordAccessorySheet().getModelForTesting().get(0).getCaption(),
                is("FirstPassword"));

        // Simulate creating a second tab:
        Tab secondTab = addTab(mediator, 2222, firstTab);
        mController.registerPasswordProvider(secondTabProvider);
        secondTabProvider.notifyObservers(new Item[] {Item.createSuggestion(
                "SecondPassword", "SecondPassword", true, result -> {}, null)});
        assertThat(mediator.getPasswordAccessorySheet().getModelForTesting().get(0).getCaption(),
                is("SecondPassword"));

        // Simulate switching back to the first tab:
        switchTab(mediator, /*from=*/secondTab, /*to=*/firstTab);
        assertThat(mediator.getPasswordAccessorySheet().getModelForTesting().get(0).getCaption(),
                is("FirstPassword"));

        // And back to the second:
        switchTab(mediator, /*from=*/firstTab, /*to=*/secondTab);
        assertThat(mediator.getPasswordAccessorySheet().getModelForTesting().get(0).getCaption(),
                is("SecondPassword"));
    }

    @Test
    public void testKeyboardAccessoryActionsPersistAfterSwitchingBrowserTabs() {
        ManualFillingMediator mediator = mController.getMediatorForTesting();
        PropertyProvider<Action> firstTabProvider =
                new PropertyProvider<>(GENERATE_PASSWORD_AUTOMATIC);
        PropertyProvider<Action> secondTabProvider =
                new PropertyProvider<>(GENERATE_PASSWORD_AUTOMATIC);
        ListModel<Action> keyboardActions = mediator.getKeyboardAccessory()
                                                    .getMediatorForTesting()
                                                    .getModelForTesting()
                                                    .getActionList();
        keyboardActions.addObserver(mMockItemListObserver);

        // Simulate opening a new tab which automatically triggers the registration:
        Tab firstTab = addTab(mediator, 1111, null);
        mController.registerActionProvider(firstTabProvider);
        firstTabProvider.notifyObservers(new Action[] {
                new Action("Generate Password", GENERATE_PASSWORD_AUTOMATIC, p -> {})});
        mMockItemListObserver.onItemRangeInserted(keyboardActions, 0, 1);
        assertThat(keyboardActions.get(0).getCaption(), is("Generate Password"));

        // Simulate creating a second tab:
        Tab secondTab = addTab(mediator, 2222, firstTab);
        mController.registerActionProvider(secondTabProvider);
        secondTabProvider.notifyObservers(new Action[0]);
        mMockItemListObserver.onItemRangeRemoved(keyboardActions, 0, 1);
        assertThat(keyboardActions.size(), is(0)); // No actions on this tab.

        // Simulate switching back to the first tab:
        switchTab(mediator, /*from=*/secondTab, /*to=*/firstTab);
        mMockItemListObserver.onItemRangeInserted(keyboardActions, 0, 1);
        assertThat(keyboardActions.get(0).getCaption(), is("Generate Password"));

        // And back to the second:
        switchTab(mediator, /*from=*/firstTab, /*to=*/secondTab);
        mMockItemListObserver.onItemRangeRemoved(keyboardActions, 0, 1);
        assertThat(keyboardActions.size(), is(0)); // Still no actions on this tab.
    }

    @Test
    public void testPasswordTabRestoredWhenSwitchingBrowserTabs() {
        ManualFillingMediator mediator = mController.getMediatorForTesting();
        KeyboardAccessoryModel keyboardAccessoryModel =
                mediator.getKeyboardAccessory().getMediatorForTesting().getModelForTesting();
        AccessorySheetModel accessorySheetModel =
                mediator.getAccessorySheet().getMediatorForTesting().getModelForTesting();

        assertThat(keyboardAccessoryModel.getTabList().size(), is(0));
        assertThat(accessorySheetModel.getTabList().size(), is(0));

        // Create a new tab with a passwords tab:
        Tab firstTab = addTab(mediator, 1111, null);
        mController.registerPasswordProvider(new PropertyProvider<>());
        // There should be a tab in accessory and sheet:
        assertThat(keyboardAccessoryModel.getTabList().size(), is(1));
        assertThat(accessorySheetModel.getTabList().size(), is(1));

        // Simulate creating a second tab without any tabs:
        Tab secondTab = addTab(mediator, 2222, firstTab);
        // There should be no tab in accessory and sheet:
        assertThat(keyboardAccessoryModel.getTabList().size(), is(0));
        assertThat(accessorySheetModel.getTabList().size(), is(0));

        // Simulate switching back to the first tab:
        switchTab(mediator, /*from=*/secondTab, /*to=*/firstTab);
        // There should be a tab in accessory and sheet:
        assertThat(keyboardAccessoryModel.getTabList().size(), is(1));
        assertThat(accessorySheetModel.getTabList().size(), is(1));

        // And back to the second:
        switchTab(mediator, /*from=*/firstTab, /*to=*/secondTab);
        // Still no tab in accessory and sheet:
        assertThat(keyboardAccessoryModel.getTabList().size(), is(0));
        assertThat(accessorySheetModel.getTabList().size(), is(0));
    }

    @Test
    public void testRecoversFromInvalidState() {
        ManualFillingMediator mediator = mController.getMediatorForTesting();

        // Open a tab but pretend that the states became inconsistent.
        Tab tab = addTab(mediator, 1111, null);
        mediator.getModelForTesting().get(tab).mPasswordAccessorySheet =
                new PasswordAccessorySheetCoordinator(mMockActivity);

        // Create a new tab with a passwords tab:
        addTab(mediator, 1111, tab);
    }

    @Test
    public void testTreatNeverProvidedActionsAsEmptyActionList() {
        ManualFillingMediator mediator = mController.getMediatorForTesting();
        KeyboardAccessoryModel keyboardAccessoryModel =
                mediator.getKeyboardAccessory().getMediatorForTesting().getModelForTesting();

        // Open a tab.
        Tab tab = addTab(mediator, 1111, null);
        // Add an action provider that never provided actions.
        mController.registerActionProvider(
                new PropertyProvider<Action>(GENERATE_PASSWORD_AUTOMATIC));
        assertThat(keyboardAccessoryModel.getActionList().size(), is(0));

        // Create a new tab with an action:
        Tab secondTab = addTab(mediator, 1111, tab);
        PropertyProvider<Action> provider =
                new PropertyProvider<Action>(GENERATE_PASSWORD_AUTOMATIC);
        mController.registerActionProvider(provider);
        provider.notifyObservers(new Action[] {
                new Action("Test Action", GENERATE_PASSWORD_AUTOMATIC, (action) -> {})});
        assertThat(keyboardAccessoryModel.getActionList().size(), is(1));

        switchTab(mediator, secondTab, tab);
        assertThat(keyboardAccessoryModel.getActionList().size(), is(0));
    }

    @Test
    public void testUpdatesInactiveAccessory() {
        ManualFillingMediator mediator = mController.getMediatorForTesting();
        KeyboardAccessoryModel keyboardAccessoryModel =
                mediator.getKeyboardAccessory().getMediatorForTesting().getModelForTesting();

        // Open a tab.
        Tab tab = addTab(mediator, 1111, null);
        // Add an action provider that hasn't provided actions yet.
        PropertyProvider<Action> delayedProvider =
                new PropertyProvider<>(GENERATE_PASSWORD_AUTOMATIC);
        mController.registerActionProvider(delayedProvider);
        assertThat(keyboardAccessoryModel.getActionList().size(), is(0));

        // Create and switch to a new tab:
        Tab secondTab = addTab(mediator, 1111, tab);
        PropertyProvider<Action> provider =
                new PropertyProvider<Action>(GENERATE_PASSWORD_AUTOMATIC);
        mController.registerActionProvider(provider);

        // And provide data to the active tab.
        provider.notifyObservers(new Action[] {
                new Action("Test Action", GENERATE_PASSWORD_AUTOMATIC, (action) -> {})});
        // Now, have the delayed provider provide data for the backgrounded tab.
        delayedProvider.notifyObservers(
                new Action[] {new Action("Delayed", GENERATE_PASSWORD_AUTOMATIC, (action) -> {})});

        // The current tab should not be influenced by the delayed provider.
        assertThat(keyboardAccessoryModel.getActionList().size(), is(1));
        assertThat(keyboardAccessoryModel.getActionList().get(0).getCaption(), is("Test Action"));

        // Switching tabs back should only show the action that was received in the background.
        switchTab(mediator, secondTab, tab);
        assertThat(keyboardAccessoryModel.getActionList().size(), is(1));
        assertThat(keyboardAccessoryModel.getActionList().get(0).getCaption(), is("Delayed"));
    }

    @Test
    public void testDestroyingTabCleansModelForThisTab() {
        ManualFillingMediator mediator = mController.getMediatorForTesting();
        KeyboardAccessoryModel keyboardAccessoryModel =
                mediator.getKeyboardAccessory().getMediatorForTesting().getModelForTesting();
        AccessorySheetModel accessorySheetModel = mController.getMediatorForTesting()
                                                          .getAccessorySheet()
                                                          .getMediatorForTesting()
                                                          .getModelForTesting();

        Provider<Item> firstTabProvider = new PropertyProvider<>();
        PropertyProvider<Action> firstActionProvider =
                new PropertyProvider<Action>(GENERATE_PASSWORD_AUTOMATIC);
        Provider<Item> secondTabProvider = new PropertyProvider<>();
        PropertyProvider<Action> secondActionProvider =
                new PropertyProvider<Action>(GENERATE_PASSWORD_AUTOMATIC);

        // Simulate opening a new tab:
        Tab firstTab = addTab(mediator, 1111, null);
        mController.registerPasswordProvider(firstTabProvider);
        mController.registerActionProvider(firstActionProvider);
        firstTabProvider.notifyObservers(new Item[] {
                Item.createSuggestion("FirstPassword", "FirstPassword", true, result -> {}, null)});
        firstActionProvider.notifyObservers(new Action[] {
                new Action("2BDestroyed", GENERATE_PASSWORD_AUTOMATIC, (action) -> {})});

        // Create and switch to a new tab: (because destruction shouldn't rely on tab to be active)
        addTab(mediator, 2222, firstTab);
        mController.registerPasswordProvider(secondTabProvider);
        mController.registerActionProvider(secondActionProvider);
        secondTabProvider.notifyObservers(new Item[] {Item.createSuggestion(
                "SecondPassword", "SecondPassword", true, result -> {}, null)});
        secondActionProvider.notifyObservers(
                new Action[] {new Action("2BKept", GENERATE_PASSWORD_AUTOMATIC, (action) -> {})});

        // The current tab should be valid.
        assertThat(keyboardAccessoryModel.getTabList().size(), is(1));
        assertThat(accessorySheetModel.getTabList().size(), is(1));
        assertThat(keyboardAccessoryModel.getActionList().size(), is(1));
        assertThat(keyboardAccessoryModel.getActionList().get(0).getCaption(), is("2BKept"));

        // Request destruction of the first Tab:
        mediator.getTabObserverForTesting().onDestroyed(firstTab);

        // The current tab should not be influenced by the destruction.
        assertThat(keyboardAccessoryModel.getTabList().size(), is(1));
        assertThat(accessorySheetModel.getTabList().size(), is(1));
        assertThat(keyboardAccessoryModel.getActionList().size(), is(1));
        assertThat(keyboardAccessoryModel.getActionList().get(0).getCaption(), is("2BKept"));

        // The other tabs data should be gone.
        ManualFillingMediator.AccessoryState oldState = mediator.getModelForTesting().get(firstTab);
        if (oldState == null)
            return; // Having no state is fine - it would be completely destroyed then.

        assertThat(oldState.mActionsProvider, nullValue());

        if (oldState.mPasswordAccessorySheet == null)
            return; // Having no password sheet is fine - it would be completely destroyed then.
        assertThat(oldState.mPasswordAccessorySheet.getModelForTesting().size(), is(0));
    }

    /**
     * Creates a tab and calls the observer events as if it was just created and switched to.
     * @param mediator The {@link ManualFillingMediator} whose observers should be triggered.
     * @param id The id of the new tab.
     * @param lastTab A previous mocked {@link Tab} to be hidden. Needs |getId()|. May be null.
     * @return Returns a mock of the newly added {@link Tab}. Provides |getId()|.
     */
    private Tab addTab(ManualFillingMediator mediator, int id, @Nullable Tab lastTab) {
        int lastId = INVALID_TAB_ID;
        if (lastTab != null) {
            lastId = lastTab.getId();
            mediator.getTabObserverForTesting().onHidden(lastTab);
        }
        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(id);
        when(mMockTabModelSelector.getCurrentTab()).thenReturn(tab);
        mediator.getTabModelObserverForTesting().didAddTab(tab, FROM_BROWSER_ACTIONS);
        mediator.getTabObserverForTesting().onShown(tab);
        mediator.getTabModelObserverForTesting().didSelectTab(tab, FROM_NEW, lastId);
        return tab;
    }

    /**
     * Simulates switching to a different tab by calling observer events on the given |mediator|.
     * @param mediator The mediator providing the observer instances.
     * @param from The mocked {@link Tab} to be switched from. Needs |getId()|. May be null.
     * @param to The mocked {@link Tab} to be switched to. Needs |getId()|.
     */
    private void switchTab(ManualFillingMediator mediator, @Nullable Tab from, Tab to) {
        int lastId = INVALID_TAB_ID;
        if (from != null) {
            lastId = from.getId();
            mediator.getTabObserverForTesting().onHidden(from);
        }
        when(mMockTabModelSelector.getCurrentTab()).thenReturn(to);
        mediator.getTabModelObserverForTesting().didSelectTab(to, FROM_USER, lastId);
        mediator.getTabObserverForTesting().onShown(to);
    }

    /**
     * Simulates destroying the given tab by calling observer events on the given |mediator|.
     * @param mediator The mediator providing the observer instances.
     * @param tabToBeClosed The mocked {@link Tab} to be closed. Needs |getId()|.
     * @param next A mocked {@link Tab} to be switched to. Needs |getId()|. May be null.
     */
    private void closeTab(ManualFillingMediator mediator, Tab tabToBeClosed, @Nullable Tab next) {
        mediator.getTabModelObserverForTesting().willCloseTab(tabToBeClosed, false);
        mediator.getTabObserverForTesting().onHidden(tabToBeClosed);
        if (next != null) {
            when(mMockTabModelSelector.getCurrentTab()).thenReturn(next);
            mediator.getTabModelObserverForTesting().didSelectTab(
                    next, FROM_CLOSE, tabToBeClosed.getId());
        }
        mediator.getTabObserverForTesting().onDestroyed(tabToBeClosed);
    }
}
