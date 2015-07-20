// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

import android.test.suitebuilder.annotation.SmallTest;

/**
 * Unittests for {@link LogcatExtractionCallable}.
 */
public class LogcatExtractionCallableTest extends CrashTestCase {

    @SmallTest
    public void testElideEmail() {
        String original = "email me at someguy@mailservice.com";
        String expected = "email me at XXX@EMAIL.ELIDED";
        assertEquals(expected, LogcatExtractionCallable.elideEmail(original));
    }

    @SmallTest
    public void testElideUrl() {
        String original = "file bugs at crbug.com";
        String expected = "file bugs at HTTP://WEBADDRESS.ELIDED";
        assertEquals(expected, LogcatExtractionCallable.elideUrl(original));
    }

    @SmallTest
    public void testElideUrl2() {
        String original =
                "exception at org.chromium.chrome.browser.crash.LogcatExtractionCallableTest";
        assertEquals(original, LogcatExtractionCallable.elideUrl(original));
    }

    @SmallTest
    public void testElideUrl3() {
        String original = "file bugs at crbug.com or code.google.com";
        String expected = "file bugs at HTTP://WEBADDRESS.ELIDED or HTTP://WEBADDRESS.ELIDED";
        assertEquals(expected, LogcatExtractionCallable.elideUrl(original));
    }

    @SmallTest
    public void testElideUrl4() {
        String original = "test shorturl.com !!!";
        String expected = "test HTTP://WEBADDRESS.ELIDED !!!";
        assertEquals(expected, LogcatExtractionCallable.elideUrl(original));
    }

    @SmallTest
    public void testElideUrl5() {
        String original = "test just.the.perfect.len.url !!!";
        String expected = "test HTTP://WEBADDRESS.ELIDED !!!";
        assertEquals(expected, LogcatExtractionCallable.elideUrl(original));
    }

    @SmallTest
    public void testElideUrl6() {
        String original = "test a.very.very.very.very.very.long.url !!!";
        String expected = "test HTTP://WEBADDRESS.ELIDED !!!";
        assertEquals(expected, LogcatExtractionCallable.elideUrl(original));
    }

    @SmallTest
    public void testElideIp() {
        String original = "traceroute 127.0.0.1";
        String expected = "traceroute 1.2.3.4";
        assertEquals(expected, LogcatExtractionCallable.elideIp(original));
    }

    @SmallTest
    public void testElideMac1() {
        String original = "MAC: AB-AB-AB-AB-AB-AB";
        String expected = "MAC: 01:23:45:67:89:AB";
        assertEquals(expected, LogcatExtractionCallable.elideMac(original));
    }

    @SmallTest
    public void testElideMac2() {
        String original = "MAC: AB:AB:AB:AB:AB:AB";
        String expected = "MAC: 01:23:45:67:89:AB";
        assertEquals(expected, LogcatExtractionCallable.elideMac(original));
    }

    @SmallTest
    public void testElideConsole() {
        String original = "I/chromium(123): [INFO:CONSOLE(2)] hello!";
        String expected = "I/chromium(123): [ELIDED:CONSOLE(0)] ELIDED CONSOLE MESSAGE";
        assertEquals(expected, LogcatExtractionCallable.elideConsole(original));
    }
}
