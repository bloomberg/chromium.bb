// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;

/**
 * Part of the test suite for the Java Bridge. This test tests the
 * use of fields.
 */
public class JavaBridgeFieldsTest extends JavaBridgeTestBase {
    private class TestObject extends Controller {
        private String mStringValue;

        // These methods are used to control the test.
        public synchronized void setStringValue(String x) {
            mStringValue = x;
            notifyResultIsReady();
        }
        public synchronized String waitForStringValue() {
            waitForResult();
            return mStringValue;
        }

        public boolean booleanField = true;
        public byte byteField = 42;
        public char charField = '\u002A';
        public short shortField = 42;
        public int intField = 42;
        public long longField = 42L;
        public float floatField = 42.0f;
        public double doubleField = 42.0;
        public String stringField = "foo";
        public Object objectField = new Object();
        public CustomType customTypeField = new CustomType();
    }

    // A custom type used when testing passing objects.
    private static class CustomType {
    }

    TestObject mTestObject;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTestObject = new TestObject();
        setUpContentView(mTestObject, "testObject");
    }

    // Note that this requires that we can pass a JavaScript string to Java.
    protected String executeJavaScriptAndGetStringResult(String script) throws Throwable {
        executeJavaScript("testObject.setStringValue(" + script + ");");
        return mTestObject.waitForStringValue();
    }

    // The Java bridge does not provide access to fields.
    // FIXME: Consider providing support for this. See See b/4408210.
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testFieldTypes() throws Throwable {
        assertEquals("undefined",
                executeJavaScriptAndGetStringResult("typeof testObject.booleanField"));
        assertEquals("undefined",
                executeJavaScriptAndGetStringResult("typeof testObject.byteField"));
        assertEquals("undefined",
                executeJavaScriptAndGetStringResult("typeof testObject.charField"));
        assertEquals("undefined",
                executeJavaScriptAndGetStringResult("typeof testObject.shortField"));
        assertEquals("undefined",
                executeJavaScriptAndGetStringResult("typeof testObject.intField"));
        assertEquals("undefined",
                executeJavaScriptAndGetStringResult("typeof testObject.longField"));
        assertEquals("undefined",
                executeJavaScriptAndGetStringResult("typeof testObject.floatField"));
        assertEquals("undefined",
                executeJavaScriptAndGetStringResult("typeof testObject.doubleField"));
        assertEquals("undefined",
                executeJavaScriptAndGetStringResult("typeof testObject.objectField"));
        assertEquals("undefined",
                executeJavaScriptAndGetStringResult("typeof testObject.stringField"));
        assertEquals("undefined",
                executeJavaScriptAndGetStringResult("typeof testObject.customTypeField"));
    }
}
