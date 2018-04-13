// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.notNullValue;
import static org.hamcrest.Matchers.equalTo;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.support.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.modelutil.ListObservable;
import org.chromium.chrome.browser.modelutil.PropertyObservable.PropertyObserver;

import java.util.ArrayList;
import java.util.List;

/**
 * Controller tests for the keyboard accessory component.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class KeyboardAccessoryControllerTest {
    @Mock
    PropertyObserver<KeyboardAccessoryModel.PropertyKey> mMockPropertyObserver;
    @Mock
    ListObservable.ListObserver mMockTabListObserver;
    @Mock
    ListObservable.ListObserver mMockActionListObserver;

    private class TestActionListProvider implements KeyboardAccessoryData.ActionListProvider {
        private final List<KeyboardAccessoryData.ActionListObserver> mObservers = new ArrayList<>();

        @Override
        public void addObserver(KeyboardAccessoryData.ActionListObserver observer) {
            mObservers.add(observer);
        }

        public void sendActionsToReceivers(KeyboardAccessoryData.Action[] actions) {
            for (KeyboardAccessoryData.ActionListObserver observer : mObservers) {
                observer.onActionsAvailable(actions);
            }
        }
    }

    private static class FakeTab implements KeyboardAccessoryData.Tab {}
    private static class FakeAction implements KeyboardAccessoryData.Action {}

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    @SmallTest
    @Feature({"keyboard-accessory"})
    public void testCreatesValidSubComponents() {
        final KeyboardAccessoryCoordinator coordinator = new KeyboardAccessoryCoordinator();
        final KeyboardAccessoryMediator mediator = coordinator.getMediatorForTesting();
        assertThat(mediator, is(notNullValue()));
        final KeyboardAccessoryModel model = mediator.getModelForTesting();
        assertThat(model, is(notNullValue()));
    }

    @Test
    @SmallTest
    @Feature({"keyboard-accessory"})
    public void testModelNotifiesVisibilityChangeOnShowAndHide() {
        final KeyboardAccessoryCoordinator coordinator = new KeyboardAccessoryCoordinator();
        final KeyboardAccessoryModel model =
                coordinator.getMediatorForTesting().getModelForTesting();
        model.addObserver(mMockPropertyObserver);

        // Calling show on the coordinator should make model propagate that it's visible.
        coordinator.show();
        verify(mMockPropertyObserver)
                .onPropertyChanged(model, KeyboardAccessoryModel.PropertyKey.VISIBLE);
        assertThat(model.isVisible(), is(true));

        // Calling hide on the coordinator should make model propagate that it's invisible.
        coordinator.hide();
        verify(mMockPropertyObserver, times(2))
                .onPropertyChanged(model, KeyboardAccessoryModel.PropertyKey.VISIBLE);
        assertThat(model.isVisible(), is(false));
    }

    @Test
    @SmallTest
    @Feature({"keyboard-accessory"})
    public void testChangingTabsNotifiesTabObserver() {
        final KeyboardAccessoryCoordinator coordinator = new KeyboardAccessoryCoordinator();
        final KeyboardAccessoryModel model =
                coordinator.getMediatorForTesting().getModelForTesting();
        final FakeTab testTab = new FakeTab();

        model.addTabListObserver(mMockTabListObserver);

        // Calling addTab on the coordinator should make model propagate that it has a new tab.
        coordinator.addTab(testTab);
        verify(mMockTabListObserver).onItemRangeInserted(model.getTabList(), 0, 1);
        assertThat(model.getTabList().getItemCount(), is(1));
        assertThat(model.getTabList().get(0), is(equalTo(testTab)));

        // Calling hide on the coordinator should make model propagate that it's invisible.
        coordinator.removeTab(testTab);
        verify(mMockTabListObserver).onItemRangeRemoved(model.getTabList(), 0, 1);
        assertThat(model.getTabList().getItemCount(), is(0));
    }

    @Test
    @SmallTest
    @Feature({"keyboard-accessory"})
    public void testModelNotifiesAboutActionsChangedByProvider() {
        final KeyboardAccessoryCoordinator coordinator = new KeyboardAccessoryCoordinator();
        final KeyboardAccessoryModel model =
                coordinator.getMediatorForTesting().getModelForTesting();

        final TestActionListProvider testProvider = new TestActionListProvider();
        final FakeAction testAction = new FakeAction();

        model.addActionListObserver(mMockActionListObserver);
        coordinator.registerActionListProvider(testProvider);

        // If the mediator receives an initial set of actions, the model should report an insertion.
        testProvider.sendActionsToReceivers(new KeyboardAccessoryData.Action[] {testAction});
        verify(mMockActionListObserver).onItemRangeInserted(model.getActionList(), 0, 1);
        assertThat(model.getActionList().getItemCount(), is(1));
        assertThat(model.getActionList().get(0), is(equalTo(testAction)));

        // If the mediator receives a new set of actions, the model should report a change.
        testProvider.sendActionsToReceivers(new KeyboardAccessoryData.Action[] {testAction});
        verify(mMockActionListObserver)
                .onItemRangeChanged(model.getActionList(), 0, 1, model.getActionList());
        assertThat(model.getActionList().getItemCount(), is(1));
        assertThat(model.getActionList().get(0), is(equalTo(testAction)));

        // If the mediator receives an empty set of actions, the model should report a deletion.
        testProvider.sendActionsToReceivers(new KeyboardAccessoryData.Action[] {});
        verify(mMockActionListObserver).onItemRangeRemoved(model.getActionList(), 0, 1);
        assertThat(model.getActionList().getItemCount(), is(0));

        // There should be no notification if no actions are reported repeatedly.
        testProvider.sendActionsToReceivers(new KeyboardAccessoryData.Action[] {});
        verifyNoMoreInteractions(mMockActionListObserver);
    }
}