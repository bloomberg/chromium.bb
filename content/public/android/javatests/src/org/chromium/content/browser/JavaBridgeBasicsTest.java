// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.JavascriptInterface;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;

/**
 * Part of the test suite for the Java Bridge. Tests a number of features including ...
 * - The type of injected objects
 * - The type of their methods
 * - Replacing objects
 * - Removing objects
 * - Access control
 * - Calling methods on returned objects
 * - Multiply injected objects
 * - Threading
 * - Inheritance
 */
public class JavaBridgeBasicsTest extends JavaBridgeTestBase {
    private class TestController extends Controller {
        private int mIntValue;
        private long mLongValue;
        private String mStringValue;
        private boolean mBooleanValue;

        public synchronized void setIntValue(int x) {
            mIntValue = x;
            notifyResultIsReady();
        }
        public synchronized void setLongValue(long x) {
            mLongValue = x;
            notifyResultIsReady();
        }
        public synchronized void setStringValue(String x) {
            mStringValue = x;
            notifyResultIsReady();
        }
        public synchronized void setBooleanValue(boolean x) {
            mBooleanValue = x;
            notifyResultIsReady();
        }

        public synchronized int waitForIntValue() {
            waitForResult();
            return mIntValue;
        }
        public synchronized long waitForLongValue() {
            waitForResult();
            return mLongValue;
        }
        public synchronized String waitForStringValue() {
            waitForResult();
            return mStringValue;
        }
        public synchronized boolean waitForBooleanValue() {
            waitForResult();
            return mBooleanValue;
        }
    }

    private static class ObjectWithStaticMethod {
        public static String staticMethod() {
            return "foo";
        }
    }

    TestController mTestController;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mTestController = new TestController();
        setUpContentView(mTestController, "testController");
    }

    // Note that this requires that we can pass a JavaScript string to Java.
    protected String executeJavaScriptAndGetStringResult(String script) throws Throwable {
        executeJavaScript("testController.setStringValue(" + script + ");");
        return mTestController.waitForStringValue();
    }

    protected void injectObjectAndReload(final Object object, final String name) throws Throwable {
        injectObjectAndReload(object, name, false);
    }

    protected void injectObjectAndReload(final Object object, final String name,
            final boolean requireAnnotation) throws Throwable {
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mTestCallbackHelperContainer.getOnPageFinishedHelper();
        int currentCallCount = onPageFinishedHelper.getCallCount();
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                getContentView().getContentViewCore().addJavascriptInterface(object, name,
                        requireAnnotation);
                getContentView().reload();
            }
        });
        onPageFinishedHelper.waitForCallback(currentCallCount);
    }

    // Note that this requires that we can pass a JavaScript boolean to Java.
    private void assertRaisesException(String script) throws Throwable {
        executeJavaScript("try {" +
                          script + ";" +
                          "  testController.setBooleanValue(false);" +
                          "} catch (exception) {" +
                          "  testController.setBooleanValue(true);" +
                          "}");
        assertTrue(mTestController.waitForBooleanValue());
    }

    @SmallTest
    @Feature({"Android-WebView", "Android-JavaBridge"})
    public void testTypeOfInjectedObject() throws Throwable {
        assertEquals("object", executeJavaScriptAndGetStringResult("typeof testController"));
    }

    @SmallTest
    @Feature({"Android-WebView", "Android-JavaBridge"})
    public void testAdditionNotReflectedUntilReload() throws Throwable {
        assertEquals("undefined", executeJavaScriptAndGetStringResult("typeof testObject"));
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                getContentView().getContentViewCore().addJavascriptInterface(new Object(),
                                                                             "testObject", false);
            }
        });
        assertEquals("undefined", executeJavaScriptAndGetStringResult("typeof testObject"));
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mTestCallbackHelperContainer.getOnPageFinishedHelper();
        int currentCallCount = onPageFinishedHelper.getCallCount();
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                getContentView().reload();
            }
        });
        onPageFinishedHelper.waitForCallback(currentCallCount);
        assertEquals("object", executeJavaScriptAndGetStringResult("typeof testObject"));
    }

    @SmallTest
    @Feature({"Android-WebView", "Android-JavaBridge"})
    public void testRemovalNotReflectedUntilReload() throws Throwable {
        injectObjectAndReload(new Object(), "testObject");
        assertEquals("object", executeJavaScriptAndGetStringResult("typeof testObject"));
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                getContentView().getContentViewCore().removeJavascriptInterface("testObject");
            }
        });
        assertEquals("object", executeJavaScriptAndGetStringResult("typeof testObject"));
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mTestCallbackHelperContainer.getOnPageFinishedHelper();
        int currentCallCount = onPageFinishedHelper.getCallCount();
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                getContentView().reload();
            }
        });
        onPageFinishedHelper.waitForCallback(currentCallCount);
        assertEquals("undefined", executeJavaScriptAndGetStringResult("typeof testObject"));
    }

    @SmallTest
    @Feature({"Android-WebView", "Android-JavaBridge"})
    public void testRemoveObjectNotAdded() throws Throwable {
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mTestCallbackHelperContainer.getOnPageFinishedHelper();
        int currentCallCount = onPageFinishedHelper.getCallCount();
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                getContentView().getContentViewCore().removeJavascriptInterface("foo");
                getContentView().reload();
            }
        });
        onPageFinishedHelper.waitForCallback(currentCallCount);
        assertEquals("undefined", executeJavaScriptAndGetStringResult("typeof foo"));
    }

    @SmallTest
    @Feature({"Android-WebView", "Android-JavaBridge"})
    public void testTypeOfMethod() throws Throwable {
        assertEquals("function",
                executeJavaScriptAndGetStringResult("typeof testController.setStringValue"));
    }

    @SmallTest
    @Feature({"Android-WebView", "Android-JavaBridge"})
    public void testTypeOfInvalidMethod() throws Throwable {
        assertEquals("undefined", executeJavaScriptAndGetStringResult("typeof testController.foo"));
    }

    @SmallTest
    @Feature({"Android-WebView", "Android-JavaBridge"})
    public void testCallingInvalidMethodRaisesException() throws Throwable {
        assertRaisesException("testController.foo()");
    }

    @SmallTest
    @Feature({"Android-WebView", "Android-JavaBridge"})
    public void testUncaughtJavaExceptionRaisesJavaScriptException() throws Throwable {
        injectObjectAndReload(new Object() {
            public void method() { throw new RuntimeException("foo"); }
        }, "testObject");
        assertRaisesException("testObject.method()");
    }

    // Note that this requires that we can pass a JavaScript string to Java.
    @SmallTest
    @Feature({"Android-WebView", "Android-JavaBridge"})
    public void testTypeOfStaticMethod() throws Throwable {
        injectObjectAndReload(new ObjectWithStaticMethod(), "testObject");
        executeJavaScript("testController.setStringValue(typeof testObject.staticMethod)");
        assertEquals("function", mTestController.waitForStringValue());
    }

    // Note that this requires that we can pass a JavaScript string to Java.
    @SmallTest
    @Feature({"Android-WebView", "Android-JavaBridge"})
    public void testCallStaticMethod() throws Throwable {
        injectObjectAndReload(new ObjectWithStaticMethod(), "testObject");
        executeJavaScript("testController.setStringValue(testObject.staticMethod())");
        assertEquals("foo", mTestController.waitForStringValue());
    }

    @SmallTest
    @Feature({"Android-WebView", "Android-JavaBridge"})
    public void testPrivateMethodNotExposed() throws Throwable {
        injectObjectAndReload(new Object() {
            private void method() {}
            protected void method2() {}
        }, "testObject");
        assertEquals("undefined",
                executeJavaScriptAndGetStringResult("typeof testObject.method"));
        assertEquals("undefined",
                executeJavaScriptAndGetStringResult("typeof testObject.method2"));
    }

    @SmallTest
    @Feature({"Android-WebView", "Android-JavaBridge"})
    public void testReplaceInjectedObject() throws Throwable {
        injectObjectAndReload(new Object() {
            public void method() { mTestController.setStringValue("object 1"); }
        }, "testObject");
        executeJavaScript("testObject.method()");
        assertEquals("object 1", mTestController.waitForStringValue());

        injectObjectAndReload(new Object() {
            public void method() { mTestController.setStringValue("object 2"); }
        }, "testObject");
        executeJavaScript("testObject.method()");
        assertEquals("object 2", mTestController.waitForStringValue());
    }

    @SmallTest
    @Feature({"Android-WebView", "Android-JavaBridge"})
    public void testInjectNullObjectIsIgnored() throws Throwable {
        injectObjectAndReload(null, "testObject");
        assertEquals("undefined", executeJavaScriptAndGetStringResult("typeof testObject"));
    }

    @SmallTest
    @Feature({"Android-WebView", "Android-JavaBridge"})
    public void testReplaceInjectedObjectWithNullObjectIsIgnored() throws Throwable {
        injectObjectAndReload(new Object(), "testObject");
        assertEquals("object", executeJavaScriptAndGetStringResult("typeof testObject"));
        injectObjectAndReload(null, "testObject");
        assertEquals("object", executeJavaScriptAndGetStringResult("typeof testObject"));
    }

    @SmallTest
    @Feature({"Android-WebView", "Android-JavaBridge"})
    public void testCallOverloadedMethodWithDifferentNumberOfArguments() throws Throwable {
        injectObjectAndReload(new Object() {
            public void method() { mTestController.setStringValue("0 args"); }
            public void method(int x) { mTestController.setStringValue("1 arg"); }
            public void method(int x, int y) { mTestController.setStringValue("2 args"); }
        }, "testObject");
        executeJavaScript("testObject.method()");
        assertEquals("0 args", mTestController.waitForStringValue());
        executeJavaScript("testObject.method(42)");
        assertEquals("1 arg", mTestController.waitForStringValue());
        executeJavaScript("testObject.method(null)");
        assertEquals("1 arg", mTestController.waitForStringValue());
        executeJavaScript("testObject.method(undefined)");
        assertEquals("1 arg", mTestController.waitForStringValue());
        executeJavaScript("testObject.method(42, 42)");
        assertEquals("2 args", mTestController.waitForStringValue());
    }

    @SmallTest
    @Feature({"Android-WebView", "Android-JavaBridge"})
    public void testCallMethodWithWrongNumberOfArgumentsRaisesException() throws Throwable {
        assertRaisesException("testController.setIntValue()");
        assertRaisesException("testController.setIntValue(42, 42)");
    }

    @SmallTest
    @Feature({"Android-WebView", "Android-JavaBridge"})
    public void testObjectPersistsAcrossPageLoads() throws Throwable {
        assertEquals("object", executeJavaScriptAndGetStringResult("typeof testController"));
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mTestCallbackHelperContainer.getOnPageFinishedHelper();
        int currentCallCount = onPageFinishedHelper.getCallCount();
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                getContentView().reload();
            }
        });
        onPageFinishedHelper.waitForCallback(currentCallCount);
        assertEquals("object", executeJavaScriptAndGetStringResult("typeof testController"));
    }

    @SmallTest
    @Feature({"Android-WebView", "Android-JavaBridge"})
    public void testSameObjectInjectedMultipleTimes() throws Throwable {
        class TestObject {
            private int mNumMethodInvocations;
            public void method() { mTestController.setIntValue(++mNumMethodInvocations); }
        }
        final TestObject testObject = new TestObject();
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mTestCallbackHelperContainer.getOnPageFinishedHelper();
        int currentCallCount = onPageFinishedHelper.getCallCount();
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                getContentView().getContentViewCore().addJavascriptInterface(testObject,
                                                                             "testObject1", false);
                getContentView().getContentViewCore().addJavascriptInterface(testObject,
                                                                             "testObject2", false);
                getContentView().reload();
            }
        });
        onPageFinishedHelper.waitForCallback(currentCallCount);
        executeJavaScript("testObject1.method()");
        assertEquals(1, mTestController.waitForIntValue());
        executeJavaScript("testObject2.method()");
        assertEquals(2, mTestController.waitForIntValue());
    }

    @SmallTest
    @Feature({"Android-WebView", "Android-JavaBridge"})
    public void testCallMethodOnReturnedObject() throws Throwable {
        injectObjectAndReload(new Object() {
            public Object getInnerObject() {
                return new Object() {
                    public void method(int x) { mTestController.setIntValue(x); }
                };
            }
        }, "testObject");
        executeJavaScript("testObject.getInnerObject().method(42)");
        assertEquals(42, mTestController.waitForIntValue());
    }

    @SmallTest
    @Feature({"Android-WebView", "Android-JavaBridge"})
    public void testReturnedObjectInjectedElsewhere() throws Throwable {
        class InnerObject {
            private int mNumMethodInvocations;
            public void method() { mTestController.setIntValue(++mNumMethodInvocations); }
        }
        final InnerObject innerObject = new InnerObject();
        final Object object = new Object() {
            public InnerObject getInnerObject() {
                return innerObject;
            }
        };
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mTestCallbackHelperContainer.getOnPageFinishedHelper();
        int currentCallCount = onPageFinishedHelper.getCallCount();
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                getContentView().getContentViewCore().addJavascriptInterface(object, "testObject",
                                                                             false);
                getContentView().getContentViewCore().addJavascriptInterface(innerObject,
                                                                             "innerObject", false);
                getContentView().reload();
            }
        });
        onPageFinishedHelper.waitForCallback(currentCallCount);
        executeJavaScript("testObject.getInnerObject().method()");
        assertEquals(1, mTestController.waitForIntValue());
        executeJavaScript("innerObject.method()");
        assertEquals(2, mTestController.waitForIntValue());
    }

    @SmallTest
    @Feature({"Android-WebView", "Android-JavaBridge"})
    public void testMethodInvokedOnBackgroundThread() throws Throwable {
        injectObjectAndReload(new Object() {
            public void captureThreadId() {
                mTestController.setLongValue(Thread.currentThread().getId());
            }
        }, "testObject");
        executeJavaScript("testObject.captureThreadId()");
        final long threadId = mTestController.waitForLongValue();
        assertFalse(threadId == Thread.currentThread().getId());
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                assertFalse(threadId == Thread.currentThread().getId());
            }
        });
    }

    @SmallTest
    @Feature({"Android-WebView", "Android-JavaBridge"})
    public void testPublicInheritedMethod() throws Throwable {
        class Base {
            public void method(int x) { mTestController.setIntValue(x); }
        }
        class Derived extends Base {
        }
        injectObjectAndReload(new Derived(), "testObject");
        assertEquals("function", executeJavaScriptAndGetStringResult("typeof testObject.method"));
        executeJavaScript("testObject.method(42)");
        assertEquals(42, mTestController.waitForIntValue());
    }

    @SmallTest
    @Feature({"Android-WebView", "Android-JavaBridge"})
    public void testPrivateInheritedMethod() throws Throwable {
        class Base {
            private void method() {}
        }
        class Derived extends Base {
        }
        injectObjectAndReload(new Derived(), "testObject");
        assertEquals("undefined", executeJavaScriptAndGetStringResult("typeof testObject.method"));
    }

    @SmallTest
    @Feature({"Android-WebView", "Android-JavaBridge"})
    public void testOverriddenMethod() throws Throwable {
        class Base {
            public void method() { mTestController.setStringValue("base"); }
        }
        class Derived extends Base {
            public void method() { mTestController.setStringValue("derived"); }
        }
        injectObjectAndReload(new Derived(), "testObject");
        executeJavaScript("testObject.method()");
        assertEquals("derived", mTestController.waitForStringValue());
    }

    @SmallTest
    @Feature({"Android-WebView", "Android-JavaBridge"})
    public void testEnumerateMembers() throws Throwable {
        injectObjectAndReload(new Object() {
            public void method() {}
            private void privateMethod() {}
            public int field;
            private int privateField;
        }, "testObject");
        executeJavaScript(
                "var result = \"\"; " +
                "for (x in testObject) { result += \" \" + x } " +
                "testController.setStringValue(result);");
        // LIVECONNECT_COMPLIANCE: Should be able to enumerate members.
        assertEquals("", mTestController.waitForStringValue());
    }

    @SmallTest
    @Feature({"Android-WebView", "Android-JavaBridge"})
    public void testReflectPublicMethod() throws Throwable {
        injectObjectAndReload(new Object() {
            public String method() { return "foo"; }
        }, "testObject");
        assertEquals("foo", executeJavaScriptAndGetStringResult(
                "testObject.getClass().getMethod('method', null).invoke(testObject, null)" +
                ".toString()"));
    }

    @SmallTest
    @Feature({"Android-WebView", "Android-JavaBridge"})
    public void testReflectPublicField() throws Throwable {
        injectObjectAndReload(new Object() {
            public String field = "foo";
        }, "testObject");
        assertEquals("foo", executeJavaScriptAndGetStringResult(
                "testObject.getClass().getField('field').get(testObject).toString()"));
    }

    @SmallTest
    @Feature({"Android-WebView", "Android-JavaBridge"})
    public void testReflectPrivateMethodRaisesException() throws Throwable {
        injectObjectAndReload(new Object() {
            private void method() {};
        }, "testObject");
        assertRaisesException("testObject.getClass().getMethod('method', null)");
        // getDeclaredMethod() is able to access a private method, but invoke()
        // throws a Java exception.
        assertRaisesException(
                "testObject.getClass().getDeclaredMethod('method', null).invoke(testObject, null)");
    }

    @SmallTest
    @Feature({"Android-WebView", "Android-JavaBridge"})
    public void testReflectPrivateFieldRaisesException() throws Throwable {
        injectObjectAndReload(new Object() {
            private int field;
        }, "testObject");
        assertRaisesException("testObject.getClass().getField('field')");
        // getDeclaredField() is able to access a private field, but getInt()
        // throws a Java exception.
        assertRaisesException(
                "testObject.getClass().getDeclaredField('field').getInt(testObject)");
    }

    @SmallTest
    @Feature({"Android-WebView", "Android-JavaBridge"})
    public void testAllowNonAnnotatedMethods() throws Throwable {
        injectObjectAndReload(new Object() {
            public String allowed() { return "foo"; }
        }, "testObject", false);

        // Test calling a method of an explicitly inherited class (Base#allowed()).
        assertEquals("foo", executeJavaScriptAndGetStringResult("testObject.allowed()"));

        // Test calling a method of an implicitly inherited class (Object#getClass()).
        assertEquals("object", executeJavaScriptAndGetStringResult("typeof testObject.getClass()"));
    }

    @SmallTest
    @Feature({"Android-WebView", "Android-JavaBridge"})
    public void testAllowOnlyAnnotatedMethods() throws Throwable {
        class Test {
            public String allowed() { return "foo"; }
        }

        injectObjectAndReload(new Object() {
            @JavascriptInterface
            public String allowed() { return "foo"; }

            public String disallowed() { return "bar"; }
        }, "testObject", true);

        // getClass() is an Object method and does not have the @JavascriptInterface annotation and
        // should not be able to be called.
        assertRaisesException("testObject.getClass()");
        assertEquals("undefined", executeJavaScriptAndGetStringResult(
                "typeof testObject.getClass"));

        // allowed() is marked with the @JavascriptInterface annotation and should be allowed to be
        // called.
        assertEquals("foo", executeJavaScriptAndGetStringResult("testObject.allowed()"));

        // disallowed() is not marked with the @JavascriptInterface annotation and should not be
        // able to be called.
        assertRaisesException("testObject.disallowed()");
        assertEquals("undefined", executeJavaScriptAndGetStringResult(
                "typeof testObject.disallowed"));
    }

    @SmallTest
    @Feature({"Android-WebView", "Android-JavaBridge"})
    public void testAnnotationRequirementRetainsPropertyAcrossObjects() throws Throwable {
        class Test {
            @JavascriptInterface
            public String safe() { return "foo"; }

            public String unsafe() { return "bar"; }
        }

        class TestReturner {
            @JavascriptInterface
            public Test getTest() { return new Test(); }
        }

        // First test with safe mode off.
        injectObjectAndReload(new TestReturner(), "unsafeTestObject", false);

        // safe() should be able to be called regardless of whether or not we are in safe mode.
        assertEquals("foo", executeJavaScriptAndGetStringResult(
                "unsafeTestObject.getTest().safe()"));
        // unsafe() should be able to be called because we are not in safe mode.
        assertEquals("bar", executeJavaScriptAndGetStringResult(
                "unsafeTestObject.getTest().unsafe()"));

        // Now test with safe mode on.
        injectObjectAndReload(new TestReturner(), "safeTestObject", true);

        // safe() should be able to be called regardless of whether or not we are in safe mode.
        assertEquals("foo", executeJavaScriptAndGetStringResult(
                "safeTestObject.getTest().safe()"));
        // unsafe() should not be able to be called because we are in safe mode.
        assertRaisesException("safeTestObject.getTest().unsafe()");
        assertEquals("undefined", executeJavaScriptAndGetStringResult(
                "typeof safeTestObject.getTest().unsafe"));
        // getClass() is an Object method and does not have the @JavascriptInterface annotation and
        // should not be able to be called.
        assertRaisesException("safeTestObject.getTest().getClass()");
        assertEquals("undefined", executeJavaScriptAndGetStringResult(
                "typeof safeTestObject.getTest().getClass"));
    }

    @SmallTest
    @Feature({"Android-WebView", "Android-JavaBridge"})
    public void testAnnotationDoesNotGetInherited() throws Throwable {
        class Base {
            @JavascriptInterface
            public void base() { }
        }

        class Child extends Base {
            @Override
            public void base() { }
        }

        injectObjectAndReload(new Child(), "testObject", true);

        // base() is inherited.  The inherited method does not have the @JavascriptInterface
        // annotation and should not be able to be called.
        assertRaisesException("testObject.base()");
        assertEquals("undefined", executeJavaScriptAndGetStringResult(
                "typeof testObject.base"));
    }
}
