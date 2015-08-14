// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.AdvancedMockContext;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Unittests for {@link LogcatExtractionCallable}.
 */
public class LogcatExtractionCallableTest extends CrashTestCase {
    private File mCrashDir;

    protected void setUp() throws Exception {
        super.setUp();
        mCrashDir = new CrashFileManager(mCacheDir).getCrashDirectory();
    }

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
    public void testElideUrl7() {
        String original = " at android.content.Intent \n at java.util.ArrayList";
        assertEquals(original, LogcatExtractionCallable.elideUrl(original));
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

    @MediumTest
    public void testExtractToFile() {
        final AtomicInteger numServiceStarts = new AtomicInteger(0);
        final String logContent = "some random log content";
        final Intent testIntent = new Intent();
        File dmp1 = new File(mCrashDir, "test1.dmp");
        File dmp2 = new File(mCrashDir, "test2.dmp");
        File dmp3 = new File(mCrashDir, "test3.dmp");
        final String[] dumps = new String[] {dmp1.getName(), dmp2.getName(), dmp3.getName()};

        Context testContext = new AdvancedMockContext(getInstrumentation().getTargetContext()) {
            @Override
            public ComponentName startService(Intent intentToCheck) {
                assertTrue("We should have no more than 3 intents created",
                        numServiceStarts.incrementAndGet() <= dumps.length);
                Intent redirectIntent = intentToCheck.getParcelableExtra("redirect_intent");

                // Only the very last one will have a non-null redirect_intent.
                if (numServiceStarts.get() == dumps.length) {
                    // And it should be == to the one we pass into the constructor.
                    assertSame(redirectIntent, testIntent);
                } else {
                    assertNull(redirectIntent);
                }
                return new ComponentName(
                        getPackageName(), LogcatExtractionCallable.class.getName());
            }
        };

        LogcatExtractionCallable callable = new LogcatExtractionCallable(
                testContext, dumps, testIntent) {
            @Override
            protected List<String> getLogcat() throws IOException, InterruptedException {
                    List<String> result = new ArrayList<String>();
                    result.add(logContent);
                    return result;
            }
        };

        callable.call();

        assertFileContent("test1.logcat", logContent);
        assertFileContent("test2.logcat", logContent);
        assertFileContent("test3.logcat", logContent);
    }

    private void assertFileContent(String fileName, String content) {
        try {
            File logfile = new File(mCrashDir, fileName);
            assertTrue("Logfile does not exist!", logfile.exists());
            BufferedReader input = new BufferedReader(new FileReader(logfile));
            StringBuffer sb = new StringBuffer();
            String line = null;
            while ((line = input.readLine()) != null) {
                sb.append(line);
            }
            input.close();
            assertEquals("Content not matched!", content, sb.toString());
        } catch (Exception e) {
            fail(e.toString());
        }
    }
}
