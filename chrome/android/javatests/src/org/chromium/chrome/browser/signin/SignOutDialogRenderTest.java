// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.pressBack;
import static android.support.test.espresso.matcher.ViewMatchers.isRoot;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.support.test.InstrumentationRegistry;
import android.view.View;

import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.signin.GAIAServiceType;
import org.chromium.ui.test.util.DisableAnimationsTestRule;
import org.chromium.ui.test.util.DummyUiActivityTestCase;

/**
 * Render tests for {@link SignOutDialogFragment}
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SignOutDialogRenderTest extends DummyUiActivityTestCase {
    private static final String TEST_DOMAIN = "test.domain.example.com";

    @Rule
    public final DisableAnimationsTestRule mNoAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule = new ChromeRenderTestRule();

    @Rule
    public final JniMocker mocker = new JniMocker();

    @Mock
    private SigninUtils.Natives mSigninUtilsNativeMock;

    @Mock
    private SigninManager mSigninManagerMock;

    @Before
    public void setUp() {
        initMocks(this);
        mocker.mock(SigninUtilsJni.TEST_HOOKS, mSigninUtilsNativeMock);
        IdentityServicesProvider.setInstanceForTests(mock(IdentityServicesProvider.class));
        when(IdentityServicesProvider.get().getSigninManager()).thenReturn(mSigninManagerMock);
    }

    @After
    public void tearDown() {
        // Since the Dialog dismiss calls native method, we need to close the dialog before the
        // Native mock SigninUtils.Natives gets removed.
        onView(isRoot()).perform(pressBack());
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSignOutDialogForManagedAccount() throws Exception {
        when(mSigninManagerMock.getManagementDomain()).thenReturn(TEST_DOMAIN);
        mRenderTestRule.render(showSignOutDialog(), "signout_dialog_for_managed_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSignOutDialogForNonManagedAccount() throws Exception {
        mRenderTestRule.render(showSignOutDialog(), "signout_dialog_for_non_managed_account");
    }

    private View showSignOutDialog() {
        SignOutDialogFragment signOutDialog =
                SignOutDialogFragment.create(GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
        signOutDialog.show(getActivity().getSupportFragmentManager(), null);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        return signOutDialog.getDialog().getWindow().getDecorView();
    }
}
