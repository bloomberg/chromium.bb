// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.content.Intent;
import android.media.AudioManager;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowActivity;
import org.robolectric.shadows.ShadowAudioManager;

import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Tests for CastWebContentsActivity.
 *
 * TODO(sanfin): Add more tests.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CastWebContentsActivityTest {
    private ActivityController<CastWebContentsActivity> mActivityLifecycle;
    private CastWebContentsActivity mActivity;
    private ShadowActivity mShadowActivity;

    @Before
    public void setUp() {
        mActivityLifecycle = Robolectric.buildActivity(CastWebContentsActivity.class);
        mActivity = mActivityLifecycle.get();
        mActivity.testingModeForTesting();
        mShadowActivity = Shadows.shadowOf(mActivity);
    }

    @Test
    public void testNewIntentAfterFinishLaunchesNewActivity() {
        mActivityLifecycle.create();
        mActivity.finishForTesting();
        Intent intent = new Intent(Intent.ACTION_VIEW, null, RuntimeEnvironment.application,
                CastWebContentsActivity.class);
        mActivityLifecycle.newIntent(intent);
        Intent next = mShadowActivity.getNextStartedActivity();
        assertEquals(next.getComponent().getClassName(), CastWebContentsActivity.class.getName());
    }

    @Test
    public void testFinishDoesNotLaunchNewActivity() {
        mActivityLifecycle.create();
        mActivity.finishForTesting();
        Intent intent = mShadowActivity.getNextStartedActivity();
        assertNull(intent);
    }

    @Test
    public void testDoesNotRequestAudioFocusBeforeResume() {
        ShadowAudioManager shadowAudioManager = Shadows.shadowOf(
                CastAudioManager.getAudioManager(RuntimeEnvironment.application).getInternal());
        ShadowAudioManager.AudioFocusRequest originalRequest =
                shadowAudioManager.getLastAudioFocusRequest();
        mActivityLifecycle.create().start();
        assertEquals(shadowAudioManager.getLastAudioFocusRequest(), originalRequest);
    }

    @Test
    public void testRequestsAudioFocusOnResume() {
        ShadowAudioManager shadowAudioManager = Shadows.shadowOf(
                CastAudioManager.getAudioManager(RuntimeEnvironment.application).getInternal());
        ShadowAudioManager.AudioFocusRequest originalRequest =
                shadowAudioManager.getLastAudioFocusRequest();
        mActivityLifecycle.create().start().resume();
        assertNotEquals(shadowAudioManager.getLastAudioFocusRequest(), originalRequest);
    }

    @Test
    public void testAbandonsAudioFocusOnPause() {
        ShadowAudioManager shadowAudioManager = Shadows.shadowOf(
                CastAudioManager.getAudioManager(RuntimeEnvironment.application).getInternal());
        mActivityLifecycle.create().start().resume().pause();
        ShadowAudioManager.AudioFocusRequest originalRequest =
                shadowAudioManager.getLastAudioFocusRequest();
        assertEquals(
                shadowAudioManager.getLastAbandonedAudioFocusListener(), originalRequest.listener);
    }

    @Test
    public void testReleasesStreamMuteIfNecessaryOnPause() {
        CastAudioManager mockAudioManager = mock(CastAudioManager.class);
        mActivity.setAudioManagerForTesting(mockAudioManager);
        mActivityLifecycle.create().start().resume();
        mActivityLifecycle.pause();
        verify(mockAudioManager).releaseStreamMuteIfNecessary(AudioManager.STREAM_MUSIC);
    }
}
