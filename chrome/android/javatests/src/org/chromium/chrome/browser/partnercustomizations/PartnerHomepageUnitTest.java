// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import android.support.test.filters.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.test.partnercustomizations.TestPartnerBrowserCustomizationsDelayedProvider;
import org.chromium.chrome.test.partnercustomizations.TestPartnerBrowserCustomizationsProvider;

/**
 * Unit test suite for partner homepage.
 */
public class PartnerHomepageUnitTest extends BasePartnerBrowserCustomizationUnitTest {
    public static final String TAG = "PartnerHomepageUnitTest";

    private static final String TEST_CUSTOM_HOMEPAGE_URI = "http://chrome.com";

    private HomepageManager mHomepageManager;

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        mHomepageManager = HomepageManager.getInstance(getContext());
        assertNotNull(mHomepageManager);

        assertNotSame(TestPartnerBrowserCustomizationsProvider.HOMEPAGE_URI,
                TEST_CUSTOM_HOMEPAGE_URI);
    }

    /**
     * Everything is enabled for using partner homepage, except that there is no flag file.
     */
    @SmallTest
    @Feature({"Homepage"})
    public void testProviderNotFromSystemPackage() throws InterruptedException {
        mHomepageManager.setPrefHomepageEnabled(true);
        mHomepageManager.setPrefHomepageUseDefaultUri(true);
        mHomepageManager.setPrefHomepageCustomUri(TEST_CUSTOM_HOMEPAGE_URI);

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
        assertFalse(PartnerBrowserCustomizations.isHomepageProviderAvailableAndEnabled());
        assertNull(PartnerBrowserCustomizations.getHomePageUrl());
        assertFalse(HomepageManager.isHomepageEnabled(getContext()));
        assertFalse(HomepageManager.shouldShowHomepageSetting());
        assertNull(HomepageManager.getHomepageUri(getContext()));
    }

    /**
     * Everything is enabled for using partner homepage, except that there is no actual provider.
     */
    @SmallTest
    @Feature({"Homepage"})
    public void testNoProvider() throws InterruptedException {
        mHomepageManager.setPrefHomepageEnabled(true);
        mHomepageManager.setPrefHomepageUseDefaultUri(true);
        mHomepageManager.setPrefHomepageCustomUri(TEST_CUSTOM_HOMEPAGE_URI);

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
        assertFalse(PartnerBrowserCustomizations.isHomepageProviderAvailableAndEnabled());
        assertNull(PartnerBrowserCustomizations.getHomePageUrl());
        assertFalse(HomepageManager.isHomepageEnabled(getContext()));
        assertFalse(HomepageManager.shouldShowHomepageSetting());
        assertNull(HomepageManager.getHomepageUri(getContext()));
    }

    /**
     * Everything is enabled for using partner homepage, except that the homepage prefererence is
     * disabled.
     */
    @SmallTest
    @Feature({"Homepage"})
    @RetryOnFailure
    public void testHomepageDisabled() throws InterruptedException {
        mHomepageManager.setPrefHomepageEnabled(false);
        mHomepageManager.setPrefHomepageUseDefaultUri(true);
        mHomepageManager.setPrefHomepageCustomUri(TEST_CUSTOM_HOMEPAGE_URI);

        PartnerBrowserCustomizations.ignoreBrowserProviderSystemPackageCheckForTests(true);
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
        assertTrue(PartnerBrowserCustomizations.isHomepageProviderAvailableAndEnabled());
        assertEquals(TestPartnerBrowserCustomizationsProvider.HOMEPAGE_URI,
                PartnerBrowserCustomizations.getHomePageUrl());
        assertFalse(HomepageManager.isHomepageEnabled(getContext()));
        assertTrue(HomepageManager.shouldShowHomepageSetting());
        assertNull(HomepageManager.getHomepageUri(getContext()));
    }

    /**
     * Everything is enabled for using partner homepage, except that the preference is set to use
     * custom user-specified homepage.
     */
    @SmallTest
    @Feature({"Homepage"})
    @RetryOnFailure
    public void testCustomHomepage() throws InterruptedException {
        mHomepageManager.setPrefHomepageEnabled(true);
        mHomepageManager.setPrefHomepageUseDefaultUri(false);
        mHomepageManager.setPrefHomepageCustomUri(TEST_CUSTOM_HOMEPAGE_URI);

        PartnerBrowserCustomizations.ignoreBrowserProviderSystemPackageCheckForTests(true);
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
        assertTrue(PartnerBrowserCustomizations.isHomepageProviderAvailableAndEnabled());
        assertEquals(TestPartnerBrowserCustomizationsProvider.HOMEPAGE_URI,
                PartnerBrowserCustomizations.getHomePageUrl());
        assertTrue(HomepageManager.isHomepageEnabled(getContext()));
        assertTrue(HomepageManager.shouldShowHomepageSetting());
        assertEquals(TEST_CUSTOM_HOMEPAGE_URI, HomepageManager.getHomepageUri(getContext()));
    }

    /**
     * Everything is enabled for using partner homepage, but the homepage provider query takes
     * longer than the timeout we specify.
     */
    @SmallTest
    @Feature({"Homepage"})
    public void testHomepageProviderTimeout() throws InterruptedException {
        mHomepageManager.setPrefHomepageEnabled(true);
        mHomepageManager.setPrefHomepageUseDefaultUri(true);
        mHomepageManager.setPrefHomepageCustomUri(TEST_CUSTOM_HOMEPAGE_URI);

        PartnerBrowserCustomizations.ignoreBrowserProviderSystemPackageCheckForTests(true);
        PartnerBrowserCustomizations.setProviderAuthorityForTests(
                PARTNER_BROWSER_CUSTOMIZATIONS_DELAYED_PROVIDER);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PartnerBrowserCustomizations.initializeAsync(getContext(), 500);
            }
        });
        PartnerBrowserCustomizations.setOnInitializeAsyncFinished(mCallback, 300);

        mCallbackLock.acquire();

        assertFalse(PartnerBrowserCustomizations.isInitialized());
        assertFalse(PartnerBrowserCustomizations.isHomepageProviderAvailableAndEnabled());
        assertNull(PartnerBrowserCustomizations.getHomePageUrl());
        assertFalse(HomepageManager.isHomepageEnabled(getContext()));
        assertFalse(HomepageManager.shouldShowHomepageSetting());
        assertNull(HomepageManager.getHomepageUri(getContext()));

        PartnerBrowserCustomizations.setOnInitializeAsyncFinished(mCallback, 2000);

        mCallbackLock.acquire();

        assertTrue(PartnerBrowserCustomizations.isInitialized());
        assertFalse(PartnerBrowserCustomizations.isHomepageProviderAvailableAndEnabled());
        assertNull(PartnerBrowserCustomizations.getHomePageUrl());
        assertFalse(HomepageManager.isHomepageEnabled(getContext()));
        assertFalse(HomepageManager.shouldShowHomepageSetting());
        assertNull(HomepageManager.getHomepageUri(getContext()));
    }

    /**
     * Everything is enabled for using partner homepage. The homepage provider query does not take
     * longer than the timeout we specify, but longer than the first async task wait timeout. This
     * scenario covers that the homepage provider is not ready at the cold startup initial homepage
     * tab, but be ready later than that.
     */
    @SmallTest
    @Feature({"Homepage"})
    public void testHomepageProviderDelayed() throws InterruptedException {
        mHomepageManager.setPrefHomepageEnabled(true);
        mHomepageManager.setPrefHomepageUseDefaultUri(true);
        mHomepageManager.setPrefHomepageCustomUri(TEST_CUSTOM_HOMEPAGE_URI);

        PartnerBrowserCustomizations.ignoreBrowserProviderSystemPackageCheckForTests(true);
        PartnerBrowserCustomizations.setProviderAuthorityForTests(
                PARTNER_BROWSER_CUSTOMIZATIONS_DELAYED_PROVIDER);
        setDelayProviderUriPathForDelay(PartnerBrowserCustomizations.PARTNER_HOMEPAGE_PATH);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PartnerBrowserCustomizations.initializeAsync(getContext(), 2000);
            }
        });
        PartnerBrowserCustomizations.setOnInitializeAsyncFinished(mCallback, 300);

        mCallbackLock.acquire();

        assertFalse(PartnerBrowserCustomizations.isInitialized());
        assertFalse(PartnerBrowserCustomizations.isHomepageProviderAvailableAndEnabled());
        assertNull(PartnerBrowserCustomizations.getHomePageUrl());
        assertFalse(HomepageManager.isHomepageEnabled(getContext()));
        assertFalse(HomepageManager.shouldShowHomepageSetting());
        assertNull(HomepageManager.getHomepageUri(getContext()));

        PartnerBrowserCustomizations.setOnInitializeAsyncFinished(mCallback, 3000);

        mCallbackLock.acquire();

        assertTrue(PartnerBrowserCustomizations.isInitialized());
        assertTrue(PartnerBrowserCustomizations.isHomepageProviderAvailableAndEnabled());
        assertEquals(TestPartnerBrowserCustomizationsDelayedProvider.HOMEPAGE_URI,
                PartnerBrowserCustomizations.getHomePageUrl());
        assertTrue(HomepageManager.isHomepageEnabled(getContext()));
        assertTrue(HomepageManager.shouldShowHomepageSetting());
        assertEquals(TestPartnerBrowserCustomizationsDelayedProvider.HOMEPAGE_URI,
                HomepageManager.getHomepageUri(getContext()));
    }

    /**
     * Everything is enabled for using partner homepage. It should be able to successfully retrieve
     * homepage URI from the provider.
     */
    @SmallTest
    @Feature({"Homepage"})
    @RetryOnFailure
    public void testReadHomepageProvider() throws InterruptedException {
        mHomepageManager.setPrefHomepageEnabled(true);
        mHomepageManager.setPrefHomepageUseDefaultUri(true);
        mHomepageManager.setPrefHomepageCustomUri(TEST_CUSTOM_HOMEPAGE_URI);

        PartnerBrowserCustomizations.ignoreBrowserProviderSystemPackageCheckForTests(true);
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
        assertTrue(PartnerBrowserCustomizations.isHomepageProviderAvailableAndEnabled());
        assertEquals(TestPartnerBrowserCustomizationsProvider.HOMEPAGE_URI,
                PartnerBrowserCustomizations.getHomePageUrl());
        assertTrue(HomepageManager.isHomepageEnabled(getContext()));
        assertTrue(HomepageManager.shouldShowHomepageSetting());
        assertEquals(TestPartnerBrowserCustomizationsProvider.HOMEPAGE_URI,
                HomepageManager.getHomepageUri(getContext()));
    }
}
