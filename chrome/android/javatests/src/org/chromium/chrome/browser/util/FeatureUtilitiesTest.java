// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import android.accounts.Account;
import android.accounts.AuthenticatorDescription;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.speech.RecognizerIntent;
import android.test.InstrumentationTestCase;
import android.test.mock.MockContext;
import android.test.mock.MockPackageManager;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.sync.signin.AccountManagerHelper;
import org.chromium.sync.test.util.MockAccountManager;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;

/**
 * Unit Test for FeatureUtilities.
 */
public class FeatureUtilitiesTest extends InstrumentationTestCase {

    private IntentTestMockContext mContextWithSpeech;
    private IntentTestMockContext mContextWithoutSpeech;
    private MockAuthenticationAccountManager mAccountManager;
    private AdvancedMockContext mAccountTestingContext;
    private Account mTestAccount;

    public FeatureUtilitiesTest() {
        mContextWithSpeech = new IntentTestMockContext(
                RecognizerIntent.ACTION_RECOGNIZE_SPEECH);

        mContextWithoutSpeech = new IntentTestMockContext(
                RecognizerIntent.ACTION_WEB_SEARCH);

        mTestAccount = AccountManagerHelper.createAccountFromName("Dummy");
    }

    @Override
    public void setUp() {
        // GetInstrumentation().getTargetContext() cannot be called in
        // constructor due to external dependencies.
        mAccountTestingContext = new AdvancedMockContext(
                getInstrumentation().getTargetContext());
    }

    private static class IntentTestPackageManager extends MockPackageManager {

        private final String mAction;

        public IntentTestPackageManager(String recognizesAction) {
            super();
            mAction = recognizesAction;
        }

        @Override
        public List<ResolveInfo>queryIntentActivities(Intent intent, int flags) {
            List<ResolveInfo>resolveInfoList = new ArrayList<ResolveInfo>();

            if (intent.getAction().equals(mAction)) {
                // Add an entry to the returned list as the action
                // being queried exists.
                ResolveInfo resolveInfo = new ResolveInfo();
                resolveInfoList.add(resolveInfo);
            }

            return resolveInfoList;
        }
    }

    private static class IntentTestMockContext extends MockContext {

        private final String mAction;

        public IntentTestMockContext(String recognizesAction) {
            super();
            mAction = recognizesAction;
        }

        @Override
        public IntentTestPackageManager getPackageManager() {
            return new IntentTestPackageManager(mAction);
        }
    }

    private static class MockAuthenticationAccountManager extends MockAccountManager {

       private final String mAccountType;

       public MockAuthenticationAccountManager(Context localContext,
                                               Context testContext,
                                               String accountType,
                                               Account... accounts) {
           super(localContext, testContext, accounts);
           mAccountType = accountType;
       }

       @Override
       public Account[] getAccountsByType(String accountType) {
           if (accountType.equals(mAccountType)) {
               // Calling function uses length of array to determine if
               // accounts of the requested type are available, so return
               // a non-empty array to indicate the account type is correct.
               return new Account[] {
                       AccountManagerHelper.createAccountFromName("Dummy")
               };
           }

           return new Account[0];
       }

       @Override
       public AuthenticatorDescription[] getAuthenticatorTypes() {
           AuthenticatorDescription googleAuthenticator =
                   new AuthenticatorDescription(mAccountType, "p1", 0, 0, 0, 0);

           return new AuthenticatorDescription[] { googleAuthenticator };
       }
    }

    private static boolean isRecognitionIntentPresent(
        final IntentTestMockContext context,
        final boolean useCachedResult) {
        // Context can only be queried on a UI Thread.
        return ThreadUtils.runOnUiThreadBlockingNoException(
            new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return FeatureUtilities.isRecognitionIntentPresent(
                        context,
                        useCachedResult);
            }
        });
    }

    private void setUpAccount(final String accountType) {
        mAccountManager = new MockAuthenticationAccountManager(
                mAccountTestingContext,
                getInstrumentation().getContext(),
                accountType,
                mTestAccount);

        AccountManagerHelper.overrideAccountManagerHelperForTests(
                mAccountTestingContext,
                mAccountManager);
    }

    @SmallTest
    @Feature({"FeatureUtilities", "Speech"})
    public void testSpeechFeatureAvailable() {

        final boolean doNotUseCachedResult = false;
        final boolean recognizesSpeech = isRecognitionIntentPresent(
                mContextWithSpeech,
                doNotUseCachedResult);

        assertTrue(recognizesSpeech);
    }

    @SmallTest
    @Feature({"FeatureUtilities", "Speech"})
    public void testSpeechFeatureUnavailable() {

        final boolean doNotUseCachedResult = false;
        final boolean recognizesSpeech = isRecognitionIntentPresent(
                mContextWithoutSpeech,
                doNotUseCachedResult);

        assertFalse(recognizesSpeech);
    }

    @SmallTest
    @Feature({"FeatureUtilities", "Speech"})
    public void testCachedSpeechFeatureAvailability() {

        // Initial call will cache the fact that speech is recognized.
        final boolean doNotUseCachedResult = false;
        isRecognitionIntentPresent(
                mContextWithSpeech,
                doNotUseCachedResult);

        // Pass a context that does not recognize speech, but use cached result
        // which does recognize speech.
        final boolean useCachedResult = true;
        final boolean recognizesSpeech = isRecognitionIntentPresent(
                mContextWithoutSpeech,
                useCachedResult);

        // Check that we still recognize speech as we're using cached result.
        assertTrue(recognizesSpeech);

        // Check if we can turn cached result off again.
        final boolean RecognizesSpeechUncached = isRecognitionIntentPresent(
                mContextWithoutSpeech,
                doNotUseCachedResult);

        assertFalse(RecognizesSpeechUncached);
    }

    @SmallTest
    @Feature({"FeatureUtilities", "GoogleAccounts"})
    public void testHasGoogleAccountCorrectlyDetected() {

        // Set up an account manager mock that returns Google account types
        // when queried.
        setUpAccount(AccountManagerHelper.GOOGLE_ACCOUNT_TYPE);

        boolean hasAccounts = FeatureUtilities.hasGoogleAccounts(
                mAccountTestingContext);

        assertTrue(hasAccounts);

        boolean hasAuthenticator = FeatureUtilities.hasGoogleAccountAuthenticator(
                mAccountTestingContext);

        assertTrue(hasAuthenticator);
    }

    @SmallTest
    @Feature({"FeatureUtilities", "GoogleAccounts"})
    public void testHasNoGoogleAccountCorrectlyDetected() {

        // Set up an account manager mock that doesn't return Google account
        // types when queried.
        setUpAccount("Not A Google Account");

        boolean hasAccounts = FeatureUtilities.hasGoogleAccounts(
                mAccountTestingContext);

        assertFalse(hasAccounts);

        boolean hasAuthenticator = FeatureUtilities.hasGoogleAccountAuthenticator(
                mAccountTestingContext);

        assertFalse(hasAuthenticator);
    }
}
