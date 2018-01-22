// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.remoteobjects;

import static org.mockito.AdditionalMatchers.aryEq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
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
import org.chromium.blink.mojom.RemoteInvocationResult;
import org.chromium.blink.mojom.RemoteObject;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.Arrays;
import java.util.function.Consumer;

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
        remoteObject.invokeMethod("frobnicate", new RemoteInvocationArgument[] {null}, response);

        InOrder inOrder = inOrder(consumer);
        inOrder.verify(consumer).accept(0);
        inOrder.verify(consumer).accept(1);
        verify(response, times(2)).call(resultIsOk());
    }

    private RemoteInvocationResult resultIsOk() {
        // TODO(jbroman): Check the error code once there is one.
        return ArgumentMatchers.<RemoteInvocationResult>any();
    }
}
