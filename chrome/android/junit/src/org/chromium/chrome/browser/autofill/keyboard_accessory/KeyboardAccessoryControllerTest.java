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
import static org.mockito.Mockito.when;

import android.support.test.filters.SmallTest;
import android.view.ViewStub;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.modelutil.ListObservable;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor;
import org.chromium.chrome.browser.modelutil.PropertyObservable.PropertyObserver;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.List;

/**
 * Controller tests for the keyboard accessory component.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class KeyboardAccessoryControllerTest {
    @Mock
    private PropertyObserver<KeyboardAccessoryModel.PropertyKey> mMockPropertyObserver;
    @Mock
    private ListObservable.ListObserver mMockTabListObserver;
    @Mock
    private ListObservable.ListObserver mMockActionListObserver;
    @Mock
    private WindowAndroid mMockWindow;
    @Mock
    private ViewStub mMockViewStub;
    @Mock
    private KeyboardAccessoryView mMockView;
    @Mock
    private PropertyModelChangeProcessor<KeyboardAccessoryModel, KeyboardAccessoryView,
            KeyboardAccessoryModel.PropertyKey> mMockModelChangeProcessor;

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

    private KeyboardAccessoryCoordinator mCoordinator;
    private KeyboardAccessoryModel mModel;
    private KeyboardAccessoryMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mMockViewStub.inflate()).thenReturn(mMockView);
        mCoordinator = new KeyboardAccessoryCoordinator(mMockWindow, mMockViewStub);
        mMediator = mCoordinator.getMediatorForTesting();
        mModel = mMediator.getModelForTesting();
    }

    @Test
    @SmallTest
    @Feature({"keyboard-accessory"})
    public void testCreatesValidSubComponents() {
        assertThat(mCoordinator, is(notNullValue()));
        assertThat(mMediator, is(notNullValue()));
        assertThat(mModel, is(notNullValue()));
        verify(mMockWindow).addKeyboardVisibilityListener(mMediator);
    }

    @Test
    @SmallTest
    @Feature({"keyboard-accessory"})
    public void testModelNotifiesVisibilityChangeOnShowAndHide() {
        mModel.addObserver(mMockPropertyObserver);

        // Calling show on the mediator should make model propagate that it's visible.
        mMediator.show();
        verify(mMockPropertyObserver)
                .onPropertyChanged(mModel, KeyboardAccessoryModel.PropertyKey.VISIBLE);
        assertThat(mModel.isVisible(), is(true));

        // Calling hide on the mediator should make model propagate that it's invisible.
        mMediator.hide();
        verify(mMockPropertyObserver, times(2))
                .onPropertyChanged(mModel, KeyboardAccessoryModel.PropertyKey.VISIBLE);
        assertThat(mModel.isVisible(), is(false));
    }

    @Test
    @SmallTest
    @Feature({"keyboard-accessory"})
    public void testChangingTabsNotifiesTabObserver() {
        final FakeTab testTab = new FakeTab();

        mModel.addTabListObserver(mMockTabListObserver);

        // Calling addTab on the coordinator should make model propagate that it has a new tab.
        mCoordinator.addTab(testTab);
        verify(mMockTabListObserver).onItemRangeInserted(mModel.getTabList(), 0, 1);
        assertThat(mModel.getTabList().getItemCount(), is(1));
        assertThat(mModel.getTabList().get(0), is(equalTo(testTab)));

        // Calling hide on the coordinator should make model propagate that it's invisible.
        mCoordinator.removeTab(testTab);
        verify(mMockTabListObserver).onItemRangeRemoved(mModel.getTabList(), 0, 1);
        assertThat(mModel.getTabList().getItemCount(), is(0));
    }

    @Test
    @SmallTest
    @Feature({"keyboard-accessory"})
    public void testModelNotifiesAboutActionsChangedByProvider() {
        final TestActionListProvider testProvider = new TestActionListProvider();
        final FakeAction testAction = new FakeAction();

        mModel.addActionListObserver(mMockActionListObserver);
        mCoordinator.registerActionListProvider(testProvider);

        // If the coordinator receives an initial actions, the model should report an insertion.
        testProvider.sendActionsToReceivers(new KeyboardAccessoryData.Action[] {testAction});
        verify(mMockActionListObserver).onItemRangeInserted(mModel.getActionList(), 0, 1);
        assertThat(mModel.getActionList().getItemCount(), is(1));
        assertThat(mModel.getActionList().get(0), is(equalTo(testAction)));

        // If the coordinator receives a new set of actions, the model should report a change.
        testProvider.sendActionsToReceivers(new KeyboardAccessoryData.Action[] {testAction});
        verify(mMockActionListObserver)
                .onItemRangeChanged(mModel.getActionList(), 0, 1, mModel.getActionList());
        assertThat(mModel.getActionList().getItemCount(), is(1));
        assertThat(mModel.getActionList().get(0), is(equalTo(testAction)));

        // If the coordinator receives an empty set of actions, the model should report a deletion.
        testProvider.sendActionsToReceivers(new KeyboardAccessoryData.Action[] {});
        verify(mMockActionListObserver).onItemRangeRemoved(mModel.getActionList(), 0, 1);
        assertThat(mModel.getActionList().getItemCount(), is(0));

        // There should be no notification if no actions are reported repeatedly.
        testProvider.sendActionsToReceivers(new KeyboardAccessoryData.Action[] {});
        verifyNoMoreInteractions(mMockActionListObserver);
    }

    @Test
    @SmallTest
    @Feature({"keyboard-accessory"})
    public void testModelDoesntNotifyUnchangedData() {
        mModel.addObserver(mMockPropertyObserver);

        // Calling show on the coordinator should make model propagate that it's visible.
        mCoordinator.show();
        verify(mMockPropertyObserver)
                .onPropertyChanged(mModel, KeyboardAccessoryModel.PropertyKey.VISIBLE);
        assertThat(mModel.isVisible(), is(true));

        // Marking it as visible again should not result in a second notification.
        mCoordinator.show();
        verify(mMockPropertyObserver) // Unchanged number of invocations.
                .onPropertyChanged(mModel, KeyboardAccessoryModel.PropertyKey.VISIBLE);
        assertThat(mModel.isVisible(), is(true));
    }
}