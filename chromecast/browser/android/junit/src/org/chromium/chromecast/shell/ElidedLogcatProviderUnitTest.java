// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

// TODO(sandv): Add test cases as need arises.
/**
 * Tests for LogcatProvider.
 *
 * Full testing of elision of PII is done in LogcatElisionUnitTest.
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class ElidedLogcatProviderUnitTest {
    @Test
    public void testEmptyLogcat() {
        List<String> lines = new ArrayList<String>();
        assertEquals("", ElidedLogcatProvider.elideLogcat(lines));
    }

    @Test
    public void testLogcatOfEmptyString() {
        List<String> lines = Arrays.asList("");
        assertEquals("\n", ElidedLogcatProvider.elideLogcat(lines));
    }

    @Test
    public void testJoinsLines() {
        List<String> lines = Arrays.asList("a", "b", "c");
        assertEquals("a\nb\nc\n", ElidedLogcatProvider.elideLogcat(lines));
    }

    @Test
    public void testElidesPii() {
        List<String> lines = Arrays.asList("email me at someguy@mailservice.com",
                "file bugs at crbug.com", "at android.content.Intent", "at java.util.ArrayList",
                "mac address: AB-AB-AB-AB-AB-AB");
        String elided = ElidedLogcatProvider.elideLogcat(lines);
        // PII like email addresses, web addresses, and MAC addresses are elided.
        assertThat(elided, not(containsString("someguy@mailservice.com")));
        assertThat(elided, not(containsString("crbug.com")));
        assertThat(elided, not(containsString("AB-AB-AB-AB-AB-AB")));
        // Tags for class names relevant for debugging should not be elided.
        assertThat(elided, containsString("android.content.Intent"));
        assertThat(elided, containsString("java.util.ArrayList"));
    }
}
