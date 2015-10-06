// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

import static org.chromium.chrome.browser.crash.LogcatExtractionCallable.BEGIN_MICRODUMP;
import static org.chromium.chrome.browser.crash.LogcatExtractionCallable.END_MICRODUMP;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;

import android.text.TextUtils;

import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import java.io.BufferedReader;
import java.io.StringReader;
import java.util.Arrays;
import java.util.LinkedList;
import java.util.List;

/**
 * junit tests for {@link LogcatExtractionCallable}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LogcatExtractionCallableTest {
    private static final int MAX_LINES = 5;

    @Test
    public void testElideEmail() {
        String original = "email me at someguy@mailservice.com";
        String expected = "email me at XXX@EMAIL.ELIDED";
        assertEquals(expected, LogcatExtractionCallable.elideEmail(original));
    }

    @Test
    public void testElideUrl() {
        String original = "file bugs at crbug.com";
        String expected = "file bugs at HTTP://WEBADDRESS.ELIDED";
        assertEquals(expected, LogcatExtractionCallable.elideUrl(original));
    }

    @Test
    public void testElideUrl2() {
        String original =
                "exception at org.chromium.chrome.browser.crash.LogcatExtractionCallableTest";
        assertEquals(original, LogcatExtractionCallable.elideUrl(original));
    }

    @Test
    public void testElideUrl3() {
        String original = "file bugs at crbug.com or code.google.com";
        String expected = "file bugs at HTTP://WEBADDRESS.ELIDED or HTTP://WEBADDRESS.ELIDED";
        assertEquals(expected, LogcatExtractionCallable.elideUrl(original));
    }

    @Test
    public void testElideUrl4() {
        String original = "test shorturl.com !!!";
        String expected = "test HTTP://WEBADDRESS.ELIDED !!!";
        assertEquals(expected, LogcatExtractionCallable.elideUrl(original));
    }

    @Test
    public void testElideUrl5() {
        String original = "test just.the.perfect.len.url !!!";
        String expected = "test HTTP://WEBADDRESS.ELIDED !!!";
        assertEquals(expected, LogcatExtractionCallable.elideUrl(original));
    }

    @Test
    public void testElideUrl6() {
        String original = "test a.very.very.very.very.very.long.url !!!";
        String expected = "test HTTP://WEBADDRESS.ELIDED !!!";
        assertEquals(expected, LogcatExtractionCallable.elideUrl(original));
    }

    @Test
    public void testElideUrl7() {
        String original = " at android.content.Intent \n at java.util.ArrayList";
        assertEquals(original, LogcatExtractionCallable.elideUrl(original));
    }

    @Test
    public void testDontElideFileSuffixes() {
        String original = "chromium_android_linker.so";
        assertEquals(original, LogcatExtractionCallable.elideUrl(original));
    }

    @Test
    public void testElideIp() {
        String original = "traceroute 127.0.0.1";
        String expected = "traceroute 1.2.3.4";
        assertEquals(expected, LogcatExtractionCallable.elideIp(original));
    }

    @Test
    public void testElideMac1() {
        String original = "MAC: AB-AB-AB-AB-AB-AB";
        String expected = "MAC: 01:23:45:67:89:AB";
        assertEquals(expected, LogcatExtractionCallable.elideMac(original));
    }

    @Test
    public void testElideMac2() {
        String original = "MAC: AB:AB:AB:AB:AB:AB";
        String expected = "MAC: 01:23:45:67:89:AB";
        assertEquals(expected, LogcatExtractionCallable.elideMac(original));
    }

    @Test
    public void testElideConsole() {
        String original = "I/chromium(123): [INFO:CONSOLE(2)] hello!";
        String expected = "I/chromium(123): [ELIDED:CONSOLE(0)] ELIDED CONSOLE MESSAGE";
        assertEquals(expected, LogcatExtractionCallable.elideConsole(original));
    }

    @Test
    public void testLogTagNotElided() {
        List<String> original = Arrays.asList(new String[] {"I/cr_FooBar(123): Some message"});
        assertEquals(original, LogcatExtractionCallable.processLogcat(original));
    }

    @Test
    public void testLogcatEmpty() {
        final String original = "";
        List<String> expected = new LinkedList<>();
        List<String> logcat = null;
        try {
            logcat = LogcatExtractionCallable.extractLogcatFromReader(
                    new BufferedReader(new StringReader(original)), MAX_LINES);
        } catch (Exception e) {
            fail(e.toString());
        }
        assertArrayEquals(expected.toArray(), logcat.toArray());
    }

    @Test
    public void testLogcatWithoutBeginOrEnd_smallLogcat() {
        final List<String> original = Arrays.asList("Line 1", "Line 2", "Line 3", "Line 4",
                "Line 5");
        assertLogcatLists(original, original);
    }

    @Test
    public void testLogcatWithoutBeginOrEnd_largeLogcat() {
        final List<String> original = Arrays.asList("Line 1", "Line 2", "Line 3", "Line 4",
                "Line 5", "Redundant Line 1", "Redundant Line 2");
        final List<String> expected = Arrays.asList("Line 1", "Line 2", "Line 3", "Line 4",
                "Line 5");
        assertLogcatLists(expected, original);
    }

    @Test
    public void testLogcatBeginsWithBegin() {
        final List<String> original = Arrays.asList(BEGIN_MICRODUMP, "a", "b", "c", "d", "e");
        assertLogcatLists(new LinkedList<String>(), original);
    }

    @Test
    public void testLogcatWithBegin() {
        final List<String> original = Arrays.asList("Line 1", "Line 2", BEGIN_MICRODUMP, "a",
                "b", "c", "d", "e");
        final List<String> expected = Arrays.asList("Line 1", "Line 2");
        assertLogcatLists(expected, original);
    }

    @Test
    public void testLogcatWithEnd() {
        final List<String> original = Arrays.asList("Line 1", "Line 2", END_MICRODUMP);
        assertLogcatLists(new LinkedList<String>(), original);
    }

    @Test
    public void testLogcatWithBeginAndEnd_smallLogcat() {
        final List<String> original = Arrays.asList("Line 1", "Line 2", BEGIN_MICRODUMP, "a", "b",
                "c", "d", "e", END_MICRODUMP);
        final List<String> expected = Arrays.asList("Line 1", "Line 2");
        assertLogcatLists(expected, original);
    }

    @Test
    public void testLogcatWithBeginAndEnd_largeLogcat() {
        final List<String> original = Arrays.asList("Line 1", "Line 2", BEGIN_MICRODUMP, "a", "b",
                "c", "d", "e", END_MICRODUMP, "Line 3", "Line 4");
        final List<String> expected = Arrays.asList("Line 1", "Line 2", "Line 3", "Line 4");
        assertLogcatLists(expected, original);
    }

    @Test
    public void testLogcatWithEndAndBegin_smallLogcat() {
        final List<String> original = Arrays.asList(END_MICRODUMP, "Line 1", "Line 2",
                BEGIN_MICRODUMP, "a", "b", "c", "d", "e");
        final List<String> expected = Arrays.asList("Line 1", "Line 2");
        assertLogcatLists(expected, original);
    }

    @Test
    public void testLogcatWithEndAndBegin_largeLogcat() {
        final List<String> original = Arrays.asList(END_MICRODUMP, "Line 1", "Line 2",
                BEGIN_MICRODUMP, "a", "b", "c", "d", "e", END_MICRODUMP, "Line 3", "Line 4");
        final List<String> expected = Arrays.asList("Line 1", "Line 2", "Line 3", "Line 4");
        assertLogcatLists(expected, original);
    }

    private void assertLogcatLists(List<String> expected, List<String> original) {
        List<String> actualLogcat = null;
        String combinedLogcat = TextUtils.join("\n", original);
        try {
            //simulate a file reader to test whether the extraction process
            //successfully strips microdump from logcat
            actualLogcat = LogcatExtractionCallable.extractLogcatFromReader(
                    new BufferedReader(new StringReader(combinedLogcat)), MAX_LINES);
        } catch (Exception e) {
            fail(e.toString());
        }
        assertArrayEquals(expected.toArray(), actualLogcat.toArray());
    }
}
