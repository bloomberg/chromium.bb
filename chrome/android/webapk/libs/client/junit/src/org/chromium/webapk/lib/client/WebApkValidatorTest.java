// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.lib.client;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.ResolveInfo;
import android.content.pm.Signature;

import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.res.builder.RobolectricPackageManager;

import java.net.URISyntaxException;
import java.util.ArrayList;
import java.util.List;

/**
 * Unit tests for {@link org.chromium.webapk.lib.client.WebApkValidator}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class WebApkValidatorTest {
    private static final String WEBAPK_PACKAGE_NAME = "org.chromium.webapk.foo";
    private static final String INVALID_WEBAPK_PACKAGE_NAME = "invalid.org.chromium.webapk.foo";
    private static final String URL_OF_WEBAPK = "https://www.foo.com";
    private static final String URL_WITHOUT_WEBAPK = "https://www.other.com";

    private static final byte[] EXPECTED_SIGNATURE = new byte[] {
        48, -126, 3, -121, 48, -126, 2, 111, -96, 3, 2, 1, 2, 2, 4, 20, -104, -66, -126, 48, 13,
        6, 9, 42, -122, 72, -122, -9, 13, 1, 1, 11, 5, 0, 48, 116, 49, 11, 48, 9, 6, 3, 85, 4,
        6, 19, 2, 67, 65, 49, 16, 48, 14, 6, 3, 85, 4, 8, 19, 7, 79, 110, 116, 97, 114, 105,
        111, 49, 17, 48, 15, 6, 3, 85, 4, 7, 19, 8, 87, 97, 116, 101, 114, 108, 111, 111, 49,
        17, 48, 15, 6, 3, 85, 4, 10, 19, 8, 67, 104, 114, 111, 109, 105, 117, 109, 49, 17, 48};

    private static final byte[] SIGNATURE_1 = new byte[] {
        13, 52, 51, 48, 51, 48, 51, 49, 53, 49, 54, 52, 52, 90, 48, 116, 49, 11, 48, 9, 6, 3,
        85, 4, 6, 19, 2, 67, 65, 49, 16, 48, 14, 6, 3, 85, 4, 8, 19, 7, 79, 110, 116, 97, 114};

    private static final byte[] SIGNATURE_2 = new byte[] {
        49, 17, 48, 15, 6, 3, 85, 4, 10, 19, 8, 67, 104, 114, 111, 109, 105, 117, 109, 49, 17,
        48, 15, 6, 3, 85, 4, 11, 19, 8, 67, 104, 114, 111, 109, 105, 117, 109, 49, 26, 48, 24};

    private Context mContext;
    private PackageManager mPackageManager;
    private PackageInfo mWebApkPackageInfo;
    private PackageInfo mInvalidWebApkPackageInfo;

    @Before
    public void setUp() {
        mContext = mock(Context.class);
        mPackageManager = mock(PackageManager.class);
        when(mContext.getPackageManager()).thenReturn(mPackageManager);
        WebApkValidator.initWithBrowserHostSignature(EXPECTED_SIGNATURE);
    }

    /**
     * Tests {@link WebApkValidator.queryWebApkPackage()} returns a WebAPK's package name if the
     * WebAPK can handle the given URL.
     */
    @Test
    public void testQueryWebApkPackageReturnsWebApkPackageNameIfTheURLCanBeHandled() {
        try {
            Intent intent = Intent.parseUri(URL_OF_WEBAPK, Intent.URI_INTENT_SCHEME);
            intent.addCategory(Intent.CATEGORY_BROWSABLE);

            ResolveInfo info = newResolveInfo(WEBAPK_PACKAGE_NAME);
            RobolectricPackageManager packageManager =
                    (RobolectricPackageManager) Robolectric.application.getPackageManager();
            packageManager.addResolveInfoForIntent(intent, info);

            assertEquals(WEBAPK_PACKAGE_NAME,
                    WebApkValidator.queryWebApkPackage(Robolectric.application, URL_OF_WEBAPK));
        } catch (URISyntaxException e) {
            Assert.fail("URI is invalid.");
        }
    }

    /**
     * Tests {@link WebApkValidator.queryWebApkPackage()} returns null for a non-browsable Intent.
     */
    @Test
    public void testQueryWebApkPackageReturnsNullForNonBrowsableIntent() {
        try {
            Intent intent = Intent.parseUri(URL_OF_WEBAPK, Intent.URI_INTENT_SCHEME);

            ResolveInfo info = newResolveInfo(WEBAPK_PACKAGE_NAME);
            RobolectricPackageManager packageManager =
                    (RobolectricPackageManager) Robolectric.application.getPackageManager();
            packageManager.addResolveInfoForIntent(intent, info);

            assertNull(WebApkValidator.queryWebApkPackage(
                    Robolectric.application, URL_OF_WEBAPK));
        } catch (URISyntaxException e) {
            Assert.fail("URI is invalid.");
        }
    }

    /**
     * Tests {@link WebApkValidator.queryWebApkPackage()} returns null if no WebAPK handles
     * the given URL.
     */
    @Test
    public void testQueryWebApkPackageReturnsNullWhenNoWebApkHandlesTheURL() {
        try {
            Intent intent = Intent.parseUri(URL_OF_WEBAPK, Intent.URI_INTENT_SCHEME);
            intent.addCategory(Intent.CATEGORY_BROWSABLE);

            ResolveInfo info = newResolveInfo(WEBAPK_PACKAGE_NAME);
            RobolectricPackageManager packageManager =
                    (RobolectricPackageManager) Robolectric.application.getPackageManager();
            packageManager.addResolveInfoForIntent(intent, info);

            assertNull(WebApkValidator.queryWebApkPackage(
                    Robolectric.application, URL_WITHOUT_WEBAPK));
        } catch (URISyntaxException e) {
            Assert.fail("URI is invalid.");
        }
    }

    /**
     * Tests {@link WebApkValidator.findWebApkPackage()} returns a WebAPK's package name when there
     * are ResolveInfos corresponds to a WebAPK.
     */
    @Test
    public void testFindWebApkPackageReturnsWebApkPackageName() {
        List<ResolveInfo> infos = new ArrayList<ResolveInfo>();
        infos.add(newResolveInfo(WEBAPK_PACKAGE_NAME));
        assertEquals(WEBAPK_PACKAGE_NAME, WebApkValidator.findWebApkPackage(infos));
    }

    /**
     * Tests {@link WebApkValidator.findWebApkPackage()} returns null when there isn't any
     * ResolveInfos corresponds to a WebAPK.
     */
    @Test
    public void testFindWebApkPackageReturnsNullWhenNoResolveInfosCorrespondingToWebApk() {
        List<ResolveInfo> infos = new ArrayList<ResolveInfo>();
        infos.add(newResolveInfo("com.google.android"));
        assertNull(WebApkValidator.findWebApkPackage(infos));
    }
    /**
     * Tests {@link WebApkValidator.IsValidWebApk} returns false for a package that doesn't start
     * with {@link WebApkConstants.WEBAPK_PACKAGE_PREFIX}.
     */
    @Test
    public void testIsValidWebApkReturnsFalseForInvalidPackageName() {
        assertFalse(WebApkValidator.isValidWebApk(Robolectric.application.getApplicationContext(),
                INVALID_WEBAPK_PACKAGE_NAME));
    }

    /**
     * Tests {@link WebApkValidator.IsValidWebApk} returns true if the WebAPK has the expected
     * signature.
     */
    @Test
    public void testIsValidWebApkReturnsTrueForValidWebApk() throws NameNotFoundException {
        mWebApkPackageInfo = mock(PackageInfo.class);
        when(mPackageManager.getPackageInfo(WEBAPK_PACKAGE_NAME, PackageManager.GET_SIGNATURES))
                     .thenReturn(mWebApkPackageInfo);
        mWebApkPackageInfo.signatures = new Signature[] {new Signature(EXPECTED_SIGNATURE)};

        assertTrue(WebApkValidator.isValidWebApk(mContext, WEBAPK_PACKAGE_NAME));
    }

    /**
     * Tests {@link WebApkValidator.IsValidWebApk} returns true if a WebAPK has multiple
     * signatures and one matches the expected signature.
     */
    @Test
    public void testIsValidWebApkReturnsTrueForWebApkWithMultipleSignaturesAndOneMatched()
            throws NameNotFoundException {
        mWebApkPackageInfo = mock(PackageInfo.class);
        when(mPackageManager.getPackageInfo(WEBAPK_PACKAGE_NAME, PackageManager.GET_SIGNATURES))
                     .thenReturn(mWebApkPackageInfo);
        mWebApkPackageInfo.signatures = new Signature[] {new Signature(SIGNATURE_1),
                new Signature(SIGNATURE_2), new Signature(EXPECTED_SIGNATURE)};

        assertTrue(WebApkValidator.isValidWebApk(mContext, WEBAPK_PACKAGE_NAME));
    }

    /**
     * Tests {@link WebApkValidator.IsValidWebApk} returns false if a WebAPK has multiple
     * signatures but none of them matches the expected signature.
     */
    @Test
    public void testIsValidWebApkReturnsFalseForWebApkWithMultipleSignaturesWithoutAnyMatched()
            throws NameNotFoundException {
        mWebApkPackageInfo = mock(PackageInfo.class);
        when(mPackageManager.getPackageInfo(WEBAPK_PACKAGE_NAME, PackageManager.GET_SIGNATURES))
                     .thenReturn(mWebApkPackageInfo);
        mWebApkPackageInfo.signatures = new Signature[] {new Signature(SIGNATURE_1),
                new Signature(SIGNATURE_2)};

        assertFalse(WebApkValidator.isValidWebApk(mContext, WEBAPK_PACKAGE_NAME));
    }

    private static ResolveInfo newResolveInfo(String packageName) {
        ActivityInfo activityInfo = new ActivityInfo();
        activityInfo.packageName = packageName;
        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo = activityInfo;
        return resolveInfo;
    }
}
