// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.remoteobjects;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import org.chromium.blink.mojom.RemoteInvocationArgument;
import org.chromium.blink.mojom.RemoteInvocationResult;
import org.chromium.blink.mojom.RemoteObject;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.Arrays;

/**
 * Tests the implementation of the Mojo object which wraps invocations
 * of Java methods.
 */
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

    private static class Holder<T> {
        public T value = null;

        public Holder() {}
        public Holder(T value) {
            this.value = value;
        }
        public void reset() {
            value = null;
        }
    }

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
        final Holder<Boolean> hasMethodResult = new Holder<>();

        // This method is public and annotated; it should be exposed.
        hasMethodResult.reset();
        remoteObject.hasMethod("exposedMethod", (Boolean result) -> hasMethodResult.value = result);
        Assert.assertEquals(true, hasMethodResult.value);

        // This method is private; it should not be exposed.
        hasMethodResult.reset();
        remoteObject.hasMethod(
                "privateAnnotatedMethod", (Boolean result) -> hasMethodResult.value = result);
        Assert.assertEquals(false, hasMethodResult.value);

        // This method is not annotated; it should not be exposed.
        hasMethodResult.reset();
        remoteObject.hasMethod(
                "unannotatedMethod", (Boolean result) -> hasMethodResult.value = result);
        Assert.assertEquals(false, hasMethodResult.value);

        // getMethods should provide a result consistent with this.
        // The result must also be in sorted order and have no duplicates.
        final Holder<String[]> getMethodsResult = new Holder<>();
        remoteObject.getMethods((String[] result) -> getMethodsResult.value = result);
        Assert.assertArrayEquals(
                new String[] {"anotherExposedMethod", "exposedMethod"}, getMethodsResult.value);
    }

    @Test
    public void testHasMethodWithoutSafeAnnotationClass() {
        Object target = new Object() {
            @TestJavascriptInterface
            public void annotatedMethod() {}

            public void unannotatedMethod() {}
        };

        RemoteObject remoteObject = new RemoteObjectImpl(target, null);
        final Holder<Boolean> hasMethodResult = new Holder<Boolean>();

        // This method has an annotation; it should be exposed.
        hasMethodResult.reset();
        remoteObject.hasMethod(
                "annotatedMethod", (Boolean result) -> hasMethodResult.value = result);
        Assert.assertEquals(true, hasMethodResult.value);

        // This method doesn't, but passing null skips the check.
        hasMethodResult.reset();
        remoteObject.hasMethod(
                "unannotatedMethod", (Boolean result) -> hasMethodResult.value = result);
        Assert.assertEquals(true, hasMethodResult.value);

        // getMethods should provide a result consistent with this.
        // The result must also be in sorted order.
        // Note that this includes all of the normal java.lang.Object methods.
        final Holder<String[]> getMethodsResult = new Holder<>();
        remoteObject.getMethods((String[] result) -> getMethodsResult.value = result);
        Assert.assertTrue(Arrays.asList(getMethodsResult.value).contains("annotatedMethod"));
        Assert.assertTrue(Arrays.asList(getMethodsResult.value).contains("unannotatedMethod"));
        Assert.assertTrue(Arrays.asList(getMethodsResult.value).contains("hashCode"));
        String[] sortedMethods =
                Arrays.copyOf(getMethodsResult.value, getMethodsResult.value.length);
        Arrays.sort(sortedMethods);
        Assert.assertArrayEquals(sortedMethods, getMethodsResult.value);
    }

    @Test
    public void testInvokeMethodBasic() {
        final Holder<Integer> frobnicateCount = new Holder<>(0);
        Object target = new Object() {
            @TestJavascriptInterface
            public void frobnicate() {
                frobnicateCount.value++;
            }
        };

        RemoteObject remoteObject = new RemoteObjectImpl(target, TestJavascriptInterface.class);
        Assert.assertEquals(Integer.valueOf(0), frobnicateCount.value);
        remoteObject.invokeMethod("frobnicate", new RemoteInvocationArgument[] {},
                (RemoteInvocationResult result) -> {});
        Assert.assertEquals(Integer.valueOf(1), frobnicateCount.value);
        remoteObject.invokeMethod("frobnicate", new RemoteInvocationArgument[] {},
                (RemoteInvocationResult result) -> {});
        Assert.assertEquals(Integer.valueOf(2), frobnicateCount.value);
    }

    @Test
    public void testInvokeMethodOverloadUsingArity() {
        final Holder<Integer> frobnicateCount = new Holder<>(0);
        Object target = new Object() {
            @TestJavascriptInterface
            public void frobnicate() {
                frobnicateCount.value++;
            }

            @TestJavascriptInterface
            public void frobnicate(Object argument) {
                frobnicateCount.value += 2;
            }
        };

        // The method overload to be called depends on the number of arguments supplied.
        // TODO(jbroman): Once it's possible to construct a non-trivial argument, do so.
        RemoteObject remoteObject = new RemoteObjectImpl(target, TestJavascriptInterface.class);
        Assert.assertEquals(Integer.valueOf(0), frobnicateCount.value);
        remoteObject.invokeMethod("frobnicate", new RemoteInvocationArgument[] {},
                (RemoteInvocationResult result) -> {});
        Assert.assertEquals(Integer.valueOf(1), frobnicateCount.value);
        remoteObject.invokeMethod("frobnicate", new RemoteInvocationArgument[] {null},
                (RemoteInvocationResult result) -> {});
        Assert.assertEquals(Integer.valueOf(3), frobnicateCount.value);
    }
}
