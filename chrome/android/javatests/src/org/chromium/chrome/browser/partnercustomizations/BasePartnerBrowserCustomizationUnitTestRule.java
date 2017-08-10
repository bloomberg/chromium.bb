// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import android.content.Context;
import android.content.ContextWrapper;
import android.net.Uri;
import android.support.test.InstrumentationRegistry;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.CommandLine;
import org.chromium.chrome.test.partnercustomizations.TestPartnerBrowserCustomizationsDelayedProvider;
import org.chromium.chrome.test.partnercustomizations.TestPartnerBrowserCustomizationsProvider;

import java.util.concurrent.Semaphore;

/**
 * Basic shared functionality for partner customization unit tests.
 */
public class BasePartnerBrowserCustomizationUnitTestRule implements TestRule {
    public static final String PARTNER_BROWSER_CUSTOMIZATIONS_PROVIDER =
            TestPartnerBrowserCustomizationsProvider.class.getName();
    public static final String PARTNER_BROWSER_CUSTOMIZATIONS_NO_PROVIDER =
            TestPartnerBrowserCustomizationsProvider.class.getName() + "INVALID";
    public static final String PARTNER_BROWSER_CUSTOMIZATIONS_DELAYED_PROVIDER =
            TestPartnerBrowserCustomizationsDelayedProvider.class.getName();
    public static final long DEFAULT_TIMEOUT_MS = 500;

    private final Runnable mCallback = new Runnable() {
        @Override
        public void run() {
            mCallbackLock.release();
        }
    };
    private final Semaphore mCallbackLock = new Semaphore(0);

    /**
     * Specifies the URI path that should be delayed when querying the delayed provider.
     * <p>
     * This will override the provider authority in the PartnerBrowserCustomizations, so be
     * sure to reset it if you are not using the delayed provider.
     *
     * @param uriPath The path to be delayed.
     */
    public void setDelayProviderUriPathForDelay(String uriPath) {
        PartnerBrowserCustomizations.setProviderAuthorityForTests(
                PARTNER_BROWSER_CUSTOMIZATIONS_DELAYED_PROVIDER);
        Uri uri = PartnerBrowserCustomizations.buildQueryUri(uriPath);
        getContextWrapper().getContentResolver().call(uri, "setUriPathToDelay", uriPath, null);
    }

    @Override
    public Statement apply(final Statement base, Description desc) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                CommandLine.init(null);
                base.evaluate();
            }
        };
    }

    public Context getContextWrapper() {
        ContextWrapper context = new ContextWrapper(InstrumentationRegistry.getContext()) {
            @Override
            public Context getApplicationContext() {
                return getBaseContext();
            }
        };
        return context;
    }

    public Runnable getCallback() {
        return mCallback;
    }

    public Semaphore getCallbackLock() {
        return mCallbackLock;
    }
}
