// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.browserservices.Origin;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityClient;

/**
 * Tests for {@link NotificationPermissionUpdater}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NotificationPermissionUpdaterTest {
    private static final Origin ORIGIN = new Origin("https://www.website.com");
    private static final String PACKAGE_NAME = "com.package.name";
    private static final String OTHER_PACKAGE_NAME = "com.other.package.name";

    @Mock public TrustedWebActivityPermissionManager mPermissionManager;
    @Mock public TrustedWebActivityClient mTrustedWebActivityClient;

    private NotificationPermissionUpdater mNotificationPermissionUpdater;
    private ShadowPackageManager mShadowPackageManager;

    private boolean mNotificationsEnabled;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        PackageManager pm = RuntimeEnvironment.application.getPackageManager();
        mShadowPackageManager = shadowOf(pm);
        mNotificationPermissionUpdater = new NotificationPermissionUpdater(
                RuntimeEnvironment.application, mPermissionManager, mTrustedWebActivityClient);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void doesntRegister_whenClientDoesntHandleIntents() {
        mNotificationPermissionUpdater.onOriginVerified(ORIGIN, PACKAGE_NAME);

        verify(mPermissionManager, never()).register(any(), anyBoolean());
    }

    @Test
    @Feature("TrustedWebActivities")
    public void doesntRegister_whenOtherClientHandlesIntent() {
        installBrowsableIntentHandler(ORIGIN, "com.package.other");

        mNotificationPermissionUpdater.onOriginVerified(ORIGIN, PACKAGE_NAME);

        verify(mPermissionManager, never()).register(any(), anyBoolean());
    }


    @Test
    @Feature("TrustedWebActivities")
    public void disablesNotifications_whenClientDoesntHaveService() {
        installBrowsableIntentHandler(ORIGIN, PACKAGE_NAME);

        mNotificationPermissionUpdater.onOriginVerified(ORIGIN, PACKAGE_NAME);

        verify(mPermissionManager).register(eq(ORIGIN), eq(false));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void disablesNotifications_whenClientNotificationsAreDisabled() {
        installBrowsableIntentHandler(ORIGIN, PACKAGE_NAME);
        installTrustedWebActivityService(ORIGIN);
        setNotificationsEnabledForClient(false);

        mNotificationPermissionUpdater.onOriginVerified(ORIGIN, PACKAGE_NAME);

        verify(mPermissionManager).register(eq(ORIGIN), eq(false));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void enablesNotifications_whenClientNotificationsAreEnabled() {
        installBrowsableIntentHandler(ORIGIN, PACKAGE_NAME);
        // TODO(peconn): Rename this.
        installTrustedWebActivityService(ORIGIN);
        setNotificationsEnabledForClient(true);

        mNotificationPermissionUpdater.onOriginVerified(ORIGIN, PACKAGE_NAME);

        verify(mPermissionManager).register(eq(ORIGIN), eq(true));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void updatesPermission_onSubsequentCalls() {
        installBrowsableIntentHandler(ORIGIN, PACKAGE_NAME);

        installTrustedWebActivityService(ORIGIN);
        setNotificationsEnabledForClient(true);
        mNotificationPermissionUpdater.onOriginVerified(ORIGIN, PACKAGE_NAME);
        verify(mPermissionManager).register(eq(ORIGIN), eq(true));

        setNotificationsEnabledForClient(false);
        mNotificationPermissionUpdater.onOriginVerified(ORIGIN, PACKAGE_NAME);
        verify(mPermissionManager).register(eq(ORIGIN), eq(false));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void updatesPermission_onNewClient() {
        installBrowsableIntentHandler(ORIGIN, PACKAGE_NAME);
        installBrowsableIntentHandler(ORIGIN, OTHER_PACKAGE_NAME);

        installTrustedWebActivityService(ORIGIN);
        setNotificationsEnabledForClient(true);
        mNotificationPermissionUpdater.onOriginVerified(ORIGIN, PACKAGE_NAME);
        verify(mPermissionManager).register(eq(ORIGIN), eq(true));

        setNotificationsEnabledForClient(false);
        mNotificationPermissionUpdater.onOriginVerified(ORIGIN, OTHER_PACKAGE_NAME);
        verify(mPermissionManager).register(eq(ORIGIN), eq(false));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void unregisters_onClientUninstall() {
        installBrowsableIntentHandler(ORIGIN, PACKAGE_NAME);
        installTrustedWebActivityService(ORIGIN);
        setNotificationsEnabledForClient(true);

        mNotificationPermissionUpdater.onOriginVerified(ORIGIN, PACKAGE_NAME);

        uninstallTrustedWebActivityService(ORIGIN);
        mNotificationPermissionUpdater.onClientAppUninstalled(ORIGIN);

        verify(mPermissionManager).unregister(eq(ORIGIN));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void unregisters_whenClientDoesntHaveService() {
        installBrowsableIntentHandler(ORIGIN, PACKAGE_NAME);

        mNotificationPermissionUpdater.onOriginVerified(ORIGIN, PACKAGE_NAME);

        // Client handles intents but doesn't have a service, so we disable the notification
        // permission.
        verify(mPermissionManager).register(eq(ORIGIN), eq(false));

        mNotificationPermissionUpdater.onClientAppUninstalled(ORIGIN);

        verify(mPermissionManager).unregister(eq(ORIGIN));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void doesntUnregister_whenOtherClientsRemain() {
        installBrowsableIntentHandler(ORIGIN, PACKAGE_NAME);

        installTrustedWebActivityService(ORIGIN);
        setNotificationsEnabledForClient(true);

        mNotificationPermissionUpdater.onOriginVerified(ORIGIN, PACKAGE_NAME);
        verify(mPermissionManager).register(eq(ORIGIN), eq(true));

        // Since we haven't called uninstallTrustedWebActivityService, the Updater sees that
        // notifications can still be handled by other apps. We don't unregister, but we do update
        // to the permission to that of the other app.
        setNotificationsEnabledForClient(false);
        mNotificationPermissionUpdater.onClientAppUninstalled(ORIGIN);
        verify(mPermissionManager, never()).unregister(any());
        verify(mPermissionManager).register(eq(ORIGIN), eq(false));

        uninstallTrustedWebActivityService(ORIGIN);
        mNotificationPermissionUpdater.onClientAppUninstalled(ORIGIN);
        verify(mPermissionManager).unregister(eq(ORIGIN));
    }

    /** "Installs" the given package to handle intents for that origin. */
    private void installBrowsableIntentHandler(Origin origin, String packageName) {
        Intent intent = new Intent();
        intent.setPackage(packageName);
        intent.setData(origin.uri());
        intent.setAction(Intent.ACTION_VIEW);
        intent.addCategory(Intent.CATEGORY_BROWSABLE);

        mShadowPackageManager.addResolveInfoForIntent(intent, new ResolveInfo());
    }

    /** "Installs" a Trusted Web Activity Service for the origin. */
    @SuppressWarnings("unchecked")
    private void installTrustedWebActivityService(Origin origin) {
        when(mTrustedWebActivityClient.checkNotificationPermission(eq(origin), any())).thenAnswer(
                invocation -> {
                    ((Callback<Boolean>) invocation.getArgument(1))
                            .onResult(mNotificationsEnabled);
                    return true;
                }
        );
    }

    private void setNotificationsEnabledForClient(boolean enabled) {
        mNotificationsEnabled = enabled;
    }

    private void uninstallTrustedWebActivityService(Origin origin) {
        when(mTrustedWebActivityClient.checkNotificationPermission(eq(origin), any()))
                .thenReturn(false);
    }
}
