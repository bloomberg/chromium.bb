// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import android.net.Uri;
import android.os.Bundle;
import android.support.test.filters.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.partnercustomizations.TestPartnerBrowserCustomizationsProvider;

/**
 * Unit tests for the partner disabling incognito mode functionality.
 */
public class PartnerDisableIncognitoModeUnitTest extends BasePartnerBrowserCustomizationUnitTest {

    private void setParentalControlsEnabled(boolean enabled) {
        Uri uri = PartnerBrowserCustomizations.buildQueryUri(
                PartnerBrowserCustomizations.PARTNER_DISABLE_INCOGNITO_MODE_PATH);
        Bundle bundle = new Bundle();
        bundle.putBoolean(
                TestPartnerBrowserCustomizationsProvider.INCOGNITO_MODE_DISABLED_KEY, enabled);
        getContext().getContentResolver().call(uri, "setIncognitoModeDisabled", null, bundle);
    }

    @SmallTest
    @Feature({"ParentalControls"})
    public void testProviderNotFromSystemPackage() throws InterruptedException {
        // Note that unlike other tests in this file, we do not call
        // PartnerBrowserCustomizations.ignoreBrowserProviderSystemPackageCheckForTests(true);
        // here to test if Chrome ignores a customizations provider that is not from
        // a system package.
        PartnerBrowserCustomizations.setProviderAuthorityForTests(
                PARTNER_BROWSER_CUSTOMIZATIONS_PROVIDER);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PartnerBrowserCustomizations.initializeAsync(getContext(), DEFAULT_TIMEOUT_MS);
            }
        });
        PartnerBrowserCustomizations.setOnInitializeAsyncFinished(mCallback, DEFAULT_TIMEOUT_MS);

        mCallbackLock.acquire();

        assertTrue(PartnerBrowserCustomizations.isInitialized());
        assertFalse(PartnerBrowserCustomizations.isIncognitoDisabled());
    }

    @SmallTest
    @Feature({"ParentalControls"})
    public void testNoProvider() throws InterruptedException {
        PartnerBrowserCustomizations.ignoreBrowserProviderSystemPackageCheckForTests(true);
        PartnerBrowserCustomizations.setProviderAuthorityForTests(
                PARTNER_BROWSER_CUSTOMIZATIONS_NO_PROVIDER);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PartnerBrowserCustomizations.initializeAsync(getContext(), DEFAULT_TIMEOUT_MS);
            }
        });
        PartnerBrowserCustomizations.setOnInitializeAsyncFinished(mCallback, DEFAULT_TIMEOUT_MS);
        mCallbackLock.acquire();

        assertTrue(PartnerBrowserCustomizations.isInitialized());
        assertFalse(PartnerBrowserCustomizations.isIncognitoDisabled());
    }

    @SmallTest
    @Feature({"ParentalControls"})
    public void testParentalControlsNotEnabled() throws InterruptedException {
        PartnerBrowserCustomizations.ignoreBrowserProviderSystemPackageCheckForTests(true);
        PartnerBrowserCustomizations.setProviderAuthorityForTests(
                PARTNER_BROWSER_CUSTOMIZATIONS_PROVIDER);
        setParentalControlsEnabled(false);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PartnerBrowserCustomizations.initializeAsync(getContext(), DEFAULT_TIMEOUT_MS);
            }
        });
        PartnerBrowserCustomizations.setOnInitializeAsyncFinished(mCallback, DEFAULT_TIMEOUT_MS);

        mCallbackLock.acquire();

        assertTrue(PartnerBrowserCustomizations.isInitialized());
        assertFalse(PartnerBrowserCustomizations.isIncognitoDisabled());
    }

    @SmallTest
    @Feature({"ParentalControls"})
    public void testParentalControlsEnabled() throws InterruptedException {
        PartnerBrowserCustomizations.ignoreBrowserProviderSystemPackageCheckForTests(true);
        PartnerBrowserCustomizations.setProviderAuthorityForTests(
                PARTNER_BROWSER_CUSTOMIZATIONS_PROVIDER);
        setParentalControlsEnabled(true);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PartnerBrowserCustomizations.initializeAsync(getContext(), DEFAULT_TIMEOUT_MS);
            }
        });
        PartnerBrowserCustomizations.setOnInitializeAsyncFinished(mCallback, DEFAULT_TIMEOUT_MS);

        mCallbackLock.acquire();

        assertTrue(PartnerBrowserCustomizations.isInitialized());
        assertTrue(PartnerBrowserCustomizations.isIncognitoDisabled());
    }

    @SmallTest
    @Feature({"ParentalControls"})
    public void testParentalControlsProviderDelayed() throws InterruptedException {
        PartnerBrowserCustomizations.ignoreBrowserProviderSystemPackageCheckForTests(true);
        PartnerBrowserCustomizations.setProviderAuthorityForTests(
                PARTNER_BROWSER_CUSTOMIZATIONS_DELAYED_PROVIDER);
        setDelayProviderUriPathForDelay(
                PartnerBrowserCustomizations.PARTNER_DISABLE_INCOGNITO_MODE_PATH);
        setParentalControlsEnabled(true);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PartnerBrowserCustomizations.initializeAsync(getContext(), 2000);
            }
        });
        PartnerBrowserCustomizations.setOnInitializeAsyncFinished(mCallback, 300);

        mCallbackLock.acquire();

        assertFalse(PartnerBrowserCustomizations.isInitialized());
        assertFalse(PartnerBrowserCustomizations.isIncognitoDisabled());

        PartnerBrowserCustomizations.setOnInitializeAsyncFinished(mCallback, 3000);

        mCallbackLock.acquire();

        assertTrue(PartnerBrowserCustomizations.isInitialized());
        assertTrue(PartnerBrowserCustomizations.isIncognitoDisabled());
    }
}
