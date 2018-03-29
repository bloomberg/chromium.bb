// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.support_lib_boundary.util;

import android.annotation.TargetApi;
import android.os.Build;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

/**
 * A set of utility methods used for calling across the support library boundary.
 */
public class BoundaryInterfaceReflectionUtil {
    /**
     * Utility method for fetching a method from {@param delegateLoader}, with the same signature
     * (package + class + method name + parameters) as a given method defined in another
     * classloader.
     */
    public static Method dupeMethod(Method method, ClassLoader delegateLoader)
            throws ClassNotFoundException, NoSuchMethodException {
        Class<?> declaringClass =
                Class.forName(method.getDeclaringClass().getName(), true, delegateLoader);
        Class[] otherSideParameterClasses = method.getParameterTypes();
        Class[] parameterClasses = new Class[otherSideParameterClasses.length];
        for (int n = 0; n < parameterClasses.length; n++) {
            Class<?> clazz = otherSideParameterClasses[n];
            // Primitive classes are shared between the classloaders - so we can use the same
            // primitive class declarations on either side. Non-primitive classes must be looked up
            // by name.
            parameterClasses[n] = clazz.isPrimitive()
                    ? clazz
                    : Class.forName(clazz.getName(), true, delegateLoader);
        }
        return declaringClass.getDeclaredMethod(method.getName(), parameterClasses);
    }

    /**
     * Returns an implementation of the boundary interface named clazz, by delegating method calls
     * to the {@link InvocationHandler} invocationHandler.
     */
    public static <T> T castToSuppLibClass(Class<T> clazz, InvocationHandler invocationHandler) {
        return clazz.cast(
                Proxy.newProxyInstance(BoundaryInterfaceReflectionUtil.class.getClassLoader(),
                        new Class[] {clazz}, invocationHandler));
    }

    /**
     * Create an {@link java.lang.reflect.InvocationHandler} that delegates method calls to
     * {@param delegate}, making sure that the {@link java.lang.reflect.Method} and parameters being
     * passed to {@param delegate} exist in the same {@link java.lang.ClassLoader} as {@param
     * delegate}.
     */
    @TargetApi(Build.VERSION_CODES.KITKAT)
    public static InvocationHandler createInvocationHandlerFor(final Object delegate) {
        final ClassLoader delegateLoader = delegate.getClass().getClassLoader();
        return new InvocationHandler() {
            @Override
            public Object invoke(Object o, Method method, Object[] objects) throws Throwable {
                try {
                    return dupeMethod(method, delegateLoader).invoke(delegate, objects);
                } catch (InvocationTargetException e) {
                    // If something went wrong, ensure we throw the original exception.
                    throw e.getTargetException();
                } catch (ReflectiveOperationException e) {
                    throw new RuntimeException("Reflection failed for method " + method, e);
                }
            }
        };
    }
}
