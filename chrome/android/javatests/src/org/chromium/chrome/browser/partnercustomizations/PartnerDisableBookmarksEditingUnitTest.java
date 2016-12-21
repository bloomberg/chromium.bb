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
 * Unit tests for the partner disabling bookmarks editing functionality.
 */
public class PartnerDisableBookmarksEditingUnitTest
        extends BasePartnerBrowserCustomizationUnitTest {

    private void setBookmarksEditingDisabled(boolean disabled) {
        Uri uri = PartnerBrowserCustomizations.buildQueryUri(
                PartnerBrowserCustomizations.PARTNER_DISABLE_BOOKMARKS_EDITING_PATH);
        Bundle bundle = new Bundle();
        bundle.putBoolean(
                TestPartnerBrowserCustomizationsProvider.BOOKMARKS_EDITING_DISABLED_KEY, disabled);
        getContext().getContentResolver().call(uri, "setBookmarksEditingDisabled", null, bundle);
    }

    @SmallTest
    @Feature({"PartnerBookmarksEditing"})
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
        assertFalse(PartnerBrowserCustomizations.isBookmarksEditingDisabled());
    }

    @SmallTest
    @Feature({"PartnerBookmarksEditing"})
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
        assertFalse(PartnerBrowserCustomizations.isBookmarksEditingDisabled());
    }

    @SmallTest
    @Feature({"PartnerBookmarksEditing"})
    public void testBookmarksEditingNotDisabled() throws InterruptedException {
        PartnerBrowserCustomizations.ignoreBrowserProviderSystemPackageCheckForTests(true);
        PartnerBrowserCustomizations.setProviderAuthorityForTests(
                PARTNER_BROWSER_CUSTOMIZATIONS_PROVIDER);
        setBookmarksEditingDisabled(false);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PartnerBrowserCustomizations.initializeAsync(getContext(), DEFAULT_TIMEOUT_MS);
            }
        });
        PartnerBrowserCustomizations.setOnInitializeAsyncFinished(mCallback, DEFAULT_TIMEOUT_MS);

        mCallbackLock.acquire();

        assertTrue(PartnerBrowserCustomizations.isInitialized());
        assertFalse(PartnerBrowserCustomizations.isBookmarksEditingDisabled());
    }

    @SmallTest
    @Feature({"PartnerBookmarksEditing"})
    public void testBookmarksEditingDisabled() throws InterruptedException {
        PartnerBrowserCustomizations.ignoreBrowserProviderSystemPackageCheckForTests(true);
        PartnerBrowserCustomizations.setProviderAuthorityForTests(
                PARTNER_BROWSER_CUSTOMIZATIONS_PROVIDER);
        setBookmarksEditingDisabled(true);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PartnerBrowserCustomizations.initializeAsync(getContext(), DEFAULT_TIMEOUT_MS);
            }
        });
        PartnerBrowserCustomizations.setOnInitializeAsyncFinished(mCallback, DEFAULT_TIMEOUT_MS);

        mCallbackLock.acquire();

        assertTrue(PartnerBrowserCustomizations.isInitialized());
        assertTrue(PartnerBrowserCustomizations.isBookmarksEditingDisabled());
    }

    @SmallTest
    @Feature({"PartnerBookmarksEditing"})
    public void testBookmarksEditingProviderDelayed() throws InterruptedException {
        PartnerBrowserCustomizations.ignoreBrowserProviderSystemPackageCheckForTests(true);
        PartnerBrowserCustomizations.setProviderAuthorityForTests(
                PARTNER_BROWSER_CUSTOMIZATIONS_DELAYED_PROVIDER);
        setDelayProviderUriPathForDelay(
                PartnerBrowserCustomizations.PARTNER_DISABLE_BOOKMARKS_EDITING_PATH);
        setBookmarksEditingDisabled(true);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PartnerBrowserCustomizations.initializeAsync(getContext(), 2000);
            }
        });
        PartnerBrowserCustomizations.setOnInitializeAsyncFinished(mCallback, 300);

        mCallbackLock.acquire();

        assertFalse(PartnerBrowserCustomizations.isInitialized());
        assertFalse(PartnerBrowserCustomizations.isBookmarksEditingDisabled());

        PartnerBrowserCustomizations.setOnInitializeAsyncFinished(mCallback, 3000);

        mCallbackLock.acquire();

        assertTrue(PartnerBrowserCustomizations.isInitialized());
        assertTrue(PartnerBrowserCustomizations.isBookmarksEditingDisabled());
    }
}
