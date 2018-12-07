// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.UserManager;
import android.support.customtabs.CustomTabsIntent;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowApplication;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.searchwidget.SearchActivity;

/** JUnit tests for first run triggering code. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class FirstRunIntegrationUnitTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Context mContext;
    private ShadowApplication mShadowApplication;

    @Before
    public void setUp() throws InterruptedException {
        mContext = RuntimeEnvironment.application;
        mShadowApplication = ShadowApplication.getInstance();

        UserManager userManager = Mockito.mock(UserManager.class);
        Mockito.when(userManager.isDemoUser()).thenReturn(false);
        mShadowApplication.setSystemService(Context.USER_SERVICE, userManager);

        FirstRunStatus.setFirstRunFlowComplete(false);
    }

    /**
     * Checks that either {@link FirstRunActivity} or {@link TabbedModeFirstRunActivity}
     * was launched.
     */
    private void assertFirstRunActivityLaunched() {
        Intent launchedIntent = mShadowApplication.getNextStartedActivity();
        Assert.assertNotNull(launchedIntent);

        String launchedActivityClassName = launchedIntent.getComponent().getClassName();
        Assert.assertTrue(launchedActivityClassName.equals(FirstRunActivity.class.getName())
                || launchedActivityClassName.equals(TabbedModeFirstRunActivity.class.getName()));
    }

    @Test
    public void testGenericViewIntentGoesToFirstRun() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("http://test.com"));
        intent.setPackage(mContext.getPackageName());
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        Activity launcherActivity =
                Robolectric.buildActivity(ChromeLauncherActivity.class, intent).create().get();
        assertFirstRunActivityLaunched();
        Assert.assertTrue(launcherActivity.isFinishing());
    }

    @Test
    public void testRedirectCustomTabActivityToFirstRun() {
        CustomTabsIntent customTabIntent = new CustomTabsIntent.Builder().build();
        customTabIntent.intent.setPackage(mContext.getPackageName());
        customTabIntent.intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        customTabIntent.launchUrl(mContext, Uri.parse("http://test.com"));
        Intent launchedIntent = mShadowApplication.getNextStartedActivity();
        Assert.assertNotNull(launchedIntent);

        Activity launcherActivity =
                Robolectric.buildActivity(ChromeLauncherActivity.class, launchedIntent)
                        .create()
                        .get();
        assertFirstRunActivityLaunched();
        Assert.assertTrue(launcherActivity.isFinishing());
    }

    @Test
    public void testRedirectChromeTabbedActivityToFirstRun() {
        Intent intent = new Intent();
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        Activity tabbedActivity =
                Robolectric.buildActivity(ChromeTabbedActivity.class, intent).create().get();
        assertFirstRunActivityLaunched();
        Assert.assertTrue(tabbedActivity.isFinishing());
    }

    @Test
    public void testRedirectSearchActivityToFirstRun() {
        Intent intent = new Intent();
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        Activity searchActivity =
                Robolectric.buildActivity(SearchActivity.class, intent).create().get();
        assertFirstRunActivityLaunched();
        Assert.assertTrue(searchActivity.isFinishing());
    }
}
