// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.notNullValue;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.verify;
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
import org.chromium.chrome.browser.modelutil.ListObservable;
import org.chromium.ui.base.WindowAndroid;

/**
 * Controller tests for the root controller for interactions with the manual filling UI.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ManualFillingControllerTest {
    @Mock
    private WindowAndroid mMockWindow;
    @Mock
    private ViewStub mMockViewStub;
    @Mock
    private KeyboardAccessoryView mMockView;
    @Mock
    private ListObservable.ListObserver mMockTabListObserver;

    private KeyboardAccessoryCoordinator mKeyboardAccessory;
    private ManualFillingController mController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mMockViewStub.inflate()).thenReturn(mMockView);
        mKeyboardAccessory = new KeyboardAccessoryCoordinator(mMockWindow, mMockViewStub);
        mController = new ManualFillingController(mKeyboardAccessory);
    }

    @Test
    @SmallTest
    public void testCreatesValidSubComponents() {
        assertThat(mKeyboardAccessory, is(notNullValue()));
        assertThat(mController, is(notNullValue()));
    }

    @Test
    @SmallTest
    public void testAddingBottomSheetsAddsTabsToAccessory() {
        KeyboardAccessoryModel keyboardAccessoryModel =
                mKeyboardAccessory.getMediatorForTesting().getModelForTesting();
        keyboardAccessoryModel.addTabListObserver(mMockTabListObserver);

        assertThat(keyboardAccessoryModel.getTabList().getItemCount(), is(0));
        mController.addAccessorySheet(new AccessorySheetCoordinator(mMockViewStub));

        verify(mMockTabListObserver).onItemRangeInserted(keyboardAccessoryModel.getTabList(), 0, 1);
        assertThat(keyboardAccessoryModel.getTabList().getItemCount(), is(1));
    }
}