// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.lib.runtime_library;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.RemoteException;

import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

/**
 * Unit tests for {@link org.chromium.webapk.WebApkServiceImpl}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class WebApkServiceImplTest {

    private static final String ALLOWED_PACKAGE = "com.allowed.package";
    private static final int ALLOWED_UID = 0;

    private PackageManager mPackageManager;

    @Before
    public void setUp() {
        mPackageManager = mock(PackageManager.class);
        when(mPackageManager.getPackagesForUid(ALLOWED_UID)).thenReturn(
                new String[] { ALLOWED_PACKAGE });
    }

    @Test
    public void testAllowedCaller() {
        WebApkServiceImpl service =
                new WebApkServiceImpl(Robolectric.application, createBundle(ALLOWED_PACKAGE));
        assertTrue(service.checkHasAccess(ALLOWED_UID, mPackageManager));
    }

    @Test
    public void testProhibitedCaller() {
        WebApkServiceImpl service =
                new WebApkServiceImpl(Robolectric.application, createBundle(ALLOWED_PACKAGE));
        assertFalse(service.checkHasAccess(ALLOWED_UID + 1, mPackageManager));
    }

    @Test
    public void testOnTransactThrowsIfNoPermission() {
        WebApkServiceImpl service = new WebApkServiceImpl(
                Robolectric.application, createBundle(ALLOWED_PACKAGE)) {
            @Override
            boolean checkHasAccess(int uid, PackageManager packageManager) {
                return false;
            }
        };
        try {
            service.onTransact(0,  null,  null,  0);
            Assert.fail("Should have thrown an exception for no access.");
        } catch (RemoteException e) {
        }
    }

    @Test
    public void testOnTransactRunsIfHasPermission() {
        WebApkServiceImpl service = new WebApkServiceImpl(
                Robolectric.application, createBundle(ALLOWED_PACKAGE)) {
            @Override
            boolean checkHasAccess(int uid, PackageManager packageManager) {
                return true;
            }
        };
        try {
            service.onTransact(0,  null,  null,  0);
        } catch (RemoteException e) {
            e.printStackTrace();
            Assert.fail("Should not have thrown an exception when permission is granted.");
        }
    }

    /**
     * Creates Bundle to pass to WebApkServiceImpl constructor.
     */
    private static Bundle createBundle(String expectedHostBrowser) {
        Bundle bundle = new Bundle();
        bundle.putString(WebApkServiceImpl.KEY_HOST_BROWSER_PACKAGE, expectedHostBrowser);
        return bundle;
    }
}
