// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.when;

import android.content.res.Resources;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Tests for {@link CustomTabStatusBarColorProvider}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CustomTabStatusBarColorProviderTest {
    private static final int DEFAULT_COLOR = 0x11223344;
    private static final int FALLBACK_COLOR = 0x55667788;
    private static final int USER_PROVIDED_COLOR = 0x99aabbcc;

    @Mock public Resources mResources;
    @Mock public CustomTabIntentDataProvider mCustomTabIntentDataProvider;
    @Mock public CustomTabActivityTabProvider mCustomTabActivityTabProvider;
    private CustomTabStatusBarColorProvider mColorProvider;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mColorProvider = new CustomTabStatusBarColorProvider(mResources,
                mCustomTabIntentDataProvider, mCustomTabActivityTabProvider);

        // The color is accessed through ApiCompatibilityUtils which calls either
        // Resources#getColor(int, Theme) or Resources#getColor(int) depending on the Android
        // version. We mock out both calls so things don't break if we change the Android version
        // the tests are run with.
        when(mResources.getColor(anyInt(), any())).thenReturn(DEFAULT_COLOR);
        when(mResources.getColor(anyInt())).thenReturn(DEFAULT_COLOR);

        when(mCustomTabIntentDataProvider.getToolbarColor()).thenReturn(USER_PROVIDED_COLOR);
    }

    @Test
    public void fallsBackWhenOpenedByChrome() {
        when(mCustomTabIntentDataProvider.isOpenedByChrome()).thenReturn(true);

        Assert.assertEquals(FALLBACK_COLOR, mColorProvider.getBaseStatusBarColor(FALLBACK_COLOR));

        Assert.assertTrue(mColorProvider.isStatusBarDefaultThemeColor(true));
        Assert.assertFalse(mColorProvider.isStatusBarDefaultThemeColor(false));
    }

    @Test
    public void defaultThemeForPreviews() {
        Tab tab = Mockito.mock(Tab.class);
        when(tab.isPreview()).thenReturn(true);
        when(mCustomTabActivityTabProvider.getTab()).thenReturn(tab);

        Assert.assertEquals(DEFAULT_COLOR, mColorProvider.getBaseStatusBarColor(FALLBACK_COLOR));
        Assert.assertFalse(mColorProvider.isStatusBarDefaultThemeColor(true));
    }

    @Test
    public void userProvidedColor() {
        Assert.assertEquals(USER_PROVIDED_COLOR,
                mColorProvider.getBaseStatusBarColor(FALLBACK_COLOR));
        Assert.assertFalse(mColorProvider.isStatusBarDefaultThemeColor(true));
    }
}
