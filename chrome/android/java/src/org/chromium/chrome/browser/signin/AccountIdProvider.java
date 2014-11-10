// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.content.Context;

import org.chromium.base.ThreadUtils;

/**
 * Returns a stable id that can be used to identify a Google Account.  This
 * id does not change if the email address associated to the account changes,
 * nor does it change depending on whether the email has dots or varying
 * capitalization.
 */
public abstract class AccountIdProvider {
    private static AccountIdProvider sProvider;

    /**
     * Returns a stable id for the account associated with the given email address.
     * If an account wuth the given email address is not installed on the device
     * then null is returned.
     *
     * This method will throw IllegalStateException if called on the main thread.
     *
     * @param accountName The email address of a Google account.
     */
    public abstract String getAccountId(Context ctx, String accountName);

    /**
     * Sets the global account Id provider.  Trying to set a new provider when
     * one is already set throws IllegalArgumentException.  The provider can only
     * be set on the UI thread.
     */
    public static void setInstance(AccountIdProvider provider) {
        ThreadUtils.assertOnUiThread();
        if (sProvider != null) {
            throw new IllegalArgumentException("Provider already set");
        }
        sProvider = provider;
    }

    /**
     * Gets the global account Id provider.
     */
    public static AccountIdProvider getInstance() {
        ThreadUtils.assertOnUiThread();
        return sProvider;
    }
}

