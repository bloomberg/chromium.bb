// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import android.content.Context;
import android.content.ContextWrapper;
import android.net.Uri;
import android.test.AndroidTestCase;

import org.chromium.base.CommandLine;
import org.chromium.chrome.test.partnercustomizations.TestPartnerBrowserCustomizationsDelayedProvider;
import org.chromium.chrome.test.partnercustomizations.TestPartnerBrowserCustomizationsProvider;

import java.util.concurrent.Semaphore;

/**
 * Basic shared functionality for partner customization unit tests.
 */
public class BasePartnerBrowserCustomizationUnitTest extends AndroidTestCase {
    protected static final String PARTNER_BROWSER_CUSTOMIZATIONS_PROVIDER =
            TestPartnerBrowserCustomizationsProvider.class.getName();
    protected static final String PARTNER_BROWSER_CUSTOMIZATIONS_NO_PROVIDER =
            TestPartnerBrowserCustomizationsProvider.class.getName() + "INVALID";
    protected static final String PARTNER_BROWSER_CUSTOMIZATIONS_DELAYED_PROVIDER =
            TestPartnerBrowserCustomizationsDelayedProvider.class.getName();
    protected static final long DEFAULT_TIMEOUT_MS = 500;

    protected final Runnable mCallback = new Runnable() {
        @Override
        public void run() {
            mCallbackLock.release();
        }
    };
    protected final Semaphore mCallbackLock = new Semaphore(0);

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
        getContext().getContentResolver().call(uri, "setUriPathToDelay", uriPath, null);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        CommandLine.init(null);
    }

    @Override
    public Context getContext() {
        ContextWrapper context = new ContextWrapper(super.getContext()) {
            @Override
            public Context getApplicationContext() {
                return getBaseContext();
            }
        };
        return context;
    }
}
