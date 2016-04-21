// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router.cast;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import org.chromium.base.test.util.Feature;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

/**
 * Robolectric tests for {@link MediaSource} class.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MediaSourceTest {

    @Test
    @Feature({"MediaRouter"})
    public void testCorrectSourceId() {
        final String sourceId =
                "https://example.com/path?query"
                + "#__castAppId__=ABCD1234(video_out,audio_out)"
                + "/__castClientId__=1234567890"
                + "/__castAutoJoinPolicy__=tab_and_origin_scoped";

        MediaSource source = MediaSource.from(sourceId);
        assertNotNull(source);
        assertEquals("ABCD1234", source.getApplicationId());

        assertNotNull(source.getCapabilities());
        assertEquals(2, source.getCapabilities().length);
        assertEquals("video_out", source.getCapabilities()[0]);
        assertEquals("audio_out", source.getCapabilities()[1]);

        assertEquals("1234567890", source.getClientId());
        assertEquals("tab_and_origin_scoped", source.getAutoJoinPolicy());

        assertEquals(sourceId, source.getUrn());
    }

    @Test
    @Feature({"MediaRouter"})
    public void testNoFragment() {
        assertNull(MediaSource.from("https://example.com/path?query"));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testEmptyFragment() {
        assertNull(MediaSource.from("https://example.com/path?query#"));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testNoAppId() {
        assertNull(MediaSource.from("https://example.com/path?query#fragment"));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testNoValidAppId() {
        // Invalid app id needs to indicate no availability so {@link MediaSource} needs to be
        // created.
        MediaSource empty = MediaSource.from("https://example.com/path?query#__castAppId__=");
        assertNotNull(empty);
        assertEquals("", empty.getApplicationId());

        MediaSource invalid = MediaSource.from(
                "https://example.com/path?query#__castAppId__=INVALID-APP-ID");
        assertNotNull(invalid);
        assertEquals("INVALID-APP-ID", invalid.getApplicationId());
    }

    @Test
    @Feature({"MediaRouter"})
    public void testNoCapabilitiesSuffix() {
        assertNull(MediaSource.from("https://example.com/path?query#__castAppId__=A("));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testCapabilitiesEmpty() {
        assertNull(MediaSource.from("https://example.com/path?query#__castAppId__=A()"));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testInvalidCapability() {
        assertNull(MediaSource.from("https://example.com/path?query#__castAppId__=A(a)"));
        assertNull(MediaSource.from("https://example.com/path?query#__castAppId__=A(video_in,b)"));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testOptionalParameters() {
        MediaSource source = MediaSource.from("https://example.com/path?query#__castAppId__=A");
        assertNotNull(source);
        assertEquals("A", source.getApplicationId());

        assertNull(source.getCapabilities());
        assertNull(source.getClientId());
        assertEquals("tab_and_origin_scoped", source.getAutoJoinPolicy());
    }
}
