// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.mockito.Mockito.ignoreStubs;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import com.google.common.collect.ImmutableSet;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.Set;

/** Tests for {@link SigninPromoUtil}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SigninPromoUtilTest {
    private ChromePreferenceManager mPreferenceManager;

    @Before
    public void setUp() {
        mPreferenceManager = Mockito.mock(ChromePreferenceManager.class);
        // Return default value if last account names are requested
        when(mPreferenceManager.getSigninPromoLastAccountNames()).thenReturn(null);
    }

    @After
    public void tearDown() {
        verifyNoMoreInteractions(ignoreStubs(mPreferenceManager));
    }

    @Test
    public void whenNoLastShownVersionShouldReturnFalseAndSaveVersion() {
        when(mPreferenceManager.getSigninPromoLastShownVersion()).thenReturn(0);
        Assert.assertFalse(SigninPromoUtil.shouldLaunchSigninPromo(
                mPreferenceManager, 42, false, false, ImmutableSet.of("test@gmail.com")));
        verify(mPreferenceManager).setSigninPromoLastShownVersion(42);
    }

    @Test
    public void whenSignedInShouldReturnFalse() {
        when(mPreferenceManager.getSigninPromoLastShownVersion()).thenReturn(38);
        Assert.assertFalse(SigninPromoUtil.shouldLaunchSigninPromo(
                mPreferenceManager, 42, true, false, ImmutableSet.of("test@gmail.com")));
    }

    @Test
    public void whenWasSignedInShouldReturnFalse() {
        when(mPreferenceManager.getSigninPromoLastShownVersion()).thenReturn(38);
        Assert.assertFalse(SigninPromoUtil.shouldLaunchSigninPromo(
                mPreferenceManager, 42, false, true, ImmutableSet.of("test@gmail.com")));
    }

    @Test
    public void whenNoAccountsShouldReturnFalse() {
        when(mPreferenceManager.getSigninPromoLastShownVersion()).thenReturn(38);
        Assert.assertFalse(SigninPromoUtil.shouldLaunchSigninPromo(
                mPreferenceManager, 42, false, false, ImmutableSet.of()));
    }

    @Test
    public void whenVersionDifferenceTooSmallShouldReturnFalse() {
        when(mPreferenceManager.getSigninPromoLastShownVersion()).thenReturn(41);
        Assert.assertFalse(SigninPromoUtil.shouldLaunchSigninPromo(
                mPreferenceManager, 42, false, false, ImmutableSet.of("test@gmail.com")));
    }

    @Test
    public void whenAllConditionsAreMetShouldReturnTrue() {
        when(mPreferenceManager.getSigninPromoLastShownVersion()).thenReturn(40);
        Assert.assertTrue(SigninPromoUtil.shouldLaunchSigninPromo(
                mPreferenceManager, 42, false, false, ImmutableSet.of("test@gmail.com")));
    }

    @Test
    public void whenAccountListUnchangedShouldReturnFalse() {
        when(mPreferenceManager.getSigninPromoLastShownVersion()).thenReturn(40);
        Set<String> accountNames = ImmutableSet.of("test@gmail.com");
        when(mPreferenceManager.getSigninPromoLastAccountNames()).thenReturn(accountNames);
        Assert.assertFalse(SigninPromoUtil.shouldLaunchSigninPromo(
                mPreferenceManager, 42, false, false, accountNames));
    }
}
