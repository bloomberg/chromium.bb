// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.parameters;

import android.app.Instrumentation;

import org.chromium.base.test.util.parameter.BaseParameter;
import org.chromium.base.test.util.parameter.Parameter;
import org.chromium.chrome.test.util.ChromeSigninUtils;

/**
 * Adds a Google account to OS when this parameter is used.
 */
public class AddGoogleAccountToOsParameter extends BaseParameter {
    /** Adds a Google account to OS to run test as signed into the OS. */
    public static final String PARAMETER_TAG = "add-google-account-to-os-parameter";

    /** The {@Parameter.Argument#name()} value. */
    public static final class ARGUMENT {
        public static final String USERNAME = "username";
        public static final String PASSWORD = "password";
        public static final String TYPE = "type";
        private static final class DEFAULT {
            private static final String TYPE = "mail";
        }
    }

    private ChromeSigninUtils mSigninUtil;

    public AddGoogleAccountToOsParameter(Parameter.Reader parameterReader,
            Instrumentation instrumentation) {
        super(PARAMETER_TAG, parameterReader);
        mSigninUtil = new ChromeSigninUtils(instrumentation);
    }

    @Override
    public void setUp() {
        String username = getStringArgument(ARGUMENT.USERNAME);
        String password = getStringArgument(ARGUMENT.PASSWORD);
        String type = getStringArgument(ARGUMENT.TYPE, ARGUMENT.DEFAULT.TYPE);

        mSigninUtil.removeAllGoogleAccountsFromOs();
        mSigninUtil.addGoogleAccountToOs(username, password, type);
    }

    @Override
    public void tearDown() {
        mSigninUtil.removeAllGoogleAccountsFromOs();
    }

    public boolean isSignedIn(String username) {
        return mSigninUtil.isExistingGoogleAccountOnOs(username);
    }
}
