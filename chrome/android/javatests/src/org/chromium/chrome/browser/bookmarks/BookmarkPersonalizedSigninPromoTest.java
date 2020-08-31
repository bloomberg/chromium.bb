// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.accounts.Account;
import android.app.Activity;
import android.content.Context;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.signin.SigninActivityLauncher;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.components.signin.metrics.SigninAccessPoint;

/**
 * Tests for the personalized signin promo on the Bookmarks page.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class BookmarkPersonalizedSigninPromoTest {
    @Rule
    public final SyncTestRule mSyncTestRule = new SyncTestRule();

    private final SigninActivityLauncher mMockSigninActivityLauncher =
            mock(SigninActivityLauncher.class);

    @Before
    public void setUp() {
        BookmarkPromoHeader.forcePromoStateForTests(
                BookmarkPromoHeader.PromoState.PROMO_SIGNIN_PERSONALIZED);
        SigninActivityLauncher.setLauncherForTest(mMockSigninActivityLauncher);
        mSyncTestRule.startMainActivityFromLauncher();
    }

    @After
    public void tearDown() {
        SigninActivityLauncher.setLauncherForTest(null);
        BookmarkPromoHeader.forcePromoStateForTests(null);
    }

    @Test
    @MediumTest
    public void testSigninButtonDefaultAccount() {
        doNothing()
                .when(SigninActivityLauncher.get())
                .launchActivityForPromoDefaultFlow(any(Context.class), anyInt(), anyString());
        Account account = SigninTestUtil.addTestAccount();
        showBookmarkManagerAndCheckSigninPromoIsDisplayed();
        onView(withId(R.id.signin_promo_signin_button)).perform(click());
        verify(mMockSigninActivityLauncher)
                .launchActivityForPromoDefaultFlow(any(Activity.class),
                        eq(SigninAccessPoint.BOOKMARK_MANAGER), eq(account.name));
    }

    @Test
    @MediumTest
    public void testSigninButtonNotDefaultAccount() {
        doNothing()
                .when(SigninActivityLauncher.get())
                .launchActivityForPromoChooseAccountFlow(any(Context.class), anyInt(), anyString());
        Account account = SigninTestUtil.addTestAccount();
        showBookmarkManagerAndCheckSigninPromoIsDisplayed();
        onView(withId(R.id.signin_promo_choose_account_button)).perform(click());
        verify(mMockSigninActivityLauncher)
                .launchActivityForPromoChooseAccountFlow(any(Activity.class),
                        eq(SigninAccessPoint.BOOKMARK_MANAGER), eq(account.name));
    }

    @Test
    @MediumTest
    public void testSigninButtonNewAccount() {
        doNothing()
                .when(SigninActivityLauncher.get())
                .launchActivityForPromoAddAccountFlow(any(Context.class), anyInt());
        showBookmarkManagerAndCheckSigninPromoIsDisplayed();
        onView(withId(R.id.signin_promo_signin_button)).perform(click());
        verify(mMockSigninActivityLauncher)
                .launchActivityForPromoAddAccountFlow(
                        any(Activity.class), eq(SigninAccessPoint.BOOKMARK_MANAGER));
    }

    private void showBookmarkManagerAndCheckSigninPromoIsDisplayed() {
        BookmarkTestUtil.showBookmarkManager(mSyncTestRule.getActivity());
        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));
    }
}
