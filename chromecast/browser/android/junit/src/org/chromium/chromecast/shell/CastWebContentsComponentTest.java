// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.app.Application;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.ServiceConnection;
import android.support.v4.content.LocalBroadcastManager;

import org.junit.Assert;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowActivity;

import org.chromium.base.ContextUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Tests for CastWebContentsComponent.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE, application = CastWebContentsComponentTest.FakeApplication.class)
public class CastWebContentsComponentTest {
    public static class FakeApplication extends Application {
        @Override
        protected void attachBaseContext(Context base) {
            super.attachBaseContext(base);
            ContextUtils.initApplicationContextForTests(this);
        }
    }

    private static final String INSTANCE_ID = "1";

    @Mock
    private WebContents mWebContents;

    private Activity mActivity;
    private ShadowActivity mShadowActivity;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Mockito.spy(Robolectric.buildActivity(Activity.class).setup().get());
        mShadowActivity = Shadows.shadowOf(mActivity);
    }

    @Test
    public void testStartStartsWebContentsActivity() {
        Assume.assumeFalse(BuildConfig.DISPLAY_WEB_CONTENTS_IN_SERVICE);

        CastWebContentsComponent component =
                new CastWebContentsComponent(INSTANCE_ID, null, null, false, false);
        component.start(mActivity, mWebContents);
        Intent intent = mShadowActivity.getNextStartedActivity();
        Assert.assertEquals(
                intent.getComponent().getClassName(), CastWebContentsActivity.class.getName());

        component.stop(mActivity);
    }

    @Test
    public void testStopSendsStopSignalToActivity() {
        Assume.assumeFalse(BuildConfig.DISPLAY_WEB_CONTENTS_IN_SERVICE);

        BroadcastReceiver receiver = Mockito.mock(BroadcastReceiver.class);
        IntentFilter intentFilter = new IntentFilter(CastIntents.ACTION_STOP_WEB_CONTENT);
        LocalBroadcastManager.getInstance(ContextUtils.getApplicationContext())
                .registerReceiver(receiver, intentFilter);

        CastWebContentsComponent component =
                new CastWebContentsComponent(INSTANCE_ID, null, null, false, false);
        component.start(ContextUtils.getApplicationContext(), mWebContents);
        component.stop(ContextUtils.getApplicationContext());

        LocalBroadcastManager.getInstance(ContextUtils.getApplicationContext())
                .unregisterReceiver(receiver);

        verify(receiver).onReceive(any(Context.class), any(Intent.class));
    }

    @Test
    public void testStartBindsWebContentsService() {
        Assume.assumeTrue(BuildConfig.DISPLAY_WEB_CONTENTS_IN_SERVICE);

        CastWebContentsComponent component =
                new CastWebContentsComponent(INSTANCE_ID, null, null, false, false);
        component.start(mActivity, mWebContents);
        component.stop(mActivity);

        ArgumentCaptor<Intent> intent = ArgumentCaptor.forClass(Intent.class);
        verify(mActivity).bindService(
                intent.capture(), any(ServiceConnection.class), eq(Context.BIND_AUTO_CREATE));
        Assert.assertEquals(intent.getValue().getComponent().getClassName(),
                CastWebContentsService.class.getName());
    }

    @Test
    public void testStopUnbindsWebContentsService() {
        Assume.assumeTrue(BuildConfig.DISPLAY_WEB_CONTENTS_IN_SERVICE);

        CastWebContentsComponent component =
                new CastWebContentsComponent(INSTANCE_ID, null, null, false, false);
        component.start(mActivity, mWebContents);
        component.stop(mActivity);

        verify(mActivity).unbindService(any(ServiceConnection.class));
    }

    @Test
    public void testOnComponentClosedCallsCallback() {
        CastWebContentsComponent.OnComponentClosedHandler callback =
                Mockito.mock(CastWebContentsComponent.OnComponentClosedHandler.class);

        CastWebContentsComponent component =
                new CastWebContentsComponent(INSTANCE_ID, callback, null, false, false);
        component.start(ContextUtils.getApplicationContext(), mWebContents);
        CastWebContentsComponent.onComponentClosed(
                ContextUtils.getApplicationContext(), INSTANCE_ID);
        verify(callback).onComponentClosed();

        component.stop(mActivity);
    }

    @Test
    public void testOnKeyDownCallsCallback() {
        CastWebContentsComponent.OnKeyDownHandler callback =
                Mockito.mock(CastWebContentsComponent.OnKeyDownHandler.class);

        CastWebContentsComponent component =
                new CastWebContentsComponent(INSTANCE_ID, null, callback, false, false);
        component.start(mActivity, mWebContents);
        CastWebContentsComponent.onKeyDown(mActivity, INSTANCE_ID, 42);
        component.stop(mActivity);

        verify(callback).onKeyDown(42);
    }

    @Test
    public void testStopDoesNotUnbindServiceIfStartWasNotCalled() {
        CastWebContentsComponent component =
                new CastWebContentsComponent(INSTANCE_ID, null, null, false, false);

        component.stop(mActivity);

        verify(mActivity, never()).unbindService(any(ServiceConnection.class));
    }

    @Test
    public void testStartWebContentsComponentMultipleTimes() {
        CastWebContentsComponent component =
                new CastWebContentsComponent(INSTANCE_ID, null, null, false, false);
        CastWebContentsComponent.Delegate delegate = mock(CastWebContentsComponent.Delegate.class);
        component.setDelegate(delegate);
        component.start(mActivity, mWebContents);
        Object receiver1 = component.getIntentReceiver();
        Assert.assertNotNull(receiver1);
        verify(delegate, times(1)).start(eq(mActivity), eq(mWebContents));
        component.start(mActivity, mWebContents);
        Object receiver2 = component.getIntentReceiver();
        Assert.assertEquals(receiver1, receiver2);
        verify(delegate, times(1)).start(any(Context.class), any(WebContents.class));
        component.stop(mActivity);
        Assert.assertNull(component.getIntentReceiver());
        verify(delegate, times(1)).stop(any(Context.class));
    }
}
