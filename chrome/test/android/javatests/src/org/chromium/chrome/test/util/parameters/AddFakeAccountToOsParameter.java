// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.parameters;

import android.app.Instrumentation;

import org.chromium.base.test.util.parameter.BaseParameter;
import org.chromium.base.test.util.parameter.Parameter;
import org.chromium.chrome.test.util.ChromeSigninUtils;

/**
 * Adds a fake account to OS when this parameter is used.
 */
public class AddFakeAccountToOsParameter extends BaseParameter {
    /** Adds a fake test account to OS to run test as signed into the OS. */
    public static final String PARAMETER_TAG = "add-fake-account-to-os-parameter";

    /** The {@Parameter.Argument#name()} value. */
    public static final class ARGUMENT {
        public static final String USERNAME = "username";
        public static final String PASSWORD = "password";
        private static final class DEFAULT {
            private static final String USERNAME = "test@google.com";
            private static final String PASSWORD = "$3cr3t";
        }
    }

    private ChromeSigninUtils mSigninUtil;

    public AddFakeAccountToOsParameter(Parameter.Reader parameterReader,
            Instrumentation instrumentation) {
        super(PARAMETER_TAG, parameterReader);
        mSigninUtil = new ChromeSigninUtils(instrumentation);
    }

    @Override
    public void setUp() {
        String username = getStringArgument(ARGUMENT.USERNAME, ARGUMENT.DEFAULT.USERNAME);
        String password = getStringArgument(ARGUMENT.PASSWORD, ARGUMENT.DEFAULT.PASSWORD);

        mSigninUtil.removeAllFakeAccountsFromOs();
        mSigninUtil.addFakeAccountToOs(username, password);
    }

    @Override
    public void tearDown() {
        mSigninUtil.removeAllFakeAccountsFromOs();
    }

    public boolean isSignedIn() {
        return isSignedIn(ARGUMENT.DEFAULT.USERNAME);
    }

    public boolean isSignedIn(String username) {
        return mSigninUtil.isExistingFakeAccountOnOs(username);
    }
}
