// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.net.Uri;
import android.support.customtabs.CustomTabsService;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.chrome.browser.browserservices.OriginVerifier.OriginVerificationListener;

import java.util.concurrent.Callable;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/** Tests for OriginVerifier. */
@RunWith(BaseJUnit4ClassRunner.class)
public class OriginVerifierTest {
    private static final long TIMEOUT_MS = 1000;
    private static final byte[] BYTE_ARRAY = new byte[] {(byte) 0xaa, (byte) 0xbb, (byte) 0xcc,
            (byte) 0x10, (byte) 0x20, (byte) 0x30, (byte) 0x01, (byte) 0x02};
    private static final String STRING_ARRAY = "AA:BB:CC:10:20:30:01:02";

    private static final String PACKAGE_NAME = "org.chromium.chrome";
    private static final String SHA_256_FINGERPRINT =
            "32:A2:FC:74:D7:31:10:58:59:E5:A8:5D:F1:6D:95:F1:02:D8:5B"
            + ":22:09:9B:80:64:C5:D8:91:5C:61:DA:D1:E0";
    private static final Uri TEST_HTTPS_ORIGIN_1 = Uri.parse("https://www.example.com");
    private static final Uri TEST_HTTPS_ORIGIN_2 = Uri.parse("https://www.android.com");
    private static final Uri TEST_HTTP_ORIGIN = Uri.parse("http://www.android.com");

    private class TestOriginVerificationListener implements OriginVerificationListener {
        @Override
        public void onOriginVerified(String packageName, Uri origin, boolean verified) {
            mLastPackageName = packageName;
            mLastOrigin = origin;
            mLastVerified = verified;
            mVerificationResultSemaphore.release();
        }
    }

    private Semaphore mVerificationResultSemaphore;
    private OriginVerifier mUseAsOriginVerifier;
    private OriginVerifier mHandleAllUrlsVerifier;
    private volatile String mLastPackageName;
    private volatile Uri mLastOrigin;
    private volatile boolean mLastVerified;

    @Before
    public void setUp() throws Exception {
        mHandleAllUrlsVerifier = new OriginVerifier(new TestOriginVerificationListener(),
                PACKAGE_NAME, CustomTabsService.RELATION_HANDLE_ALL_URLS);
        mUseAsOriginVerifier = new OriginVerifier(new TestOriginVerificationListener(),
                PACKAGE_NAME, CustomTabsService.RELATION_USE_AS_ORIGIN);
        mVerificationResultSemaphore = new Semaphore(0);
    }

    @Test
    @SmallTest
    public void testSHA256CertificateChecks() {
        Assert.assertEquals(OriginVerifier.byteArrayToHexString(BYTE_ARRAY), STRING_ARRAY);
        Assert.assertEquals(OriginVerifier.getCertificateSHA256FingerprintForPackage(PACKAGE_NAME),
                SHA_256_FINGERPRINT);
    }

    @Test
    @SmallTest
    public void testOnlyHttpsAllowed() throws InterruptedException {
        ThreadUtils.postOnUiThread(() -> mHandleAllUrlsVerifier.start(Uri.parse("LOL")));
        Assert.assertTrue(
                mVerificationResultSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertFalse(mLastVerified);
        ThreadUtils.postOnUiThread(() -> mHandleAllUrlsVerifier.start(TEST_HTTP_ORIGIN));
        Assert.assertTrue(
                mVerificationResultSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertFalse(mLastVerified);
    }

    @Test
    @SmallTest
    public void testMultipleRelationships() throws Exception {
        ThreadUtils.postOnUiThread(
                ()
                        -> OriginVerifier.addVerifiedOriginForPackage(PACKAGE_NAME,
                                TEST_HTTPS_ORIGIN_1, CustomTabsService.RELATION_USE_AS_ORIGIN));
        ThreadUtils.postOnUiThread(() -> mUseAsOriginVerifier.start(TEST_HTTPS_ORIGIN_1));
        Assert.assertTrue(
                mVerificationResultSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertTrue(mLastVerified);
        Assert.assertTrue(ThreadUtils.runOnUiThreadBlocking(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return OriginVerifier.isValidOrigin(PACKAGE_NAME, TEST_HTTPS_ORIGIN_1,
                        CustomTabsService.RELATION_USE_AS_ORIGIN);
            }
        }));
        Assert.assertFalse(ThreadUtils.runOnUiThreadBlocking(new Callable<Boolean>() {
            @Override
            public Boolean call() throws Exception {
                return OriginVerifier.isValidOrigin(PACKAGE_NAME, TEST_HTTPS_ORIGIN_1,
                        CustomTabsService.RELATION_HANDLE_ALL_URLS);
            }
        }));
        Assert.assertEquals(mLastPackageName, PACKAGE_NAME);
        Assert.assertEquals(mLastOrigin,
                OriginVerifier.getPostMessageOriginFromVerifiedOrigin(
                        PACKAGE_NAME, TEST_HTTPS_ORIGIN_1));
    }
}
