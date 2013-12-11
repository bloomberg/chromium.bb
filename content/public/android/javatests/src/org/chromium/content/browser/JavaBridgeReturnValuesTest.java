// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;

/**
 * Part of the test suite for the Java Bridge. This test checks that we correctly convert Java
 * values to JavaScript values when returning them from the methods of injected Java objects.
 *
 * The conversions should follow
 * http://jdk6.java.net/plugin2/liveconnect/#JS_JAVA_CONVERSIONS. Places in
 * which the implementation differs from the spec are marked with
 * LIVECONNECT_COMPLIANCE.
 * FIXME: Consider making our implementation more compliant, if it will not
 * break backwards-compatibility. See b/4408210.
 */
public class JavaBridgeReturnValuesTest extends JavaBridgeTestBase {
    // An instance of this class is injected into the page to test returning
    // Java values to JavaScript.
    private class TestObject extends Controller {
        private String mStringValue;
        private boolean mBooleanValue;

        // These four methods are used to control the test.
        public synchronized void setStringValue(String x) {
            mStringValue = x;
            notifyResultIsReady();
        }
        public synchronized String waitForStringValue() {
            waitForResult();
            return mStringValue;
        }
        public synchronized void setBooleanValue(boolean x) {
            mBooleanValue = x;
            notifyResultIsReady();
        }
        public synchronized boolean waitForBooleanValue() {
            waitForResult();
            return mBooleanValue;
        }

        public boolean getBooleanValue() {
            return true;
        }
        public byte getByteValue() {
            return 42;
        }
        public char getCharValue() {
            return '\u002A';
        }
        public short getShortValue() {
            return 42;
        }
        public int getIntValue() {
            return 42;
        }
        public long getLongValue() {
            return 42L;
        }
        public float getFloatValue() {
            return 42.1f;
        }
        public float getFloatValueNoDecimal() {
            return 42.0f;
        }
        public double getDoubleValue() {
            return 42.1;
        }
        public double getDoubleValueNoDecimal() {
            return 42.0;
        }
        public String getStringValue() {
            return "foo";
        }
        public String getEmptyStringValue() {
            return "";
        }
        public String getNullStringValue() {
            return null;
        }
        public Object getObjectValue() {
            return new Object();
        }
        public Object getNullObjectValue() {
            return null;
        }
        public CustomType getCustomTypeValue() {
            return new CustomType();
        }
        public void getVoidValue() {
        }
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

    // Note that this requires that we can pass a JavaScript boolean to Java.
    private boolean executeJavaScriptAndGetBooleanResult(String script) throws Throwable {
        executeJavaScript("testObject.setBooleanValue(" + script + ");");
        return mTestObject.waitForBooleanValue();
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testMethodReturnTypes() throws Throwable {
        assertEquals("boolean",
                executeJavaScriptAndGetStringResult("typeof testObject.getBooleanValue()"));
        assertEquals("number",
                executeJavaScriptAndGetStringResult("typeof testObject.getByteValue()"));
        // char values are returned to JavaScript as numbers.
        assertEquals("number",
                executeJavaScriptAndGetStringResult("typeof testObject.getCharValue()"));
        assertEquals("number",
                executeJavaScriptAndGetStringResult("typeof testObject.getShortValue()"));
        assertEquals("number",
                executeJavaScriptAndGetStringResult("typeof testObject.getIntValue()"));
        assertEquals("number",
                executeJavaScriptAndGetStringResult("typeof testObject.getLongValue()"));
        assertEquals("number",
                executeJavaScriptAndGetStringResult("typeof testObject.getFloatValue()"));
        assertEquals("number",
                executeJavaScriptAndGetStringResult("typeof testObject.getFloatValueNoDecimal()"));
        assertEquals("number",
                executeJavaScriptAndGetStringResult("typeof testObject.getDoubleValue()"));
        assertEquals("number",
                executeJavaScriptAndGetStringResult("typeof testObject.getDoubleValueNoDecimal()"));
        assertEquals("string",
                executeJavaScriptAndGetStringResult("typeof testObject.getStringValue()"));
        assertEquals("string",
                executeJavaScriptAndGetStringResult("typeof testObject.getEmptyStringValue()"));
        // LIVECONNECT_COMPLIANCE: This should have type object.
        assertEquals("undefined",
                executeJavaScriptAndGetStringResult("typeof testObject.getNullStringValue()"));
        assertEquals("object",
                executeJavaScriptAndGetStringResult("typeof testObject.getObjectValue()"));
        assertEquals("object",
                executeJavaScriptAndGetStringResult("typeof testObject.getNullObjectValue()"));
        assertEquals("object",
                executeJavaScriptAndGetStringResult("typeof testObject.getCustomTypeValue()"));
        assertEquals("undefined",
                executeJavaScriptAndGetStringResult("typeof testObject.getVoidValue()"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testMethodReturnValues() throws Throwable {
        // We do the string comparison in JavaScript, to avoid relying on the
        // coercion algorithm from JavaScript to Java.
        assertTrue(executeJavaScriptAndGetBooleanResult("testObject.getBooleanValue()"));
        assertTrue(executeJavaScriptAndGetBooleanResult("42 === testObject.getByteValue()"));
        // char values are returned to JavaScript as numbers.
        assertTrue(executeJavaScriptAndGetBooleanResult("42 === testObject.getCharValue()"));
        assertTrue(executeJavaScriptAndGetBooleanResult("42 === testObject.getShortValue()"));
        assertTrue(executeJavaScriptAndGetBooleanResult("42 === testObject.getIntValue()"));
        assertTrue(executeJavaScriptAndGetBooleanResult("42 === testObject.getLongValue()"));
        assertTrue(executeJavaScriptAndGetBooleanResult(
                "Math.abs(42.1 - testObject.getFloatValue()) < 0.001"));
        assertTrue(executeJavaScriptAndGetBooleanResult(
                "42.0 === testObject.getFloatValueNoDecimal()"));
        assertTrue(executeJavaScriptAndGetBooleanResult(
                "Math.abs(42.1 - testObject.getDoubleValue()) < 0.001"));
        assertTrue(executeJavaScriptAndGetBooleanResult(
                "42.0 === testObject.getDoubleValueNoDecimal()"));
        assertEquals("foo", executeJavaScriptAndGetStringResult("testObject.getStringValue()"));
        assertEquals("", executeJavaScriptAndGetStringResult("testObject.getEmptyStringValue()"));
        assertTrue(executeJavaScriptAndGetBooleanResult("undefined === testObject.getVoidValue()"));
    }
}
