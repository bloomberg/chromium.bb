// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.notNullValue;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.support.test.filters.SmallTest;
import android.view.ViewStub;
import android.widget.LinearLayout;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.modelutil.PropertyObservable;

/**
 * Controller tests for the keyboard accessory bottom sheet component.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AccessorySheetControllerTest {
    @Mock
    private PropertyObservable
            .PropertyObserver<AccessorySheetModel.PropertyKey> mMockPropertyObserver;
    @Mock
    private ViewStub mMockViewStub;
    @Mock
    private LinearLayout mMockView;

    private AccessorySheetCoordinator mCoordinator;
    private AccessorySheetMediator mMediator;
    private AccessorySheetModel mModel;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mMockViewStub.inflate()).thenReturn(mMockView);
        mCoordinator = new AccessorySheetCoordinator(mMockViewStub);
        mMediator = mCoordinator.getMediatorForTesting();
        mModel = mMediator.getModelForTesting();
    }

    @Test
    @SmallTest
    public void testCreatesValidSubComponents() {
        assertThat(mCoordinator, is(notNullValue()));
        assertThat(mMediator, is(notNullValue()));
        assertThat(mModel, is(notNullValue()));
    }

    @Test
    @SmallTest
    @Feature({"keyboard-accessory"})
    public void testModelNotifiesAboutVisibilityOncePerChange() {
        mModel.addObserver(mMockPropertyObserver);

        // Calling show on the mediator should make model propagate that it's visible.
        mMediator.show();
        verify(mMockPropertyObserver)
                .onPropertyChanged(mModel, AccessorySheetModel.PropertyKey.VISIBLE);
        assertThat(mModel.isVisible(), is(true));

        // Calling show again does nothing.
        mMediator.show();
        verify(mMockPropertyObserver) // Still the same call and no new one added.
                .onPropertyChanged(mModel, AccessorySheetModel.PropertyKey.VISIBLE);

        // Calling hide on the mediator should make model propagate that it's invisible.
        mMediator.hide();
        verify(mMockPropertyObserver, times(2))
                .onPropertyChanged(mModel, AccessorySheetModel.PropertyKey.VISIBLE);
        assertThat(mModel.isVisible(), is(false));
    }
}