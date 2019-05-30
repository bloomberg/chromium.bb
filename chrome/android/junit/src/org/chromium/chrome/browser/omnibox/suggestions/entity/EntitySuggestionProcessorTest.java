// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.entity;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Color;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionHost;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Tests for {@link EntitySuggestionProcessor}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class EntitySuggestionProcessorTest {
    private static final int FALLBACK_COLOR = 0x00DEFC01;

    private EntitySuggestionProcessor mProcessor;
    private PropertyModel mModel;

    @Mock
    Context mContext;
    @Mock
    Resources mResources;
    @Mock
    SuggestionHost mSuggestionHost;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(mResources).when(mContext).getResources();

        mProcessor = new EntitySuggestionProcessor(mContext, mSuggestionHost);
        mModel = new PropertyModel(EntitySuggestionViewProperties.ALL_KEYS);

        doReturn(FALLBACK_COLOR).when(mResources).getColor(anyInt());
        doReturn(FALLBACK_COLOR).when(mContext).getColor(anyInt());
    }

    @Test
    public void applyImageDominantColor_ValidSpecs() {
        mProcessor.applyImageDominantColor("#fedcba", mModel);
        // Note: color processing deduces opacity.
        Assert.assertEquals(
                mModel.get(EntitySuggestionViewProperties.IMAGE_DOMINANT_COLOR), 0xfffedcba);
        mProcessor.applyImageDominantColor("red", mModel);
        Assert.assertEquals(
                mModel.get(EntitySuggestionViewProperties.IMAGE_DOMINANT_COLOR), Color.RED);
    }

    @Test
    public void applyImageDominantColor_InvalidSpecs() {
        mProcessor.applyImageDominantColor("", mModel);
        Assert.assertEquals(
                mModel.get(EntitySuggestionViewProperties.IMAGE_DOMINANT_COLOR), FALLBACK_COLOR);
        mProcessor.applyImageDominantColor("#", mModel);
        Assert.assertEquals(
                mModel.get(EntitySuggestionViewProperties.IMAGE_DOMINANT_COLOR), FALLBACK_COLOR);
        mProcessor.applyImageDominantColor(null, mModel);
        Assert.assertEquals(
                mModel.get(EntitySuggestionViewProperties.IMAGE_DOMINANT_COLOR), FALLBACK_COLOR);
        mProcessor.applyImageDominantColor("not a color", mModel);
        Assert.assertEquals(
                mModel.get(EntitySuggestionViewProperties.IMAGE_DOMINANT_COLOR), FALLBACK_COLOR);
    }
}
