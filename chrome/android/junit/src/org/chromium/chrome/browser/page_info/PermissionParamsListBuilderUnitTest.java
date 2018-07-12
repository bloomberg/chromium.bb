// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import static junit.framework.Assert.assertNotNull;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import android.content.Intent;
import android.provider.Settings;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ContentSettingsType;
import org.chromium.chrome.browser.page_info.PermissionParamsListBuilderUnitTest.ShadowWebsitePreferenceBridge;
import org.chromium.chrome.browser.preferences.website.ContentSetting;
import org.chromium.chrome.browser.preferences.website.WebsitePreferenceBridge;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.ui.base.AndroidPermissionDelegate;
import org.chromium.ui.base.PermissionCallback;

import java.util.List;

/**
 * Unit tests for PermissionParamsListBuilder.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowWebsitePreferenceBridge.class})
public class PermissionParamsListBuilderUnitTest {
    private PermissionParamsListBuilder mPermissionParamsListBuilder;
    private FakeSystemSettingsActivityRequiredListener mSettingsActivityRequiredListener;

    @Before
    public void setUp() throws Exception {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        AndroidPermissionDelegate permissionDelegate = new FakePermissionDelegate();
        mSettingsActivityRequiredListener = new FakeSystemSettingsActivityRequiredListener();
        mPermissionParamsListBuilder =
                new PermissionParamsListBuilder(RuntimeEnvironment.application, permissionDelegate,
                        "https://example.com", mSettingsActivityRequiredListener, result -> {});
    }

    @Test
    public void addSingleEntryAndBuild() {
        mPermissionParamsListBuilder.addPermissionEntry(
                "Foo", ContentSettingsType.CONTENT_SETTINGS_TYPE_GEOLOCATION, ContentSetting.ALLOW);

        List<PageInfoView.PermissionParams> params = mPermissionParamsListBuilder.build();

        assertEquals(1, params.size());
        PageInfoView.PermissionParams permissionParams = params.get(0);

        assertEquals(R.drawable.permission_location, permissionParams.iconResource);

        String expectedStatus = "Foo – "
                + RuntimeEnvironment.application.getString(R.string.page_info_permission_allowed);
        assertEquals(expectedStatus, permissionParams.status.toString());

        assertNull(permissionParams.clickCallback);
    }

    @Test
    public void addLocationEntryAndBuildWhenSystemLocationDisabled() {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(false);
        mPermissionParamsListBuilder.addPermissionEntry("Test",
                ContentSettingsType.CONTENT_SETTINGS_TYPE_GEOLOCATION, ContentSetting.ALLOW);

        List<PageInfoView.PermissionParams> params = mPermissionParamsListBuilder.build();

        assertEquals(1, params.size());
        PageInfoView.PermissionParams permissionParams = params.get(0);
        assertEquals(
                R.string.page_info_android_location_blocked, permissionParams.warningTextResource);

        assertNotNull(permissionParams.clickCallback);
        permissionParams.clickCallback.run();
        assertEquals(1, mSettingsActivityRequiredListener.getCallCount());
        assertEquals(Settings.ACTION_LOCATION_SOURCE_SETTINGS,
                mSettingsActivityRequiredListener.getIntentOverride().getAction());
    }

    private static class FakePermissionDelegate implements AndroidPermissionDelegate {
        @Override
        public boolean hasPermission(String permission) {
            return true;
        }

        @Override
        public boolean canRequestPermission(String permission) {
            return true;
        }

        @Override
        public boolean isPermissionRevokedByPolicy(String permission) {
            return false;
        }

        @Override
        public void requestPermissions(String[] permissions, PermissionCallback callback) {}

        @Override
        public boolean handlePermissionResult(
                int requestCode, String[] permissions, int[] grantResults) {
            return false;
        }
    }

    /**
     * Allows us to stub out the static calls to native.
     */
    @Implements(WebsitePreferenceBridge.class)
    public static class ShadowWebsitePreferenceBridge {
        @Implementation
        public static boolean isPermissionControlledByDSE(
                @ContentSettingsType int contentSettingsType, String origin, boolean isIncognito) {
            return false;
        }
    }

    private static class FakeSystemSettingsActivityRequiredListener
            implements SystemSettingsActivityRequiredListener {
        int mCallCount = 0;
        Intent mIntentOverride;

        @Override
        public void onSystemSettingsActivityRequired(Intent intentOverride) {
            mCallCount++;
            mIntentOverride = intentOverride;
        }

        public int getCallCount() {
            return mCallCount;
        }

        Intent getIntentOverride() {
            return mIntentOverride;
        }
    }
}