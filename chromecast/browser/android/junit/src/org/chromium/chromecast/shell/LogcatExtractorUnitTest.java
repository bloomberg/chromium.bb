// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.Arrays;
import java.util.List;

/**
 * junit tests for {@link LogcatExtractor}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LogcatExtractorUnitTest {
    private static final int MAX_LINES = 5;

    @Test
    public void testElideEmail() {
        String original = "email me at someguy@mailservice.com";
        String expected = "email me at XXX@EMAIL.ELIDED";
        assertEquals(expected, LogcatExtractor.elideEmail(original));
    }

    @Test
    public void testElideUrl() {
        String original = "file bugs at crbug.com";
        String expected = "file bugs at HTTP://WEBADDRESS.ELIDED";
        assertEquals(expected, LogcatExtractor.elideUrl(original));
    }

    @Test
    public void testElideUrl2() {
        String original = "exception at org.chromium.chrome.browser.crash.LogcatExtractorUnitTest";
        assertEquals(original, LogcatExtractor.elideUrl(original));
    }

    @Test
    public void testElideUrl3() {
        String original = "file bugs at crbug.com or code.google.com";
        String expected = "file bugs at HTTP://WEBADDRESS.ELIDED or HTTP://WEBADDRESS.ELIDED";
        assertEquals(expected, LogcatExtractor.elideUrl(original));
    }

    @Test
    public void testElideUrl4() {
        String original = "test shorturl.com !!!";
        String expected = "test HTTP://WEBADDRESS.ELIDED !!!";
        assertEquals(expected, LogcatExtractor.elideUrl(original));
    }

    @Test
    public void testElideUrl5() {
        String original = "test just.the.perfect.len.url !!!";
        String expected = "test HTTP://WEBADDRESS.ELIDED !!!";
        assertEquals(expected, LogcatExtractor.elideUrl(original));
    }

    @Test
    public void testElideUrl6() {
        String original = "test a.very.very.very.very.very.long.url !!!";
        String expected = "test HTTP://WEBADDRESS.ELIDED !!!";
        assertEquals(expected, LogcatExtractor.elideUrl(original));
    }

    @Test
    public void testElideUrl7() {
        String original = " at android.content.Intent \n at java.util.ArrayList";
        assertEquals(original, LogcatExtractor.elideUrl(original));
    }

    @Test
    public void testDontElideFileSuffixes() {
        String original = "chromium_android_linker.so";
        assertEquals(original, LogcatExtractor.elideUrl(original));
    }

    @Test
    public void testElideIp() {
        String original = "traceroute 127.0.0.1";
        String expected = "traceroute 1.2.3.4";
        assertEquals(expected, LogcatExtractor.elideIp(original));
    }

    @Test
    public void testElideMac1() {
        String original = "MAC: AB-AB-AB-AB-AB-AB";
        String expected = "MAC: 01:23:45:67:89:AB";
        assertEquals(expected, LogcatExtractor.elideMac(original));
    }

    @Test
    public void testElideMac2() {
        String original = "MAC: AB:AB:AB:AB:AB:AB";
        String expected = "MAC: 01:23:45:67:89:AB";
        assertEquals(expected, LogcatExtractor.elideMac(original));
    }

    @Test
    public void testElideConsole() {
        String original = "I/chromium(123): [INFO:CONSOLE(2)] hello!";
        String expected = "I/chromium(123): [ELIDED:CONSOLE(0)] ELIDED CONSOLE MESSAGE";
        assertEquals(expected, LogcatExtractor.elideConsole(original));
    }

    @Test
    public void testLogTagNotElided() {
        List<String> original = Arrays.asList(new String[] {"I/cr_FooBar(123): Some message"});
        assertEquals(original, LogcatExtractor.processLogcat(original));
    }

    @Test
    public void testLogCastNotElided() {
        String original = "09-08 11:52:12.569  4406  4406 I chromium: "
                + "Cast.Discovery.Mdns.Request.AppId.In=25";
        assertEquals(original, LogcatExtractor.elideUrl(original));
    }

    @Test
    public void testLogAndroidNotElided() {
        String original = "09-08 11:54:38.053  4406  4406 D cr_AvSettingsAndroid:"
                + "[AvSettingsAndroid.java:180] HDMI plug update: "
                + "action=android.media.action.HDMI_AUDIO_PLUG, plug=1";
        assertEquals(original, LogcatExtractor.elideUrl(original));
    }

    @Test
    public void testLogJavaNotElided() {
        String original = "09-08 11:54:38.053  4406  4406 D cr_AvSettingsAndroid: "
                + " [AvSettingsAndroid.java:187] Max channel count = 8";
        assertEquals(original, LogcatExtractor.elideUrl(original));
    }

    @Test
    public void testLogCcNotElided() {
        String original = "09-08 11:59:12.335  4406  4443 I chromium: "
                + "[4406:4443:INFO:wifi_util.cc(113)]   No peers:";
        assertEquals(original, LogcatExtractor.elideUrl(original));
    }
}