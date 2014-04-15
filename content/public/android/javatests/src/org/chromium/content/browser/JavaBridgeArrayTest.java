// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;

/**
 * Part of the test suite for the Java Bridge. This class tests the general use of arrays.
 *
 * The conversions should follow
 * http://jdk6.java.net/plugin2/liveconnect/#JS_JAVA_CONVERSIONS. Places in
 * which the implementation differs from the spec are marked with
 * LIVECONNECT_COMPLIANCE.
 * FIXME: Consider making our implementation more compliant, if it will not
 * break backwards-compatibility. See b/4408210.
 */
public class JavaBridgeArrayTest extends JavaBridgeTestBase {
    private class TestObject extends Controller {
        private boolean mBooleanValue;
        private int mIntValue;
        private String mStringValue;

        private int[] mIntArray;
        private int[][] mIntIntArray;

        private boolean mWasArrayMethodCalled;

        public synchronized void setBooleanValue(boolean x) {
            mBooleanValue = x;
            notifyResultIsReady();
        }
        public synchronized void setIntValue(int x) {
            mIntValue = x;
            notifyResultIsReady();
        }
        public synchronized void setStringValue(String x) {
            mStringValue = x;
            notifyResultIsReady();
        }

        public synchronized boolean waitForBooleanValue() {
            waitForResult();
            return mBooleanValue;
        }
        public synchronized int waitForIntValue() {
            waitForResult();
            return mIntValue;
        }
        public synchronized String waitForStringValue() {
            waitForResult();
            return mStringValue;
        }

        public synchronized void setIntArray(int[] x) {
            mIntArray = x;
            notifyResultIsReady();
        }
        public synchronized void setIntIntArray(int[][] x) {
            mIntIntArray = x;
            notifyResultIsReady();
        }

        public synchronized int[] waitForIntArray() {
            waitForResult();
            return mIntArray;
        }
        public synchronized int[][] waitForIntIntArray() {
            waitForResult();
            return mIntIntArray;
        }

        public synchronized int[] arrayMethod() {
            mWasArrayMethodCalled = true;
            return new int[] {42, 43, 44};
        }

        public synchronized boolean wasArrayMethodCalled() {
            return mWasArrayMethodCalled;
        }
    }

    private TestObject mTestObject;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTestObject = new TestObject();
        setUpContentView(mTestObject, "testObject");
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testArrayLength() throws Throwable {
        executeJavaScript("testObject.setIntArray([42, 43, 44]);");
        int[] result = mTestObject.waitForIntArray();
        assertEquals(3, result.length);
        assertEquals(42, result[0]);
        assertEquals(43, result[1]);
        assertEquals(44, result[2]);
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassNull() throws Throwable {
        executeJavaScript("testObject.setIntArray(null);");
        assertNull(mTestObject.waitForIntArray());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassUndefined() throws Throwable {
        executeJavaScript("testObject.setIntArray(undefined);");
        assertNull(mTestObject.waitForIntArray());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassEmptyArray() throws Throwable {
        executeJavaScript("testObject.setIntArray([]);");
        assertEquals(0, mTestObject.waitForIntArray().length);
    }

    // Note that this requires being able to pass a string from JavaScript to
    // Java.
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassArrayToStringMethod() throws Throwable {
        // LIVECONNECT_COMPLIANCE: Should call toString() on array.
        executeJavaScript("testObject.setStringValue([42, 42, 42]);");
        assertEquals("undefined", mTestObject.waitForStringValue());
    }

    // Note that this requires being able to pass an integer from JavaScript to
    // Java.
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassArrayToNonStringNonArrayMethod() throws Throwable {
        // LIVECONNECT_COMPLIANCE: Should raise JavaScript exception.
        executeJavaScript("testObject.setIntValue([42, 42, 42]);");
        assertEquals(0, mTestObject.waitForIntValue());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassNonArrayToArrayMethod() throws Throwable {
        // LIVECONNECT_COMPLIANCE: Should raise JavaScript exception.
        executeJavaScript("testObject.setIntArray(42);");
        assertNull(mTestObject.waitForIntArray());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testObjectWithLengthProperty() throws Throwable {
        executeJavaScript("testObject.setIntArray({length: 3, 1: 42});");
        int[] result = mTestObject.waitForIntArray();
        assertEquals(3, result.length);
        assertEquals(0, result[0]);
        assertEquals(42, result[1]);
        assertEquals(0, result[2]);
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testNonNumericLengthProperty() throws Throwable {
        // LIVECONNECT_COMPLIANCE: This should not count as an array, so we
        // should raise a JavaScript exception.
        executeJavaScript("testObject.setIntArray({length: \"foo\"});");
        assertNull(mTestObject.waitForIntArray());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testLengthOutOfBounds() throws Throwable {
        // LIVECONNECT_COMPLIANCE: This should not count as an array, so we
        // should raise a JavaScript exception.
        executeJavaScript("testObject.setIntArray({length: -1});");
        assertNull(mTestObject.waitForIntArray());

        // LIVECONNECT_COMPLIANCE: This should not count as an array, so we
        // should raise a JavaScript exception.
        long length = Integer.MAX_VALUE + 1L;
        executeJavaScript("testObject.setIntArray({length: " + length + "});");
        assertNull(mTestObject.waitForIntArray());

        // LIVECONNECT_COMPLIANCE: This should not count as an array, so we
        // should raise a JavaScript exception.
        length = Integer.MAX_VALUE + 1L - Integer.MIN_VALUE + 1L;
        executeJavaScript("testObject.setIntArray({length: " + length + "});");
        assertNull(mTestObject.waitForIntArray());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testSparseArray() throws Throwable {
        executeJavaScript("var x = [42, 43]; x[3] = 45; testObject.setIntArray(x);");
        int[] result = mTestObject.waitForIntArray();
        assertEquals(4, result.length);
        assertEquals(42, result[0]);
        assertEquals(43, result[1]);
        assertEquals(0, result[2]);
        assertEquals(45, result[3]);
    }

    // Note that this requires being able to pass a boolean from JavaScript to
    // Java.
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testMethodReturningArrayNotCalled() throws Throwable {
        // We don't invoke methods which return arrays, but note that no
        // exception is raised.
        // LIVECONNECT_COMPLIANCE: Should call method and convert result to
        // JavaScript array.
        executeJavaScript("testObject.setBooleanValue(undefined === testObject.arrayMethod())");
        assertTrue(mTestObject.waitForBooleanValue());
        assertFalse(mTestObject.wasArrayMethodCalled());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testMultiDimensionalArrayMethod() throws Throwable {
        // LIVECONNECT_COMPLIANCE: Should handle multi-dimensional arrays.
        executeJavaScript("testObject.setIntIntArray([ [42, 43], [44, 45] ]);");
        assertNull(mTestObject.waitForIntIntArray());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassMultiDimensionalArray() throws Throwable {
        // LIVECONNECT_COMPLIANCE: Should handle multi-dimensional arrays.
        executeJavaScript("testObject.setIntArray([ [42, 43], [44, 45] ]);");
        int[] result = mTestObject.waitForIntArray();
        assertEquals(2, result.length);
        assertEquals(0, result[0]);
        assertEquals(0, result[1]);
    }

    // Verify that ArrayBuffers are not converted into arrays when passed to Java.
    // The LiveConnect spec doesn't mention ArrayBuffers, so it doesn't seem to
    // be a compliance issue.
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassArrayBuffer() throws Throwable {
        executeJavaScript("buffer = new ArrayBuffer(16);");
        executeJavaScript("testObject.setIntArray(buffer);");
        assertNull(mTestObject.waitForIntArray());
    }

    // Verify that ArrayBufferViews are not converted into arrays when passed to Java.
    // The LiveConnect spec doesn't mention ArrayBufferViews, so it doesn't seem to
    // be a compliance issue.
    // Here, a DataView is used as an ArrayBufferView instance (since the latter is
    // an interface and can't be instantiated directly). See also JavaBridgeArrayCoercionTest
    // for typed arrays (that also subclass ArrayBufferView) tests.
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testPassDataView() throws Throwable {
        executeJavaScript("buffer = new ArrayBuffer(16);");
        executeJavaScript("testObject.setIntArray(new DataView(buffer));");
        assertNull(mTestObject.waitForIntArray());
    }
}
