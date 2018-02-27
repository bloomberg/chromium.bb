// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import static org.hamcrest.Matchers.contains;
import static org.hamcrest.Matchers.emptyIterable;
import static org.junit.Assert.assertThat;

import android.media.AudioManager;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowAudioManager;

import org.chromium.chromecast.base.Controller;
import org.chromium.chromecast.base.Observable;
import org.chromium.chromecast.base.Unit;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for CastAudioManager.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CastAudioManagerTest {
    @Test
    public void testAudioFocusScopeActivatedWhenRequestGranted() {
        CastAudioManager audioManager =
                CastAudioManager.getAudioManager(RuntimeEnvironment.application);
        ShadowAudioManager shadowAudioManager = Shadows.shadowOf(audioManager.getInternal());
        Controller<Unit> requestAudioFocusState = new Controller<>();
        List<String> result = new ArrayList<>();
        Observable<Unit> gotAudioFocusState = audioManager.requestAudioFocusWhen(
                requestAudioFocusState, AudioManager.STREAM_MUSIC, AudioManager.AUDIOFOCUS_GAIN);
        gotAudioFocusState.watch(() -> {
            result.add("Got audio focus");
            return () -> result.add("Lost audio focus");
        });
        requestAudioFocusState.set(Unit.unit());
        shadowAudioManager.getLastAudioFocusRequest().listener.onAudioFocusChange(
                AudioManager.AUDIOFOCUS_GAIN);
        assertThat(result, contains("Got audio focus"));
    }

    @Test
    public void testAudioFocusScopeDeactivatedWhenFocusRequestStateIsReset() {
        CastAudioManager audioManager =
                CastAudioManager.getAudioManager(RuntimeEnvironment.application);
        ShadowAudioManager shadowAudioManager = Shadows.shadowOf(audioManager.getInternal());
        Controller<Unit> requestAudioFocusState = new Controller<>();
        List<String> result = new ArrayList<>();
        Observable<Unit> gotAudioFocusState = audioManager.requestAudioFocusWhen(
                requestAudioFocusState, AudioManager.STREAM_MUSIC, AudioManager.AUDIOFOCUS_GAIN);
        gotAudioFocusState.watch(() -> {
            result.add("Got audio focus");
            return () -> result.add("Lost audio focus");
        });
        requestAudioFocusState.set(Unit.unit());
        shadowAudioManager.getLastAudioFocusRequest().listener.onAudioFocusChange(
                AudioManager.AUDIOFOCUS_GAIN);
        requestAudioFocusState.reset();
        assertThat(result, contains("Got audio focus", "Lost audio focus"));
    }

    @Test
    public void testAudioFocusScopeDeactivatedWhenAudioFocusIsLostButRequestStillActive() {
        CastAudioManager audioManager =
                CastAudioManager.getAudioManager(RuntimeEnvironment.application);
        ShadowAudioManager shadowAudioManager = Shadows.shadowOf(audioManager.getInternal());
        Controller<Unit> requestAudioFocusState = new Controller<>();
        List<String> result = new ArrayList<>();
        Observable<Unit> gotAudioFocusState = audioManager.requestAudioFocusWhen(
                requestAudioFocusState, AudioManager.STREAM_MUSIC, AudioManager.AUDIOFOCUS_GAIN);
        gotAudioFocusState.watch(() -> {
            result.add("Got audio focus");
            return () -> result.add("Lost audio focus");
        });
        requestAudioFocusState.set(Unit.unit());
        AudioManager.OnAudioFocusChangeListener listener =
                shadowAudioManager.getLastAudioFocusRequest().listener;
        listener.onAudioFocusChange(AudioManager.AUDIOFOCUS_GAIN);
        listener.onAudioFocusChange(AudioManager.AUDIOFOCUS_LOSS);
        assertThat(result, contains("Got audio focus", "Lost audio focus"));
    }

    @Test
    public void testAudioFocusScopeReactivatedWhenAudioFocusIsLostAndRegained() {
        CastAudioManager audioManager =
                CastAudioManager.getAudioManager(RuntimeEnvironment.application);
        ShadowAudioManager shadowAudioManager = Shadows.shadowOf(audioManager.getInternal());
        Controller<Unit> requestAudioFocusState = new Controller<>();
        List<String> result = new ArrayList<>();
        Observable<Unit> gotAudioFocusState = audioManager.requestAudioFocusWhen(
                requestAudioFocusState, AudioManager.STREAM_MUSIC, AudioManager.AUDIOFOCUS_GAIN);
        gotAudioFocusState.watch(() -> {
            result.add("Got audio focus");
            return () -> result.add("Lost audio focus");
        });
        requestAudioFocusState.set(Unit.unit());
        AudioManager.OnAudioFocusChangeListener listener =
                shadowAudioManager.getLastAudioFocusRequest().listener;
        listener.onAudioFocusChange(AudioManager.AUDIOFOCUS_GAIN);
        listener.onAudioFocusChange(AudioManager.AUDIOFOCUS_LOSS);
        listener.onAudioFocusChange(AudioManager.AUDIOFOCUS_GAIN);
        assertThat(result, contains("Got audio focus", "Lost audio focus", "Got audio focus"));
    }

    @Test
    public void testAudioFocusScopeNotActivatedIfRequestScopeNotActivated() {
        CastAudioManager audioManager =
                CastAudioManager.getAudioManager(RuntimeEnvironment.application);
        ShadowAudioManager shadowAudioManager = Shadows.shadowOf(audioManager.getInternal());
        Controller<Unit> requestAudioFocusState = new Controller<>();
        List<String> result = new ArrayList<>();
        Observable<Unit> gotAudioFocusState = audioManager.requestAudioFocusWhen(
                requestAudioFocusState, AudioManager.STREAM_MUSIC, AudioManager.AUDIOFOCUS_GAIN);
        gotAudioFocusState.watch(() -> {
            result.add("Got audio focus");
            return () -> result.add("Lost audio focus");
        });
        assertThat(result, emptyIterable());
    }
}
