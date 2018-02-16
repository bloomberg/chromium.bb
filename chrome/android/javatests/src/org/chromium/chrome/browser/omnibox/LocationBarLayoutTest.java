// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.support.test.filters.SmallTest;
import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.widget.TintedImageButton;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/**
 * Unit tests for {@link LocationBarLayout}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class LocationBarLayoutTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    private LocationBarLayout getLocationBar() {
        return (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
    }

    private TintedImageButton getDeleteButton() {
        return (TintedImageButton) mActivityTestRule.getActivity().findViewById(R.id.delete_button);
    }

    private TintedImageButton getMicButton() {
        return (TintedImageButton) mActivityTestRule.getActivity().findViewById(R.id.mic_button);
    }

    private void setUrlBarTextAndFocus(String text)
            throws ExecutionException, InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(new Callable<Void>() {
            @Override
            public Void call() throws InterruptedException {
                mActivityTestRule.typeInOmnibox(text, false);
                getLocationBar().onUrlFocusChange(true);
                return null;
            }
        });
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.OMNIBOX_VOICE_SEARCH_ALWAYS_VISIBLE)
    @Feature({"OmniboxVoiceSearchAlwaysVisible"})
    public void testNotShowingVoiceSearchButtonIfUrlBarContainsTextAndFlagIsDisabled()
            throws ExecutionException, InterruptedException {
        setUrlBarTextAndFocus("testing");

        Assert.assertEquals(getDeleteButton().getVisibility(), View.VISIBLE);
        Assert.assertNotEquals(getMicButton().getVisibility(), View.VISIBLE);
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.OMNIBOX_VOICE_SEARCH_ALWAYS_VISIBLE)
    @Feature({"OmniboxVoiceSearchAlwaysVisible"})
    public void testShowingVoiceSearchButtonIfUrlBarIsEmptyAndFlagIsDisabled()
            throws ExecutionException, InterruptedException {
        setUrlBarTextAndFocus("");

        Assert.assertNotEquals(getDeleteButton().getVisibility(), View.VISIBLE);
        Assert.assertEquals(getMicButton().getVisibility(), View.VISIBLE);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_VOICE_SEARCH_ALWAYS_VISIBLE)
    @Feature({"OmniboxVoiceSearchAlwaysVisible"})
    public void testShowingVoiceAndDeleteButtonsShowingIfUrlBarContainsText()
            throws ExecutionException, InterruptedException {
        setUrlBarTextAndFocus("testing");

        Assert.assertEquals(getDeleteButton().getVisibility(), View.VISIBLE);
        Assert.assertEquals(getMicButton().getVisibility(), View.VISIBLE);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_VOICE_SEARCH_ALWAYS_VISIBLE)
    @Feature({"OmniboxVoiceSearchAlwaysVisible"})
    public void testShowingOnlyVoiceButtonIfUrlBarIsEmpty()
            throws ExecutionException, InterruptedException {
        setUrlBarTextAndFocus("");

        Assert.assertNotEquals(getDeleteButton().getVisibility(), View.VISIBLE);
        Assert.assertEquals(getMicButton().getVisibility(), View.VISIBLE);
    }
}