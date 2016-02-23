// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.Manifest;
import android.os.Environment;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.preferences.website.ContentSetting;
import org.chromium.chrome.browser.preferences.website.GeolocationInfo;
import org.chromium.chrome.test.ChromeTabbedActivityTestBase;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.AndroidPermissionDelegate;
import org.chromium.ui.base.WindowAndroid.PermissionCallback;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;

/**
 * Tests for the permission update infobar.
 */
public class PermissionUpdateInfobarTest extends ChromeTabbedActivityTestBase {

    private static final String GEOLOCATION_PAGE =
            "/chrome/test/data/geolocation/geolocation_on_load.html";
    private static final String GEOLOCATION_IFRAME_PAGE =
            "/chrome/test/data/geolocation/geolocation_iframe_on_load.html";

    private InfoBarTestAnimationListener mListener;
    private EmbeddedTestServer mTestServer;

    public PermissionUpdateInfobarTest() {
        mSkipCheckHttpServer = true;
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTestServer = EmbeddedTestServer.createAndStartFileServer(
                getInstrumentation().getContext(), Environment.getExternalStorageDirectory());
    }

    @Override
    protected void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
        super.tearDown();
    }

    // Ensure destroying the permission update infobar does not crash when handling geolocation
    // permissions.
    @MediumTest
    public void testInfobarShutsDownCleanlyForGeolocation()
            throws IllegalArgumentException, InterruptedException {
        ChromeTabUtils.newTabFromMenu(getInstrumentation(), getActivity());

        // Register for animation notifications
        CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                if (getActivity().getActivityTab() == null) return false;
                if (getActivity().getActivityTab().getInfoBarContainer() == null) return false;
                return true;
            }
        });
        InfoBarContainer container = getActivity().getActivityTab().getInfoBarContainer();
        mListener =  new InfoBarTestAnimationListener();
        container.setAnimationListener(mListener);

        final String locationUrl = mTestServer.getURL(GEOLOCATION_PAGE);
        final GeolocationInfo geolocationSettings = ThreadUtils.runOnUiThreadBlockingNoException(
                new Callable<GeolocationInfo>() {
                    @Override
                    public GeolocationInfo call() {
                        return new GeolocationInfo(locationUrl, null, false);
                    }
                });

        getActivity().getWindowAndroid().setAndroidPermissionDelegate(
                new TestAndroidPermissionDelegate(
                        null,
                        Arrays.asList(Manifest.permission.ACCESS_FINE_LOCATION),
                        null));
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);

        try {
            ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    geolocationSettings.setContentSetting(ContentSetting.ALLOW);
                }
            });

            loadUrl(mTestServer.getURL(GEOLOCATION_PAGE));
            assertTrue("InfoBar not added", mListener.addInfoBarAnimationFinished());
            assertEquals(1, getInfoBars().size());

            final WebContents webContents = ThreadUtils.runOnUiThreadBlockingNoException(
                    new Callable<WebContents>() {
                        @Override
                        public WebContents call() throws Exception {
                            return getActivity().getActivityTab().getWebContents();
                        }
                    });
            assertFalse(webContents.isDestroyed());

            ChromeTabUtils.closeCurrentTab(getInstrumentation(), getActivity());
            CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return webContents.isDestroyed();
                }
            });

            CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return getActivity().getTabModelSelector().getModel(false).getCount() == 1;
                }
            });
        } finally {
            ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    geolocationSettings.setContentSetting(ContentSetting.DEFAULT);
                }
            });
        }
    }

    @MediumTest
    public void testInfobarFrameNavigationForGeolocation()
            throws IllegalArgumentException, InterruptedException, TimeoutException {
        ChromeTabUtils.newTabFromMenu(getInstrumentation(), getActivity());

        // Register for animation notifications
        CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                if (getActivity().getActivityTab() == null) return false;
                if (getActivity().getActivityTab().getInfoBarContainer() == null) return false;
                return true;
            }
        });
        InfoBarContainer container = getActivity().getActivityTab().getInfoBarContainer();
        mListener =  new InfoBarTestAnimationListener();
        container.setAnimationListener(mListener);

        final String locationUrl = mTestServer.getURL(GEOLOCATION_IFRAME_PAGE);
        final GeolocationInfo geolocationSettings = ThreadUtils.runOnUiThreadBlockingNoException(
                new Callable<GeolocationInfo>() {
                    @Override
                    public GeolocationInfo call() {
                        return new GeolocationInfo(locationUrl, null, false);
                    }
                });

        getActivity().getWindowAndroid().setAndroidPermissionDelegate(
                new TestAndroidPermissionDelegate(
                        null,
                        Arrays.asList(Manifest.permission.ACCESS_FINE_LOCATION),
                        null));
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);

        try {
            ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    geolocationSettings.setContentSetting(ContentSetting.ALLOW);
                }
            });

            loadUrl(mTestServer.getURL(GEOLOCATION_IFRAME_PAGE));
            assertTrue("InfoBar not added", mListener.addInfoBarAnimationFinished());
            assertEquals(1, getInfoBars().size());

            final WebContents webContents = ThreadUtils.runOnUiThreadBlockingNoException(
                    new Callable<WebContents>() {
                        @Override
                        public WebContents call() throws Exception {
                            return getActivity().getActivityTab().getWebContents();
                        }
                    });
            assertFalse(webContents.isDestroyed());

            runJavaScriptCodeInCurrentTab("document.querySelector('iframe').src = '';");
            CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return getInfoBars().size() == 0;
                }
            });

            ChromeTabUtils.closeCurrentTab(getInstrumentation(), getActivity());
            CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return webContents.isDestroyed();
                }
            });

            CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return getActivity().getTabModelSelector().getModel(false).getCount() == 1;
                }
            });
        } finally {
            ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    geolocationSettings.setContentSetting(ContentSetting.DEFAULT);
                }
            });
        }
    }

    private static class TestAndroidPermissionDelegate implements AndroidPermissionDelegate {
        private final Set<String> mHasPermissions;
        private final Set<String> mRequestablePermissions;
        private final Set<String> mPolicyRevokedPermissions;

        public TestAndroidPermissionDelegate(
                List<String> hasPermissions,
                List<String> requestablePermissions,
                List<String> policyRevokedPermissions) {
            mHasPermissions = new HashSet<>(hasPermissions == null
                    ? new ArrayList<String>() : hasPermissions);
            mRequestablePermissions = new HashSet<>(requestablePermissions == null
                    ? new ArrayList<String>() : requestablePermissions);
            mPolicyRevokedPermissions = new HashSet<>(policyRevokedPermissions == null
                    ? new ArrayList<String>() : policyRevokedPermissions);
        }

        @Override
        public boolean hasPermission(String permission) {
            return mHasPermissions.contains(permission);
        }

        @Override
        public boolean canRequestPermission(String permission) {
            return mRequestablePermissions.contains(permission);
        }

        @Override
        public boolean isPermissionRevokedByPolicy(String permission) {
            return mPolicyRevokedPermissions.contains(permission);
        }

        @Override
        public void requestPermissions(String[] permissions, PermissionCallback callback) {
        }
    }

}
