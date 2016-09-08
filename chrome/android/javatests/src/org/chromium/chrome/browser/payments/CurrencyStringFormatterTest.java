// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import java.util.Locale;

/**
 * A lightweight integration test for CurrencyStringFormatter to run on an Android device.
 */
public class CurrencyStringFormatterTest extends InstrumentationTestCase {
    @SmallTest
    public void testCad() throws Exception {
        CurrencyStringFormatter formatter = new CurrencyStringFormatter("CAD", new Locale("en-US"));
        assertEquals("$5.00", formatter.format("5"));
    }

    @SmallTest
    public void testAzn() throws Exception {
        CurrencyStringFormatter formatter = new CurrencyStringFormatter("AZN", new Locale("en-US"));
        assertEquals("5.00", formatter.format("5"));
    }

    @SmallTest
    public void testBmd() throws Exception {
        CurrencyStringFormatter formatter = new CurrencyStringFormatter("BMD", new Locale("en-US"));
        assertEquals("5.00", formatter.format("5"));
    }

    @SmallTest
    public void testAud() throws Exception {
        CurrencyStringFormatter formatter = new CurrencyStringFormatter("AUD", new Locale("en-US"));
        assertEquals("$5.00", formatter.format("5"));
    }

    @SmallTest
    public void testBzd() throws Exception {
        CurrencyStringFormatter formatter = new CurrencyStringFormatter("BZD", new Locale("en-US"));
        assertEquals("5.00", formatter.format("5"));
    }

    @SmallTest
    public void testClp() throws Exception {
        CurrencyStringFormatter formatter = new CurrencyStringFormatter("CLP", new Locale("en-US"));
        assertEquals("5", formatter.format("5"));
    }

    @SmallTest
    public void testLrd() throws Exception {
        CurrencyStringFormatter formatter = new CurrencyStringFormatter("LRD", new Locale("en-US"));
        assertEquals("5.00", formatter.format("5"));
    }

    @SmallTest
    public void testNio() throws Exception {
        CurrencyStringFormatter formatter = new CurrencyStringFormatter("NIO", new Locale("en-US"));
        assertEquals("5.00", formatter.format("5"));
    }

    @SmallTest
    public void testHrk() throws Exception {
        CurrencyStringFormatter formatter = new CurrencyStringFormatter("HRK", new Locale("en-US"));
        assertEquals("5.00", formatter.format("5"));
    }

    @SmallTest
    public void testRub() throws Exception {
        CurrencyStringFormatter formatter = new CurrencyStringFormatter("RUB", new Locale("en-US"));
        assertEquals("5.00", formatter.format("5"));
    }

    @SmallTest
    public void testEur() throws Exception {
        CurrencyStringFormatter formatter = new CurrencyStringFormatter("EUR", new Locale("en-US"));
        assertEquals("€5.00", formatter.format("5"));
    }

    @SmallTest
    public void testPkr() throws Exception {
        CurrencyStringFormatter formatter = new CurrencyStringFormatter("PKR", new Locale("en-US"));
        assertEquals("5", formatter.format("5"));
    }

    @SmallTest
    public void testCurrency6Chars() throws Exception {
        CurrencyStringFormatter formatter =
                new CurrencyStringFormatter("ABCDEF", new Locale("en-US"));
        assertEquals("ABCDEF", formatter.getFormattedCurrencyCode());
    }

    @SmallTest
    public void testCurrencyEllipsized() throws Exception {
        CurrencyStringFormatter formatter =
                new CurrencyStringFormatter("ABCDEFG", new Locale("en-US"));
        // Currency code more than 6 character is formatted to first 5 characters and ellipsis.
        // "\u2026" is unicode for ellipsis.
        assertEquals("ABCDE\u2026", formatter.getFormattedCurrencyCode());
    }

    @SmallTest
    public void testCurrencyEmpty() throws Exception {
        CurrencyStringFormatter formatter = new CurrencyStringFormatter("", new Locale("en-US"));
        assertEquals("", formatter.getFormattedCurrencyCode());
    }
}
