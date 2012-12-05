// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.JavascriptInterface;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;

import java.lang.annotation.Annotation;
import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

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
        injectObjectAndReload(object, name, null);
    }

    protected void injectObjectAndReload(final Object object, final String name,
            final Class<? extends Annotation> requiredAnnotation) throws Throwable {
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mTestCallbackHelperContainer.getOnPageFinishedHelper();
        int currentCallCount = onPageFinishedHelper.getCallCount();
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                getContentView().getContentViewCore().addPossiblyUnsafeJavascriptInterface(object,
                        name, requiredAnnotation);
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
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testTypeOfInjectedObject() throws Throwable {
        assertEquals("object", executeJavaScriptAndGetStringResult("typeof testController"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testAdditionNotReflectedUntilReload() throws Throwable {
        assertEquals("undefined", executeJavaScriptAndGetStringResult("typeof testObject"));
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                getContentView().getContentViewCore().addPossiblyUnsafeJavascriptInterface(
                        new Object(), "testObject", null);
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
    @Feature({"AndroidWebView", "Android-JavaBridge"})
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
    @Feature({"AndroidWebView", "Android-JavaBridge"})
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
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testTypeOfMethod() throws Throwable {
        assertEquals("function",
                executeJavaScriptAndGetStringResult("typeof testController.setStringValue"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testTypeOfInvalidMethod() throws Throwable {
        assertEquals("undefined", executeJavaScriptAndGetStringResult("typeof testController.foo"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testCallingInvalidMethodRaisesException() throws Throwable {
        assertRaisesException("testController.foo()");
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testUncaughtJavaExceptionRaisesJavaScriptException() throws Throwable {
        injectObjectAndReload(new Object() {
            public void method() { throw new RuntimeException("foo"); }
        }, "testObject");
        assertRaisesException("testObject.method()");
    }

    // Note that this requires that we can pass a JavaScript string to Java.
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testTypeOfStaticMethod() throws Throwable {
        injectObjectAndReload(new ObjectWithStaticMethod(), "testObject");
        executeJavaScript("testController.setStringValue(typeof testObject.staticMethod)");
        assertEquals("function", mTestController.waitForStringValue());
    }

    // Note that this requires that we can pass a JavaScript string to Java.
    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testCallStaticMethod() throws Throwable {
        injectObjectAndReload(new ObjectWithStaticMethod(), "testObject");
        executeJavaScript("testController.setStringValue(testObject.staticMethod())");
        assertEquals("foo", mTestController.waitForStringValue());
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
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
    @Feature({"AndroidWebView", "Android-JavaBridge"})
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
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testInjectNullObjectIsIgnored() throws Throwable {
        injectObjectAndReload(null, "testObject");
        assertEquals("undefined", executeJavaScriptAndGetStringResult("typeof testObject"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testReplaceInjectedObjectWithNullObjectIsIgnored() throws Throwable {
        injectObjectAndReload(new Object(), "testObject");
        assertEquals("object", executeJavaScriptAndGetStringResult("typeof testObject"));
        injectObjectAndReload(null, "testObject");
        assertEquals("object", executeJavaScriptAndGetStringResult("typeof testObject"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
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
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testCallMethodWithWrongNumberOfArgumentsRaisesException() throws Throwable {
        assertRaisesException("testController.setIntValue()");
        assertRaisesException("testController.setIntValue(42, 42)");
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
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
    @Feature({"AndroidWebView", "Android-JavaBridge"})
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
                getContentView().getContentViewCore().addPossiblyUnsafeJavascriptInterface(
                        testObject, "testObject1", null);
                getContentView().getContentViewCore().addPossiblyUnsafeJavascriptInterface(
                        testObject, "testObject2", null);
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
    @Feature({"AndroidWebView", "Android-JavaBridge"})
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
    @Feature({"AndroidWebView", "Android-JavaBridge"})
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
                getContentView().getContentViewCore().addPossiblyUnsafeJavascriptInterface(
                        object, "testObject", null);
                getContentView().getContentViewCore().addPossiblyUnsafeJavascriptInterface(
                        innerObject, "innerObject", null);
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
    @Feature({"AndroidWebView", "Android-JavaBridge"})
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
    @Feature({"AndroidWebView", "Android-JavaBridge"})
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
    @Feature({"AndroidWebView", "Android-JavaBridge"})
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
    @Feature({"AndroidWebView", "Android-JavaBridge"})
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
    @Feature({"AndroidWebView", "Android-JavaBridge"})
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
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testReflectPublicMethod() throws Throwable {
        injectObjectAndReload(new Object() {
            public String method() { return "foo"; }
        }, "testObject");
        assertEquals("foo", executeJavaScriptAndGetStringResult(
                "testObject.getClass().getMethod('method', null).invoke(testObject, null)" +
                ".toString()"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testReflectPublicField() throws Throwable {
        injectObjectAndReload(new Object() {
            public String field = "foo";
        }, "testObject");
        assertEquals("foo", executeJavaScriptAndGetStringResult(
                "testObject.getClass().getField('field').get(testObject).toString()"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
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
    @Feature({"AndroidWebView", "Android-JavaBridge"})
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
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testAllowNonAnnotatedMethods() throws Throwable {
        injectObjectAndReload(new Object() {
            public String allowed() { return "foo"; }
        }, "testObject", null);

        // Test calling a method of an explicitly inherited class (Base#allowed()).
        assertEquals("foo", executeJavaScriptAndGetStringResult("testObject.allowed()"));

        // Test calling a method of an implicitly inherited class (Object#getClass()).
        assertEquals("object", executeJavaScriptAndGetStringResult("typeof testObject.getClass()"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testAllowOnlyAnnotatedMethods() throws Throwable {
        injectObjectAndReload(new Object() {
            @JavascriptInterface
            public String allowed() { return "foo"; }

            public String disallowed() { return "bar"; }
        }, "testObject", JavascriptInterface.class);

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
    @Feature({"AndroidWebView", "Android-JavaBridge"})
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
        injectObjectAndReload(new TestReturner(), "unsafeTestObject", null);

        // safe() should be able to be called regardless of whether or not we are in safe mode.
        assertEquals("foo", executeJavaScriptAndGetStringResult(
                "unsafeTestObject.getTest().safe()"));
        // unsafe() should be able to be called because we are not in safe mode.
        assertEquals("bar", executeJavaScriptAndGetStringResult(
                "unsafeTestObject.getTest().unsafe()"));

        // Now test with safe mode on.
        injectObjectAndReload(new TestReturner(), "safeTestObject", JavascriptInterface.class);

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
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testAnnotationDoesNotGetInherited() throws Throwable {
        class Base {
            @JavascriptInterface
            public void base() { }
        }

        class Child extends Base {
            @Override
            public void base() { }
        }

        injectObjectAndReload(new Child(), "testObject", JavascriptInterface.class);

        // base() is inherited.  The inherited method does not have the @JavascriptInterface
        // annotation and should not be able to be called.
        assertRaisesException("testObject.base()");
        assertEquals("undefined", executeJavaScriptAndGetStringResult(
                "typeof testObject.base"));
    }

    @SuppressWarnings("javadoc")
    @Retention(RetentionPolicy.RUNTIME)
    @Target({ElementType.METHOD})
    @interface TestAnnotation {
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testCustomAnnotationRestriction() throws Throwable {
        class Test {
            @TestAnnotation
            public String checkTestAnnotationFoo() { return "bar"; }

            @JavascriptInterface
            public String checkJavascriptInterfaceFoo() { return "bar"; }
        }

        // Inject javascriptInterfaceObj and require the JavascriptInterface annotation.
        injectObjectAndReload(new Test(), "javascriptInterfaceObj", JavascriptInterface.class);

        // Test#testAnnotationFoo() should fail, as it isn't annotated with JavascriptInterface.
        assertRaisesException("javascriptInterfaceObj.checkTestAnnotationFoo()");
        assertEquals("undefined", executeJavaScriptAndGetStringResult(
                "typeof javascriptInterfaceObj.checkTestAnnotationFoo"));

        // Test#javascriptInterfaceFoo() should pass, as it is annotated with JavascriptInterface.
        assertEquals("bar", executeJavaScriptAndGetStringResult(
                "javascriptInterfaceObj.checkJavascriptInterfaceFoo()"));

        // Inject testAnnotationObj and require the TestAnnotation annotation.
        injectObjectAndReload(new Test(), "testAnnotationObj", TestAnnotation.class);

        // Test#testAnnotationFoo() should pass, as it is annotated with TestAnnotation.
        assertEquals("bar", executeJavaScriptAndGetStringResult(
                "testAnnotationObj.checkTestAnnotationFoo()"));

        // Test#javascriptInterfaceFoo() should fail, as it isn't annotated with TestAnnotation.
        assertRaisesException("testAnnotationObj.checkJavascriptInterfaceFoo()");
        assertEquals("undefined", executeJavaScriptAndGetStringResult(
                "typeof testAnnotationObj.checkJavascriptInterfaceFoo"));
    }

    @SmallTest
    @Feature({"AndroidWebView", "Android-JavaBridge"})
    public void testAddJavascriptInterfaceIsSafeByDefault() throws Throwable {
        class Test {
            public String blocked() { return "bar"; }

            @JavascriptInterface
            public String allowed() { return "bar"; }
        }

        // Manually inject the Test object, making sure to use the
        // ContentViewCore#addJavascriptInterface, not the possibly unsafe version.
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mTestCallbackHelperContainer.getOnPageFinishedHelper();
        int currentCallCount = onPageFinishedHelper.getCallCount();
        runTestOnUiThread(new Runnable() {
            @Override
            public void run() {
                getContentView().getContentViewCore().addJavascriptInterface(new Test(),
                        "testObject");
                getContentView().reload();
            }
        });
        onPageFinishedHelper.waitForCallback(currentCallCount);

        // Test#allowed() should pass, as it is annotated with JavascriptInterface.
        assertEquals("bar", executeJavaScriptAndGetStringResult(
                "testObject.allowed()"));

        // Test#blocked() should fail, as it isn't annotated with JavascriptInterface.
        assertRaisesException("testObject.blocked()");
        assertEquals("undefined", executeJavaScriptAndGetStringResult(
                "typeof testObject.blocked"));
    }
}
