// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.lib.client;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.ResolveInfo;
import android.content.pm.Signature;

import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
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

    private RobolectricPackageManager mPackageManager;

    @Before
    public void setUp() {
        mPackageManager = (RobolectricPackageManager) RuntimeEnvironment
                .application.getPackageManager();
        WebApkValidator.initWithBrowserHostSignature(EXPECTED_SIGNATURE);
    }

    /**
     * Tests {@link WebApkValidator.queryWebApkPackage()} returns a WebAPK's package name if the
     * WebAPK can handle the given URL and the WebAPK is valid.
     */
    @Test
    public void testQueryWebApkPackageReturnsPackageIfTheURLCanBeHandled() {
        try {
            Intent intent = Intent.parseUri(URL_OF_WEBAPK, Intent.URI_INTENT_SCHEME);
            intent.addCategory(Intent.CATEGORY_BROWSABLE);

            mPackageManager.addResolveInfoForIntent(intent, newResolveInfo(WEBAPK_PACKAGE_NAME));
            mPackageManager.addPackage(newPackageInfoWithBrowserSignature(
                    WEBAPK_PACKAGE_NAME, new Signature(EXPECTED_SIGNATURE)));

            assertEquals(WEBAPK_PACKAGE_NAME, WebApkValidator.queryWebApkPackage(
                    RuntimeEnvironment.application, URL_OF_WEBAPK));
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

            mPackageManager.addResolveInfoForIntent(intent, newResolveInfo(WEBAPK_PACKAGE_NAME));
            mPackageManager.addPackage(newPackageInfoWithBrowserSignature(
                    WEBAPK_PACKAGE_NAME, new Signature(EXPECTED_SIGNATURE)));

            assertNull(WebApkValidator.queryWebApkPackage(
                    RuntimeEnvironment.application, URL_OF_WEBAPK));
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

            mPackageManager.addResolveInfoForIntent(intent, newResolveInfo(WEBAPK_PACKAGE_NAME));
            mPackageManager.addPackage(newPackageInfoWithBrowserSignature(
                    WEBAPK_PACKAGE_NAME, new Signature(EXPECTED_SIGNATURE)));

            assertNull(WebApkValidator.queryWebApkPackage(
                    RuntimeEnvironment.application, URL_WITHOUT_WEBAPK));
        } catch (URISyntaxException e) {
            Assert.fail("URI is invalid.");
        }
    }

    /**
     * Tests {@link WebApkValidator.findWebApkPackage} returns the WebAPK package name if one of the
     * ResolveInfos corresponds to a WebAPK and the WebAPK is valid.
     */
    @Test
    public void testFindWebApkPackageReturnsPackageForValidWebApk() throws NameNotFoundException {
        List<ResolveInfo> infos = new ArrayList<ResolveInfo>();
        infos.add(newResolveInfo(WEBAPK_PACKAGE_NAME));
        mPackageManager.addPackage(newPackageInfoWithBrowserSignature(
                WEBAPK_PACKAGE_NAME, new Signature(EXPECTED_SIGNATURE)));

        assertEquals(WEBAPK_PACKAGE_NAME,
                WebApkValidator.findWebApkPackage(RuntimeEnvironment.application, infos));
    }

    /**
     * Tests {@link WebApkValidator.findWebApkPackage} returns null if none of the packages for the
     * ResolveInfos start with {@link WebApkConstants.WEBAPK_PACKAGE_PREFIX}.
     */
    @Test
    public void testFindWebApkPackageReturnsNullForInvalidPackageName() {
        List<ResolveInfo> infos = new ArrayList<ResolveInfo>();
        infos.add(newResolveInfo(INVALID_WEBAPK_PACKAGE_NAME));
        mPackageManager.addPackage(newPackageInfoWithBrowserSignature(
                INVALID_WEBAPK_PACKAGE_NAME, new Signature(EXPECTED_SIGNATURE)));

        assertNull(WebApkValidator.findWebApkPackage(RuntimeEnvironment.application, infos));
    }

    /**
     * Tests {@link WebApkValidator.findWebApkPackage} returns null if a WebAPK has more than 2
     * signatures, even if the second one matches the expected signature.
     */
    @Test
    public void testFindWebApkPackageReturnsNullForMoreThanTwoSignatures()
            throws NameNotFoundException {
        List<ResolveInfo> infos = new ArrayList<ResolveInfo>();
        infos.add(newResolveInfo(WEBAPK_PACKAGE_NAME));
        Signature[] signatures = new Signature[] {new Signature(SIGNATURE_1),
                new Signature(EXPECTED_SIGNATURE), new Signature(SIGNATURE_2)};
        mPackageManager.addPackage(newPackageInfo(WEBAPK_PACKAGE_NAME, signatures));

        assertNull(WebApkValidator.findWebApkPackage(RuntimeEnvironment.application, infos));
    }

    /**
     * Tests {@link WebApkValidator.findWebApkPackage} returns null if a WebAPK has multiple
     * signatures but none of the signatures match the expected signature.
     */
    @Test
    public void testFindWebApkPackageReturnsNullForWebApkWithMultipleSignaturesWithoutAnyMatched()
            throws NameNotFoundException {
        List<ResolveInfo> infos = new ArrayList<ResolveInfo>();
        infos.add(newResolveInfo(WEBAPK_PACKAGE_NAME));
        Signature signatures[] =
                new Signature[] {new Signature(SIGNATURE_1), new Signature(SIGNATURE_2)};
        mPackageManager.addPackage(newPackageInfo(WEBAPK_PACKAGE_NAME, signatures));

        assertNull(WebApkValidator.findWebApkPackage(RuntimeEnvironment.application, infos));
    }

    private static ResolveInfo newResolveInfo(String packageName) {
        ActivityInfo activityInfo = new ActivityInfo();
        activityInfo.packageName = packageName;
        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo = activityInfo;
        return resolveInfo;
    }

    private static PackageInfo newPackageInfo(String packageName, Signature[] signatures) {
        PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = packageName;
        packageInfo.signatures = signatures;
        return packageInfo;
    }

    // The browser signature is expected to always be the second signature - the first (and any
    // additional ones after the second) are ignored.
    private static PackageInfo newPackageInfoWithBrowserSignature(
            String packageName, Signature signature) {
        return newPackageInfo(packageName, new Signature[] {new Signature(""), signature});
    }
}
