// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.parameters;

import android.app.Instrumentation;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.parameter.BaseParameter;
import org.chromium.base.test.util.parameter.Parameter;
import org.chromium.chrome.browser.init.ProcessInitializationHandler;
import org.chromium.chrome.test.util.ChromeSigninUtils;
import org.chromium.components.signin.ChromeSigninController;

/**
 * Adds a fake account to app when this parameter is used.
 */
public class AddFakeAccountToAppParameter extends BaseParameter {
    /** Adds a fake test account to app to run test as signed into the app. */
    public static final String PARAMETER_TAG = "add-fake-account-to-app-parameter";

    /** The {@Parameter.Argument#name()} value. */
    public static final class ARGUMENT {
        public static final String USERNAME = "username";
        private static final class DEFAULT {
            private static final String USERNAME = "test@google.com";
        }
    }

    private ChromeSigninController mSigninController;
    private ChromeSigninUtils mSigninUtil;

    public AddFakeAccountToAppParameter(Parameter.Reader parameterReader,
            Instrumentation instrumentation) {
        super(PARAMETER_TAG, parameterReader);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ProcessInitializationHandler.getInstance().initializePreNative();
            }
        });
        mSigninController = ChromeSigninController.get(instrumentation.getTargetContext());
        mSigninUtil = new ChromeSigninUtils(instrumentation);
    }

    @Override
    public void setUp() {
        String username = getStringArgument(ARGUMENT.USERNAME, ARGUMENT.DEFAULT.USERNAME);

        mSigninController.setSignedInAccountName(null);
        mSigninUtil.addAccountToApp(username);
    }

    @Override
    public void tearDown() {
        mSigninController.setSignedInAccountName(null);
    }

    /**
     * Only affects Java tests.
     *
     * @return {@code true} if an account is on the app, {@code false} otherwise.
     */
    public boolean isSignedIn() {
        return mSigninController.isSignedIn();
    }
}
