// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static junit.framework.Assert.assertNotNull;
import static junit.framework.Assert.assertNull;

import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.R.id;

import android.support.test.filters.MediumTest;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * View tests for the keyboard accessory component.
 *
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class KeyboardAccessoryViewTest {
    private KeyboardAccessoryModel mModel;
    private KeyboardAccessoryViewBinder.AccessoryViewHolder mViewHolder;

    @Rule
    public ChromeActivityTestRule<ChromeTabbedActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeTabbedActivity.class);

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        mViewHolder = new KeyboardAccessoryViewBinder.AccessoryViewHolder(
                mActivityTestRule.getActivity().findViewById(id.keyboard_accessory_stub));
        mModel = new KeyboardAccessoryModel();
        mModel.addObserver(new PropertyModelChangeProcessor<>(
                mModel, mViewHolder, new KeyboardAccessoryViewBinder()));
    }

    @Test
    @MediumTest
    public void testAccessoryVisibilityChangedByModel() {
        // Initially, there shouldn't be a view yet.
        assertNull(mViewHolder.getView());

        // After setting the visibility to true, the view should exist and be visible.
        ThreadUtils.runOnUiThreadBlocking(() -> mModel.setVisible(true));
        assertNotNull(mViewHolder.getView());
        assertTrue(mViewHolder.getView().getVisibility() == View.VISIBLE);

        // After hiding the view, the view should still exist but be invisible.
        ThreadUtils.runOnUiThreadBlocking(() -> mModel.setVisible(false));
        assertNotNull(mViewHolder.getView());
        assertTrue(mViewHolder.getView().getVisibility() != View.VISIBLE);
    }
}