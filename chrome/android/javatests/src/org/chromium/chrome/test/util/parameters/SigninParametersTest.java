// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.parameters;

import android.support.test.filters.SmallTest;

import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.EnormousTest;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.parameter.Parameter;
import org.chromium.base.test.util.parameter.ParameterizedTest;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;

/**
 * Tester class for implementation of Signin testing and ParameterizedTest.
 */
@RetryOnFailure
public class SigninParametersTest extends ChromeActivityTestCaseBase<ChromeActivity> {
    private static final String GOOGLE_ACCOUNT_USERNAME = "chromiumforandroid01@gmail.com";
    private static final String GOOGLE_ACCOUNT_PASSWORD = "chromeforandroid";

    private AddFakeAccountToAppParameter mAddFakeAccountToAppParameter;
    private AddFakeAccountToOsParameter mAddFakeAccountToOsParameter;
    private AddGoogleAccountToOsParameter mAddGoogleAccountToOsParameter;

    public SigninParametersTest() {
        super(ChromeActivity.class);
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mAddFakeAccountToAppParameter =
                getAvailableParameter(AddFakeAccountToAppParameter.PARAMETER_TAG);
        mAddFakeAccountToOsParameter =
                getAvailableParameter(AddFakeAccountToOsParameter.PARAMETER_TAG);
        mAddGoogleAccountToOsParameter =
                getAvailableParameter(AddGoogleAccountToOsParameter.PARAMETER_TAG);
    }

    @SmallTest
    public void testActivityIsNotSignedInOnAppOrFakeOSorGoogleOS() {
        assertFalse("Should not be signed into app.",
                mAddFakeAccountToAppParameter.isSignedIn());
        assertFalse("Should not be signed into OS with fake account.",
                mAddFakeAccountToOsParameter.isSignedIn());
        assertFalse("Should not be signed in on OS with Google account.",
                mAddGoogleAccountToOsParameter.isSignedIn(GOOGLE_ACCOUNT_USERNAME));
    }

    /*
    @SmallTest
    @ParameterizedTest(parameters = {
            @Parameter(tag = AddFakeAccountToAppParameter.PARAMETER_TAG)})
    */
    @DisabledTest(message = "crbug.com/524189")
    public void testIsSignedInOnApp() {
        assertTrue("Should not be signed into app.",
                mAddFakeAccountToAppParameter.isSignedIn());
        assertFalse("Should not be signed into OS with fake account.",
                mAddFakeAccountToOsParameter.isSignedIn());
        assertFalse("Should not be signed in on OS with Google account.",
                mAddGoogleAccountToOsParameter.isSignedIn(GOOGLE_ACCOUNT_USERNAME));
    }

    @SmallTest
    @ParameterizedTest(parameters = {
            @Parameter(tag = AddFakeAccountToOsParameter.PARAMETER_TAG)})
    public void testIsSignedInOnFakeOS() {
        assertFalse("Should not be signed in on app.",
                mAddFakeAccountToAppParameter.isSignedIn());
        assertTrue("Should be signed in on OS with fake account.",
                mAddFakeAccountToOsParameter.isSignedIn());
        assertFalse("Should not be signed in on OS with Google account.",
                mAddGoogleAccountToOsParameter.isSignedIn(GOOGLE_ACCOUNT_USERNAME));
    }

    @FlakyTest
    @EnormousTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @ParameterizedTest(parameters = {
            @Parameter(
                    tag = AddGoogleAccountToOsParameter.PARAMETER_TAG,
                    arguments = {
                            @Parameter.Argument(
                                    name = AddGoogleAccountToOsParameter.ARGUMENT.USERNAME,
                                    stringVar = GOOGLE_ACCOUNT_USERNAME),
                            @Parameter.Argument(
                                    name = AddGoogleAccountToOsParameter.ARGUMENT.PASSWORD,
                                    stringVar = GOOGLE_ACCOUNT_PASSWORD)})})
    public void testIsSignedInOnGoogleOS() {
        assertFalse("Should not be signed into app.",
                mAddFakeAccountToAppParameter.isSignedIn());
        assertFalse("Should not be signed into OS with fake account.",
                mAddFakeAccountToOsParameter.isSignedIn());
        assertTrue("Should be signed in on OS with Google account.",
                mAddGoogleAccountToOsParameter.isSignedIn(GOOGLE_ACCOUNT_USERNAME));
    }

    /*
    @SmallTest
    @ParameterizedTest(parameters = {
            @Parameter(tag = AddFakeAccountToAppParameter.PARAMETER_TAG),
            @Parameter(tag = AddFakeAccountToOsParameter.PARAMETER_TAG)})
    */
    @DisabledTest(message = "crbug.com/524189")
    public void testIsSignedInOnFakeOSandApp() {
        assertTrue("Should be signed in on app.",
                mAddFakeAccountToAppParameter.isSignedIn());
        assertTrue("Should be signed in on OS with fake account.",
                mAddFakeAccountToOsParameter.isSignedIn());
        assertFalse("Should not be signed in on OS with Google account.",
                mAddGoogleAccountToOsParameter.isSignedIn(GOOGLE_ACCOUNT_USERNAME));
    }

    /*
    @FlakyTest
    @EnormousTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @ParameterizedTest(parameters = {
            @Parameter(tag = AddFakeAccountToAppParameter.PARAMETER_TAG),
            @Parameter(
                    tag = AddGoogleAccountToOsParameter.PARAMETER_TAG,
                    arguments = {
                            @Parameter.Argument(
                                    name = AddGoogleAccountToOsParameter.ARGUMENT.USERNAME,
                                    stringVar = GOOGLE_ACCOUNT_USERNAME),
                            @Parameter.Argument(
                                    name = AddGoogleAccountToOsParameter.ARGUMENT.PASSWORD,
                                    stringVar = GOOGLE_ACCOUNT_PASSWORD)})})
    */
    @DisabledTest(message = "crbug.com/524189")
    public void testIsSignedInOnAppAndGoogleOS() {
        assertTrue("Should be signed into app.",
                mAddFakeAccountToAppParameter.isSignedIn());
        assertFalse("Should not be signed into OS with fake account.",
                mAddFakeAccountToOsParameter.isSignedIn());
        assertTrue("Should be signed in on OS with Google account.",
                mAddGoogleAccountToOsParameter.isSignedIn(GOOGLE_ACCOUNT_USERNAME));
    }

    @FlakyTest
    @EnormousTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @ParameterizedTest(parameters = {
            @Parameter(tag = AddFakeAccountToOsParameter.PARAMETER_TAG),
            @Parameter(
                    tag = AddGoogleAccountToOsParameter.PARAMETER_TAG,
                    arguments = {
                            @Parameter.Argument(
                                    name = AddGoogleAccountToOsParameter.ARGUMENT.USERNAME,
                                    stringVar = GOOGLE_ACCOUNT_USERNAME),
                            @Parameter.Argument(
                                    name = AddGoogleAccountToOsParameter.ARGUMENT.PASSWORD,
                                    stringVar = GOOGLE_ACCOUNT_PASSWORD)})})
    public void testIsSignedInOnFakeOSandGoogleOS() {
        assertFalse("Should not be signed into app.",
                mAddFakeAccountToAppParameter.isSignedIn());
        assertTrue("Should be signed into OS with fake account.",
                mAddFakeAccountToOsParameter.isSignedIn());
        assertTrue("Should be signed in on OS with Google account.",
                mAddGoogleAccountToOsParameter.isSignedIn(GOOGLE_ACCOUNT_USERNAME));
    }

    /*
    @FlakyTest
    @EnormousTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @ParameterizedTest(parameters = {
            @Parameter(tag = AddFakeAccountToAppParameter.PARAMETER_TAG),
            @Parameter(tag = AddFakeAccountToOsParameter.PARAMETER_TAG),
            @Parameter(
                    tag = AddGoogleAccountToOsParameter.PARAMETER_TAG,
                    arguments = {
                            @Parameter.Argument(
                                    name = AddGoogleAccountToOsParameter.ARGUMENT.USERNAME,
                                    stringVar = GOOGLE_ACCOUNT_USERNAME),
                            @Parameter.Argument(
                                    name = AddGoogleAccountToOsParameter.ARGUMENT.PASSWORD,
                                    stringVar = GOOGLE_ACCOUNT_PASSWORD)})})
    */
    @DisabledTest(message = "crbug.com/524189")
    public void testIsSignedInOnAppAndFakeOSandGoogleOS() {
        assertTrue("Should be signed into app.",
                mAddFakeAccountToAppParameter.isSignedIn());
        assertTrue("Should be signed into OS with fake account.",
                mAddFakeAccountToOsParameter.isSignedIn());
        assertTrue("Should be signed in on OS with Google account.",
                mAddGoogleAccountToOsParameter.isSignedIn(GOOGLE_ACCOUNT_USERNAME));
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }
}
