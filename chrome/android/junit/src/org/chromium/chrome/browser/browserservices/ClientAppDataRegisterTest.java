// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

import java.util.Set;

/**
 * Tests for {@link ClientAppDataRegister}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ClientAppDataRegisterTest {
    private static final int UID = 23;
    private static final String APP_NAME = "Example App";
    private static final String DOMAIN_AND_REGISTRY = "example.com";
    private static final String OTHER_DOMAIN_AND_REGISTRY = "otherexample.com";

    private ClientAppDataRegister mRegister;

    @Before
    public void setUp() {
        mRegister = new ClientAppDataRegister();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void registration() {
        mRegister.registerPackageForDomainAndRegistry(UID, APP_NAME, DOMAIN_AND_REGISTRY);

        Assert.assertTrue(mRegister.chromeHoldsDataForPackage(UID));
        Assert.assertEquals(APP_NAME, mRegister.getAppNameForRegisteredUid(UID));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void deregistration() {
        mRegister.registerPackageForDomainAndRegistry(UID, APP_NAME, DOMAIN_AND_REGISTRY);
        mRegister.removePackage(UID);

        Assert.assertFalse(mRegister.chromeHoldsDataForPackage(UID));
        Assert.assertNull(mRegister.getAppNameForRegisteredUid(UID));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void getOrigins() {
        mRegister.registerPackageForDomainAndRegistry(UID, APP_NAME, DOMAIN_AND_REGISTRY);
        mRegister.registerPackageForDomainAndRegistry(UID, APP_NAME, OTHER_DOMAIN_AND_REGISTRY);

        Set<String> origins = mRegister.getDomainAndRegistriesForRegisteredUid(UID);
        Assert.assertEquals(2, origins.size());
        Assert.assertTrue(origins.contains(DOMAIN_AND_REGISTRY));
        Assert.assertTrue(origins.contains(OTHER_DOMAIN_AND_REGISTRY));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void clearOrigins() {
        mRegister.registerPackageForDomainAndRegistry(UID, APP_NAME, DOMAIN_AND_REGISTRY);
        mRegister.registerPackageForDomainAndRegistry(UID, APP_NAME, OTHER_DOMAIN_AND_REGISTRY);
        mRegister.removePackage(UID);

        Set<String> origins = mRegister.getDomainAndRegistriesForRegisteredUid(UID);
        Assert.assertTrue(origins.isEmpty());
    }

    @Test
    @Feature("TrustedWebActivities")
    public void getAppName() {
        mRegister.registerPackageForDomainAndRegistry(UID, APP_NAME, DOMAIN_AND_REGISTRY);
        Assert.assertEquals(APP_NAME, mRegister.getAppNameForRegisteredUid(UID));
    }
}
