// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.remoteobjects;

import static org.mockito.AdditionalMatchers.aryEq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatchers;
import org.mockito.InOrder;

import org.chromium.blink.mojom.RemoteInvocationArgument;
import org.chromium.blink.mojom.RemoteInvocationError;
import org.chromium.blink.mojom.RemoteInvocationResult;
import org.chromium.blink.mojom.RemoteObject;
import org.chromium.blink.mojom.SingletonJavaScriptValue;
import org.chromium.mojo_base.mojom.String16;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.Arrays;
import java.util.function.Consumer;

/**
 * Tests the implementation of the Mojo object which wraps invocations
 * of Java methods.
 *
 * Unchecked cast warnings are suppressed because {@link org.mockito.Mockito#mock(Class)} does not
 * provide a way to cleanly deal with generics.
 */
@SuppressWarnings("unchecked")
@RunWith(BlockJUnit4ClassRunner.class)
public final class RemoteObjectImplTest {
    /**
     * Annotation which can be used in the way that {@link android.webkit.JavascriptInterface}
     * would.
     *
     * A separate one is used to ensure that RemoteObject is actually respecting the parameter.
     */
    @Retention(RetentionPolicy.RUNTIME)
    @Target({ElementType.METHOD})
    private @interface TestJavascriptInterface {}

    @Test
    public void testHasMethodWithSafeAnnotationClass() {
        Object target = new Object() {
            @TestJavascriptInterface
            public void exposedMethod() {}

            @TestJavascriptInterface
            public void anotherExposedMethod() {}

            @TestJavascriptInterface
            public void anotherExposedMethod(int x) {}

            @TestJavascriptInterface
            private void privateAnnotatedMethod() {}

            public void unannotatedMethod() {}
        };

        RemoteObject remoteObject = new RemoteObjectImpl(target, TestJavascriptInterface.class);
        RemoteObject.HasMethodResponse hasMethodResponse;

        // This method is public and annotated; it should be exposed.
        hasMethodResponse = mock(RemoteObject.HasMethodResponse.class);
        remoteObject.hasMethod("exposedMethod", hasMethodResponse);
        verify(hasMethodResponse).call(true);

        // This method is private; it should not be exposed.
        hasMethodResponse = mock(RemoteObject.HasMethodResponse.class);
        remoteObject.hasMethod("privateAnnotatedMethod", hasMethodResponse);
        verify(hasMethodResponse).call(false);

        // This method is not annotated; it should not be exposed.
        hasMethodResponse = mock(RemoteObject.HasMethodResponse.class);
        remoteObject.hasMethod("unannotatedMethod", hasMethodResponse);
        verify(hasMethodResponse).call(false);

        // getMethods should provide a result consistent with this.
        // The result must also be in sorted order and have no duplicates.
        RemoteObject.GetMethodsResponse getMethodsResponse =
                mock(RemoteObject.GetMethodsResponse.class);
        remoteObject.getMethods(getMethodsResponse);
        verify(getMethodsResponse)
                .call(aryEq(new String[] {"anotherExposedMethod", "exposedMethod"}));
    }

    @Test
    public void testHasMethodWithoutSafeAnnotationClass() {
        Object target = new Object() {
            @TestJavascriptInterface
            public void annotatedMethod() {}

            public void unannotatedMethod() {}
        };

        RemoteObject remoteObject = new RemoteObjectImpl(target, null);
        RemoteObject.HasMethodResponse hasMethodResponse;

        // This method has an annotation; it should be exposed.
        hasMethodResponse = mock(RemoteObject.HasMethodResponse.class);
        remoteObject.hasMethod("annotatedMethod", hasMethodResponse);
        verify(hasMethodResponse).call(true);

        // This method doesn't, but passing null skips the check.
        hasMethodResponse = mock(RemoteObject.HasMethodResponse.class);
        remoteObject.hasMethod("unannotatedMethod", hasMethodResponse);
        verify(hasMethodResponse).call(true);

        // getMethods should provide a result consistent with this.
        // The result must also be in sorted order.
        // Note that this includes all of the normal java.lang.Object methods.
        RemoteObject.GetMethodsResponse getMethodsResponse =
                mock(RemoteObject.GetMethodsResponse.class);
        remoteObject.getMethods(getMethodsResponse);

        ArgumentCaptor<String[]> methodsCaptor = ArgumentCaptor.forClass(String[].class);
        verify(getMethodsResponse).call(methodsCaptor.capture());
        String[] methods = methodsCaptor.getValue();
        Assert.assertTrue(Arrays.asList(methods).contains("annotatedMethod"));
        Assert.assertTrue(Arrays.asList(methods).contains("unannotatedMethod"));
        Assert.assertTrue(Arrays.asList(methods).contains("hashCode"));
        String[] sortedMethods = Arrays.copyOf(methods, methods.length);
        Arrays.sort(sortedMethods);
        Assert.assertArrayEquals(sortedMethods, methods);
    }

    @Test
    public void testInvokeMethodBasic() {
        final Runnable runnable = mock(Runnable.class);
        Object target = new Object() {
            @TestJavascriptInterface
            public void frobnicate() {
                runnable.run();
            }
        };

        RemoteObject remoteObject = new RemoteObjectImpl(target, TestJavascriptInterface.class);
        RemoteObject.InvokeMethodResponse response = mock(RemoteObject.InvokeMethodResponse.class);
        remoteObject.invokeMethod("frobnicate", new RemoteInvocationArgument[] {}, response);
        remoteObject.invokeMethod("frobnicate", new RemoteInvocationArgument[] {}, response);

        verify(runnable, times(2)).run();
        verify(response, times(2)).call(resultIsOk());
    }

    @Test
    public void testInvokeMethodOverloadUsingArity() {
        final Consumer<Integer> consumer = (Consumer<Integer>) mock(Consumer.class);
        Object target = new Object() {
            @TestJavascriptInterface
            public void frobnicate() {
                consumer.accept(0);
            }

            @TestJavascriptInterface
            public void frobnicate(Object argument) {
                consumer.accept(1);
            }
        };

        // The method overload to be called depends on the number of arguments supplied.
        // TODO(jbroman): Once it's possible to construct a non-trivial argument, do so.
        RemoteObject remoteObject = new RemoteObjectImpl(target, TestJavascriptInterface.class);
        RemoteObject.InvokeMethodResponse response = mock(RemoteObject.InvokeMethodResponse.class);
        remoteObject.invokeMethod("frobnicate", new RemoteInvocationArgument[] {}, response);
        remoteObject.invokeMethod(
                "frobnicate", new RemoteInvocationArgument[] {numberArgument(0)}, response);

        InOrder inOrder = inOrder(consumer);
        inOrder.verify(consumer).accept(0);
        inOrder.verify(consumer).accept(1);
        verify(response, times(2)).call(resultIsOk());
    }

    /**
     * Reports to the runnable it is given when its static method is called.
     * Works around the fact that a static method cannot capture variables.
     */
    static class ObjectWithStaticMethod {
        static Runnable sRunnable;

        @TestJavascriptInterface
        public static void staticMethod() {
            sRunnable.run();
        }
    }

    @Test
    public void testStaticMethod() {
        // Static methods should work just like non-static ones.

        Object target = new ObjectWithStaticMethod();
        RemoteObject remoteObject = new RemoteObjectImpl(target, TestJavascriptInterface.class);

        Runnable runnable = mock(Runnable.class);
        ObjectWithStaticMethod.sRunnable = runnable;
        RemoteObject.InvokeMethodResponse response = mock(RemoteObject.InvokeMethodResponse.class);
        remoteObject.invokeMethod("staticMethod", new RemoteInvocationArgument[] {}, response);
        ObjectWithStaticMethod.sRunnable = null;
        verify(runnable).run();
        verify(response).call(resultIsOk());

        RemoteObject.HasMethodResponse hasMethodResponse =
                mock(RemoteObject.HasMethodResponse.class);
        remoteObject.hasMethod("staticMethod", hasMethodResponse);
        verify(hasMethodResponse).call(true);

        RemoteObject.GetMethodsResponse getMethodsResponse =
                mock(RemoteObject.GetMethodsResponse.class);
        remoteObject.getMethods(getMethodsResponse);
        verify(getMethodsResponse).call(aryEq(new String[] {"staticMethod"}));
    }

    @Test
    public void testInvokeMethodNotFound() {
        Object target = new Object() {
            public void unexposedMethod() {
                Assert.fail("Unexposed method should not be called.");
            }

            @TestJavascriptInterface
            public void exposedMethodWithWrongArity(Object argument) {
                Assert.fail("Exposed method should only be called with the correct arity.");
            }
        };

        RemoteObject remoteObject = new RemoteObjectImpl(target, TestJavascriptInterface.class);
        RemoteObject.InvokeMethodResponse response = mock(RemoteObject.InvokeMethodResponse.class);
        remoteObject.invokeMethod("nonexistentMethod", new RemoteInvocationArgument[] {}, response);
        remoteObject.invokeMethod("unexposedMethod", new RemoteInvocationArgument[] {}, response);
        remoteObject.invokeMethod(
                "exposedMethodWithWrongArity", new RemoteInvocationArgument[] {}, response);

        verify(response, times(3)).call(resultHasError(RemoteInvocationError.METHOD_NOT_FOUND));
    }

    @Test
    public void testObjectGetClassBlocked() {
        Object target = new Object();
        RemoteObjectImpl.Auditor auditor = mock(RemoteObjectImpl.Auditor.class);
        RemoteObject.InvokeMethodResponse response = mock(RemoteObject.InvokeMethodResponse.class);
        RemoteObject remoteObject = new RemoteObjectImpl(target, null, auditor);
        remoteObject.invokeMethod("getClass", new RemoteInvocationArgument[] {}, response);

        verify(response).call(resultHasError(RemoteInvocationError.OBJECT_GET_CLASS_BLOCKED));
        verify(auditor).onObjectGetClassInvocationAttempt();
    }

    @Test
    public void testOverloadedGetClassPermitted() {
        final Runnable runnable = mock(Runnable.class);
        Object target = new Object() {
            @TestJavascriptInterface
            public void getClass(Object o) {
                runnable.run();
            }
        };
        RemoteObjectImpl.Auditor auditor = mock(RemoteObjectImpl.Auditor.class);
        RemoteObject.InvokeMethodResponse response = mock(RemoteObject.InvokeMethodResponse.class);
        RemoteObject remoteObject =
                new RemoteObjectImpl(target, TestJavascriptInterface.class, auditor);
        remoteObject.invokeMethod(
                "getClass", new RemoteInvocationArgument[] {numberArgument(0)}, response);

        verify(runnable).run();
        verify(response).call(resultIsOk());
        verify(auditor, never()).onObjectGetClassInvocationAttempt();
    }

    @Test
    public void testInvocationTargetException() {
        Object target = new Object() {
            @TestJavascriptInterface
            public void exceptionThrowingMethod() throws Exception {
                throw new Exception("This exception is expected during test. Do not be alarmed.");
            }
        };

        RemoteObject remoteObject = new RemoteObjectImpl(target, TestJavascriptInterface.class);
        RemoteObject.InvokeMethodResponse response = mock(RemoteObject.InvokeMethodResponse.class);
        remoteObject.invokeMethod(
                "exceptionThrowingMethod", new RemoteInvocationArgument[] {}, response);

        verify(response).call(resultHasError(RemoteInvocationError.EXCEPTION_THROWN));
    }

    private static class VariantConsumer {
        private final Consumer<Object> mConsumer;

        public VariantConsumer(Consumer<Object> consumer) {
            mConsumer = consumer;
        }

        @TestJavascriptInterface
        public void consumeByte(byte b) {
            mConsumer.accept(b);
        }
        @TestJavascriptInterface
        public void consumeChar(char c) {
            mConsumer.accept(c);
        }
        @TestJavascriptInterface
        public void consumeShort(short s) {
            mConsumer.accept(s);
        }
        @TestJavascriptInterface
        public void consumeInt(int i) {
            mConsumer.accept(i);
        }
        @TestJavascriptInterface
        public void consumeLong(long l) {
            mConsumer.accept(l);
        }
        @TestJavascriptInterface
        public void consumeFloat(float f) {
            mConsumer.accept(f);
        }
        @TestJavascriptInterface
        public void consumeDouble(double d) {
            mConsumer.accept(d);
        }
        @TestJavascriptInterface
        public void consumeBoolean(boolean b) {
            mConsumer.accept(b);
        }
        @TestJavascriptInterface
        public void consumeString(String s) {
            mConsumer.accept(s);
        }
        @TestJavascriptInterface
        public void consumeObjectArray(Object[] oa) {
            mConsumer.accept(oa);
        }
        @TestJavascriptInterface
        public void consumeObject(Object o) {
            mConsumer.accept(o);
        }
    }

    @Test
    public void testArgumentConversionNumber() {
        final Consumer<Object> consumer = (Consumer<Object>) mock(Consumer.class);
        Object target = new VariantConsumer(consumer);

        RemoteObject remoteObject = new RemoteObjectImpl(target, TestJavascriptInterface.class);
        RemoteObject.InvokeMethodResponse response = mock(RemoteObject.InvokeMethodResponse.class);
        remoteObject.invokeMethod(
                "consumeByte", new RemoteInvocationArgument[] {numberArgument(356)}, response);
        remoteObject.invokeMethod(
                "consumeChar", new RemoteInvocationArgument[] {numberArgument(356)}, response);
        remoteObject.invokeMethod(
                "consumeChar", new RemoteInvocationArgument[] {numberArgument(1.5)}, response);
        remoteObject.invokeMethod(
                "consumeChar", new RemoteInvocationArgument[] {numberArgument(-0.0)}, response);
        remoteObject.invokeMethod(
                "consumeShort", new RemoteInvocationArgument[] {numberArgument(32768)}, response);
        remoteObject.invokeMethod(
                "consumeInt", new RemoteInvocationArgument[] {numberArgument(-1.5)}, response);
        remoteObject.invokeMethod("consumeLong",
                new RemoteInvocationArgument[] {numberArgument(Double.POSITIVE_INFINITY)},
                response);
        remoteObject.invokeMethod("consumeFloat",
                new RemoteInvocationArgument[] {numberArgument(3.141592654)}, response);
        remoteObject.invokeMethod("consumeDouble",
                new RemoteInvocationArgument[] {numberArgument(Double.NaN)}, response);
        remoteObject.invokeMethod(
                "consumeBoolean", new RemoteInvocationArgument[] {numberArgument(1)}, response);
        remoteObject.invokeMethod("consumeString",
                new RemoteInvocationArgument[] {numberArgument(-1.66666666666)}, response);
        remoteObject.invokeMethod("consumeString",
                new RemoteInvocationArgument[] {numberArgument(Double.NaN)}, response);
        remoteObject.invokeMethod("consumeString",
                new RemoteInvocationArgument[] {numberArgument(Double.NEGATIVE_INFINITY)},
                response);
        remoteObject.invokeMethod(
                "consumeString", new RemoteInvocationArgument[] {numberArgument(-0.0)}, response);
        remoteObject.invokeMethod("consumeString",
                new RemoteInvocationArgument[] {numberArgument(123456789)}, response);
        remoteObject.invokeMethod("consumeString",
                new RemoteInvocationArgument[] {numberArgument(123000000.1)}, response);
        remoteObject.invokeMethod(
                "consumeObjectArray", new RemoteInvocationArgument[] {numberArgument(6)}, response);
        remoteObject.invokeMethod(
                "consumeObject", new RemoteInvocationArgument[] {numberArgument(6)}, response);

        verify(consumer).accept((byte) 100);
        verify(consumer).accept('\u0164');
        verify(consumer, times(2)).accept('\u0000');
        verify(consumer).accept((short) -32768);
        verify(consumer).accept((int) -1);
        verify(consumer).accept(Long.MAX_VALUE);
        verify(consumer).accept((float) 3.141592654);
        verify(consumer).accept(Double.NaN);
        verify(consumer).accept(false);
        verify(consumer).accept("-1.66667");
        verify(consumer).accept("nan");
        verify(consumer).accept("-inf");
        verify(consumer).accept("-0");
        verify(consumer).accept("123456789");
        verify(consumer).accept("1.23e+08");
        verify(consumer, times(2)).accept(null);
        verify(response, times(18)).call(resultIsOk());
    }

    @Test
    public void testArgumentConversionBoolean() {
        final Consumer<Object> consumer = (Consumer<Object>) mock(Consumer.class);
        Object target = new VariantConsumer(consumer);

        RemoteObject remoteObject = new RemoteObjectImpl(target, TestJavascriptInterface.class);
        RemoteObject.InvokeMethodResponse response = mock(RemoteObject.InvokeMethodResponse.class);
        remoteObject.invokeMethod(
                "consumeByte", new RemoteInvocationArgument[] {booleanArgument(true)}, response);
        remoteObject.invokeMethod(
                "consumeChar", new RemoteInvocationArgument[] {booleanArgument(true)}, response);
        remoteObject.invokeMethod(
                "consumeShort", new RemoteInvocationArgument[] {booleanArgument(true)}, response);
        remoteObject.invokeMethod(
                "consumeInt", new RemoteInvocationArgument[] {booleanArgument(true)}, response);
        remoteObject.invokeMethod(
                "consumeLong", new RemoteInvocationArgument[] {booleanArgument(true)}, response);
        remoteObject.invokeMethod(
                "consumeFloat", new RemoteInvocationArgument[] {booleanArgument(true)}, response);
        remoteObject.invokeMethod(
                "consumeDouble", new RemoteInvocationArgument[] {booleanArgument(true)}, response);
        remoteObject.invokeMethod(
                "consumeString", new RemoteInvocationArgument[] {booleanArgument(true)}, response);
        remoteObject.invokeMethod(
                "consumeString", new RemoteInvocationArgument[] {booleanArgument(false)}, response);
        remoteObject.invokeMethod("consumeObjectArray",
                new RemoteInvocationArgument[] {booleanArgument(true)}, response);
        remoteObject.invokeMethod(
                "consumeObject", new RemoteInvocationArgument[] {booleanArgument(true)}, response);

        InOrder inOrder = inOrder(consumer);
        inOrder.verify(consumer).accept((byte) 0);
        inOrder.verify(consumer).accept('\u0000');
        inOrder.verify(consumer).accept((short) 0);
        inOrder.verify(consumer).accept((int) 0);
        inOrder.verify(consumer).accept((long) 0);
        inOrder.verify(consumer).accept((float) 0);
        inOrder.verify(consumer).accept((double) 0);
        inOrder.verify(consumer).accept("true");
        inOrder.verify(consumer).accept("false");
        inOrder.verify(consumer, times(2)).accept(null);
    }

    @Test
    public void testArgumentConversionString() {
        final Consumer<Object> consumer = (Consumer<Object>) mock(Consumer.class);
        Object target = new VariantConsumer(consumer);
        String stringWithNonAsciiCharacterAndUnpairedSurrogate = "caf\u00e9\ud800";

        RemoteObject remoteObject = new RemoteObjectImpl(target, TestJavascriptInterface.class);
        RemoteObject.InvokeMethodResponse response = mock(RemoteObject.InvokeMethodResponse.class);
        remoteObject.invokeMethod(
                "consumeByte", new RemoteInvocationArgument[] {stringArgument("hello")}, response);
        remoteObject.invokeMethod(
                "consumeChar", new RemoteInvocationArgument[] {stringArgument("hello")}, response);
        remoteObject.invokeMethod(
                "consumeShort", new RemoteInvocationArgument[] {stringArgument("hello")}, response);
        remoteObject.invokeMethod(
                "consumeInt", new RemoteInvocationArgument[] {stringArgument("hello")}, response);
        remoteObject.invokeMethod(
                "consumeLong", new RemoteInvocationArgument[] {stringArgument("hello")}, response);
        remoteObject.invokeMethod(
                "consumeFloat", new RemoteInvocationArgument[] {stringArgument("hello")}, response);
        remoteObject.invokeMethod("consumeDouble",
                new RemoteInvocationArgument[] {stringArgument("hello")}, response);
        remoteObject.invokeMethod("consumeString",
                new RemoteInvocationArgument[] {stringArgument("hello")}, response);
        remoteObject.invokeMethod("consumeString",
                new RemoteInvocationArgument[] {
                        stringArgument(stringWithNonAsciiCharacterAndUnpairedSurrogate)},
                response);
        remoteObject.invokeMethod("consumeObjectArray",
                new RemoteInvocationArgument[] {stringArgument("hello")}, response);
        remoteObject.invokeMethod("consumeObject",
                new RemoteInvocationArgument[] {stringArgument("hello")}, response);

        InOrder inOrder = inOrder(consumer);
        inOrder.verify(consumer).accept((byte) 0);
        inOrder.verify(consumer).accept('\u0000');
        inOrder.verify(consumer).accept((short) 0);
        inOrder.verify(consumer).accept((int) 0);
        inOrder.verify(consumer).accept((long) 0);
        inOrder.verify(consumer).accept((float) 0);
        inOrder.verify(consumer).accept((double) 0);
        inOrder.verify(consumer).accept("hello");
        inOrder.verify(consumer).accept(stringWithNonAsciiCharacterAndUnpairedSurrogate);
        inOrder.verify(consumer, times(2)).accept(null);
    }

    @Test
    public void testArgumentConversionNull() {
        final Consumer<Object> consumer = (Consumer<Object>) mock(Consumer.class);
        Object target = new VariantConsumer(consumer);

        RemoteObject remoteObject = new RemoteObjectImpl(target, TestJavascriptInterface.class);
        RemoteObject.InvokeMethodResponse response = mock(RemoteObject.InvokeMethodResponse.class);
        RemoteInvocationArgument args[] = {nullArgument()};
        remoteObject.invokeMethod("consumeByte", args, response);
        remoteObject.invokeMethod("consumeChar", args, response);
        remoteObject.invokeMethod("consumeShort", args, response);
        remoteObject.invokeMethod("consumeInt", args, response);
        remoteObject.invokeMethod("consumeLong", args, response);
        remoteObject.invokeMethod("consumeFloat", args, response);
        remoteObject.invokeMethod("consumeDouble", args, response);
        remoteObject.invokeMethod("consumeString", args, response);
        remoteObject.invokeMethod("consumeObjectArray", args, response);
        remoteObject.invokeMethod("consumeObject", args, response);

        verify(consumer).accept((byte) 0);
        verify(consumer).accept('\u0000');
        verify(consumer).accept((short) 0);
        verify(consumer).accept((int) 0);
        verify(consumer).accept((long) 0);
        verify(consumer).accept((float) 0);
        verify(consumer).accept((double) 0);
        verify(consumer, times(3)).accept(null);
    }

    @Test
    public void testArgumentConversionUndefined() {
        final Consumer<Object> consumer = (Consumer<Object>) mock(Consumer.class);
        Object target = new VariantConsumer(consumer);

        RemoteObject remoteObject = new RemoteObjectImpl(target, TestJavascriptInterface.class);
        RemoteObject.InvokeMethodResponse response = mock(RemoteObject.InvokeMethodResponse.class);
        RemoteInvocationArgument args[] = {undefinedArgument()};
        remoteObject.invokeMethod("consumeByte", args, response);
        remoteObject.invokeMethod("consumeChar", args, response);
        remoteObject.invokeMethod("consumeShort", args, response);
        remoteObject.invokeMethod("consumeInt", args, response);
        remoteObject.invokeMethod("consumeLong", args, response);
        remoteObject.invokeMethod("consumeFloat", args, response);
        remoteObject.invokeMethod("consumeDouble", args, response);
        remoteObject.invokeMethod("consumeString", args, response);
        remoteObject.invokeMethod("consumeObjectArray", args, response);
        remoteObject.invokeMethod("consumeObject", args, response);

        verify(consumer).accept((byte) 0);
        verify(consumer).accept('\u0000');
        verify(consumer).accept((short) 0);
        verify(consumer).accept((int) 0);
        verify(consumer).accept((long) 0);
        verify(consumer).accept((float) 0);
        verify(consumer).accept((double) 0);
        verify(consumer).accept("undefined");
        verify(consumer, times(2)).accept(null);
    }

    private RemoteInvocationResult resultHasError(final int error) {
        return ArgumentMatchers.argThat(result -> result.error == error);
    }

    private RemoteInvocationResult resultIsOk() {
        return resultHasError(RemoteInvocationError.OK);
    }

    private RemoteInvocationArgument numberArgument(double numberValue) {
        RemoteInvocationArgument argument = new RemoteInvocationArgument();
        argument.setNumberValue(numberValue);
        return argument;
    }

    private RemoteInvocationArgument booleanArgument(boolean booleanValue) {
        RemoteInvocationArgument argument = new RemoteInvocationArgument();
        argument.setBooleanValue(booleanValue);
        return argument;
    }

    private RemoteInvocationArgument stringArgument(String stringValue) {
        String16 string16 = new String16();
        string16.data = new short[stringValue.length()];
        for (int i = 0; i < stringValue.length(); i++) {
            string16.data[i] = (short) stringValue.charAt(i);
        }
        RemoteInvocationArgument argument = new RemoteInvocationArgument();
        argument.setStringValue(string16);
        return argument;
    }

    private RemoteInvocationArgument nullArgument() {
        RemoteInvocationArgument argument = new RemoteInvocationArgument();
        argument.setSingletonValue(SingletonJavaScriptValue.NULL);
        return argument;
    }

    private RemoteInvocationArgument undefinedArgument() {
        RemoteInvocationArgument argument = new RemoteInvocationArgument();
        argument.setSingletonValue(SingletonJavaScriptValue.UNDEFINED);
        return argument;
    }
}
