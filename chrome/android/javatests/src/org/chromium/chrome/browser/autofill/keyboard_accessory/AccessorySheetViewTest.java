// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static junit.framework.Assert.assertNotNull;
import static junit.framework.Assert.assertNull;

import static org.junit.Assert.assertTrue;

import android.support.test.filters.MediumTest;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.modelutil.LazyViewBinderAdapter;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * View tests for the keyboard accessory sheet component.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AccessorySheetViewTest {
    private AccessorySheetModel mModel;
    private LazyViewBinderAdapter.StubHolder<View> mStubHolder;

    @Rule
    public ChromeActivityTestRule<ChromeTabbedActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeTabbedActivity.class);

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        mStubHolder = new LazyViewBinderAdapter.StubHolder<>(
                mActivityTestRule.getActivity().findViewById(R.id.keyboard_accessory_sheet_stub));
        mModel = new AccessorySheetModel();
        mModel.addObserver(new PropertyModelChangeProcessor<>(
                mModel, mStubHolder, new LazyViewBinderAdapter<>(new AccessorySheetViewBinder())));
    }

    @Test
    @MediumTest
    public void testAccessoryVisibilityChangedByModel() {
        // Initially, there shouldn't be a view yet.
        assertNull(mStubHolder.getView());

        // After setting the visibility to true, the view should exist and be visible.
        ThreadUtils.runOnUiThreadBlocking(() -> mModel.setVisible(true));
        assertNotNull(mStubHolder.getView());
        assertTrue(mStubHolder.getView().getVisibility() == View.VISIBLE);

        // After hiding the view, the view should still exist but be invisible.
        ThreadUtils.runOnUiThreadBlocking(() -> mModel.setVisible(false));
        assertNotNull(mStubHolder.getView());
        assertTrue(mStubHolder.getView().getVisibility() != View.VISIBLE);
    }
}