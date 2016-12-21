// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import android.support.test.filters.SmallTest;
import android.test.InstrumentationTestCase;

import org.chromium.base.test.util.EnormousTest;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.components.signin.ChromeSigninController;

/**
 * Tests for {@link ChromeSigninUtils}.
 */
public class ChromeSigninUtilsTest extends InstrumentationTestCase {
    private static final String FAKE_ACCOUNT_USERNAME = "test@google.com";
    private static final String FAKE_ACCOUNT_PASSWORD = "$3cr3t";
    private static final String GOOGLE_ACCOUNT_USERNAME = "chromiumforandroid01@gmail.com";
    private static final String GOOGLE_ACCOUNT_PASSWORD = "chromeforandroid";
    private static final String GOOGLE_ACCOUNT_TYPE = "mail";

    private ChromeSigninUtils mSigninUtil;
    private ChromeSigninController mSigninController;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mSigninUtil = new ChromeSigninUtils(getInstrumentation());
        mSigninController = ChromeSigninController.get(getInstrumentation().getTargetContext());
        mSigninController.setSignedInAccountName(null);
        mSigninUtil.removeAllFakeAccountsFromOs();
        mSigninUtil.removeAllGoogleAccountsFromOs();
    }

    @SmallTest
    public void testActivityIsNotSignedInOnAppOrFakeOSorGoogleOS() {
        assertFalse("Should not be signed into app.",
                mSigninController.isSignedIn());
        assertFalse("Should not be signed into OS with fake account.",
                mSigninUtil.isExistingFakeAccountOnOs(FAKE_ACCOUNT_USERNAME));
        assertFalse("Should not be signed in on OS with Google account.",
                mSigninUtil.isExistingGoogleAccountOnOs(GOOGLE_ACCOUNT_USERNAME));
    }

    @SmallTest
    public void testIsSignedInOnApp() {
        mSigninUtil.addAccountToApp(FAKE_ACCOUNT_USERNAME);
        assertTrue("Should be signed on app.",
                mSigninController.isSignedIn());
        assertFalse("Should not be signed on OS with fake account.",
                mSigninUtil.isExistingFakeAccountOnOs(FAKE_ACCOUNT_USERNAME));
        assertFalse("Should not be signed in on OS with Google account.",
                mSigninUtil.isExistingGoogleAccountOnOs(GOOGLE_ACCOUNT_USERNAME));
    }

    @SmallTest
    public void testIsSignedInOnFakeOS() {
        mSigninUtil.addFakeAccountToOs(FAKE_ACCOUNT_USERNAME, FAKE_ACCOUNT_PASSWORD);
        assertFalse("Should not be signed in on app.",
                mSigninController.isSignedIn());
        assertTrue("Should be signed in on OS with fake account.",
                mSigninUtil.isExistingFakeAccountOnOs(FAKE_ACCOUNT_USERNAME));
        assertFalse("Should not be signed in on OS with Google account.",
                mSigninUtil.isExistingGoogleAccountOnOs(GOOGLE_ACCOUNT_USERNAME));
    }

    @FlakyTest(message = "https://crbug.com/517849")
    @EnormousTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    public void testIsSignedInOnGoogleOS() {
        mSigninUtil.addGoogleAccountToOs(GOOGLE_ACCOUNT_USERNAME, GOOGLE_ACCOUNT_PASSWORD,
                GOOGLE_ACCOUNT_TYPE);
        assertFalse("Should not be signed into app.",
                mSigninController.isSignedIn());
        assertFalse("Should not be signed into OS with fake account.",
                mSigninUtil.isExistingFakeAccountOnOs(FAKE_ACCOUNT_USERNAME));
        assertTrue("Should be signed in on OS with Google account.",
                mSigninUtil.isExistingGoogleAccountOnOs(GOOGLE_ACCOUNT_USERNAME));
    }

    @SmallTest
    public void testIsSignedInOnFakeOSandApp() {
        mSigninUtil.addAccountToApp(FAKE_ACCOUNT_USERNAME);
        mSigninUtil.addFakeAccountToOs(FAKE_ACCOUNT_USERNAME, FAKE_ACCOUNT_PASSWORD);
        assertTrue("Should be signed in on app.",
                mSigninController.isSignedIn());
        assertTrue("Should be signed in on OS with fake account.",
                mSigninUtil.isExistingFakeAccountOnOs(FAKE_ACCOUNT_USERNAME));
        assertFalse("Should not be signed in on OS with Google account.",
                mSigninUtil.isExistingGoogleAccountOnOs(GOOGLE_ACCOUNT_USERNAME));
    }

    @FlakyTest(message = "https://crbug.com/517849")
    @EnormousTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    public void testIsSignedInOnAppAndGoogleOS() {
        mSigninUtil.addAccountToApp(FAKE_ACCOUNT_USERNAME);
        mSigninUtil.addGoogleAccountToOs(GOOGLE_ACCOUNT_USERNAME, GOOGLE_ACCOUNT_PASSWORD,
                GOOGLE_ACCOUNT_TYPE);
        assertTrue("Should be signed into app.",
                mSigninController.isSignedIn());
        assertFalse("Should not be signed into OS with fake account.",
                mSigninUtil.isExistingFakeAccountOnOs(FAKE_ACCOUNT_USERNAME));
        assertTrue("Should be signed in on OS with Google account.",
                mSigninUtil.isExistingGoogleAccountOnOs(GOOGLE_ACCOUNT_USERNAME));
    }

    @FlakyTest(message = "https://crbug.com/517849")
    @EnormousTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    public void testIsSignedInOnFakeOSandGoogleOS() {
        mSigninUtil.addFakeAccountToOs(FAKE_ACCOUNT_USERNAME, FAKE_ACCOUNT_PASSWORD);
        mSigninUtil.addGoogleAccountToOs(GOOGLE_ACCOUNT_USERNAME, GOOGLE_ACCOUNT_PASSWORD,
                GOOGLE_ACCOUNT_TYPE);
        assertFalse("Should not be signed into app.",
                mSigninController.isSignedIn());
        assertTrue("Should be signed into OS with fake account.",
                mSigninUtil.isExistingFakeAccountOnOs(FAKE_ACCOUNT_USERNAME));
        assertTrue("Should be signed in on OS with Google account.",
                mSigninUtil.isExistingGoogleAccountOnOs(GOOGLE_ACCOUNT_USERNAME));
    }

    @FlakyTest(message = "https://crbug.com/517849")
    @EnormousTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    public void testIsSignedInOnAppAndFakeOSandGoogleOS() {
        mSigninUtil.addAccountToApp(FAKE_ACCOUNT_USERNAME);
        mSigninUtil.addFakeAccountToOs(FAKE_ACCOUNT_USERNAME, FAKE_ACCOUNT_PASSWORD);
        mSigninUtil.addGoogleAccountToOs(GOOGLE_ACCOUNT_USERNAME, GOOGLE_ACCOUNT_PASSWORD,
                GOOGLE_ACCOUNT_TYPE);
        assertTrue("Should be signed into app.",
                mSigninController.isSignedIn());
        assertTrue("Should be signed into OS with fake account.",
                mSigninUtil.isExistingFakeAccountOnOs(FAKE_ACCOUNT_USERNAME));
        assertTrue("Should be signed in on OS with Google account.",
                mSigninUtil.isExistingGoogleAccountOnOs(GOOGLE_ACCOUNT_USERNAME));
    }

    @Override
    protected void tearDown() throws Exception {
        mSigninController.setSignedInAccountName(null);
        mSigninUtil.removeAllFakeAccountsFromOs();
        mSigninUtil.removeAllGoogleAccountsFromOs();
        super.tearDown();
    }
}
