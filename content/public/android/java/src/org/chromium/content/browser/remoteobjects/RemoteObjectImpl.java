// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.remoteobjects;

import org.chromium.blink.mojom.RemoteInvocationArgument;
import org.chromium.blink.mojom.RemoteInvocationError;
import org.chromium.blink.mojom.RemoteInvocationResult;
import org.chromium.blink.mojom.RemoteObject;
import org.chromium.mojo.system.MojoException;

import java.lang.annotation.Annotation;
import java.lang.ref.WeakReference;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;
import java.util.Set;
import java.util.SortedMap;
import java.util.TreeMap;

/**
 * Exposes limited access to a Java object over a Mojo interface.
 */
class RemoteObjectImpl implements RemoteObject {
    /**
     * Receives notification about events for auditing.
     *
     * Separated from this class proper to allow for unit testing in content_junit_tests, where the
     * Android framework is not fully initialized.
     *
     * Implementations should take care not to hold a strong reference to anything that might keep
     * the WebView contents alive due to a GC cycle.
     */
    interface Auditor {
        void onObjectGetClassInvocationAttempt();
    }

    /**
     * Method which may not be called.
     */
    private static final Method sGetClassMethod;
    static {
        try {
            sGetClassMethod = Object.class.getMethod("getClass");
        } catch (NoSuchMethodException e) {
            // java.lang.Object#getClass should always exist.
            throw new RuntimeException(e);
        }
    }

    /**
     * The object to which invocations should be directed.
     *
     * The target object cannot be referred to strongly, because it may contain
     * references which form an uncollectable cycle.
     */
    private final WeakReference<Object> mTarget;

    /**
     * Receives notification about events for auditing.
     */
    private final Auditor mAuditor;

    /**
     * Callable methods, indexed by name.
     */
    private final SortedMap<String, List<Method>> mMethods = new TreeMap<>();

    public RemoteObjectImpl(Object target, Class<? extends Annotation> safeAnnotationClass) {
        this(target, safeAnnotationClass, null);
    }

    public RemoteObjectImpl(
            Object target, Class<? extends Annotation> safeAnnotationClass, Auditor auditor) {
        mTarget = new WeakReference<>(target);
        mAuditor = auditor;

        for (Method method : target.getClass().getMethods()) {
            if (safeAnnotationClass != null && !method.isAnnotationPresent(safeAnnotationClass)) {
                continue;
            }

            String methodName = method.getName();
            List<Method> methodsWithName = mMethods.get(methodName);
            if (methodsWithName == null) {
                methodsWithName = new ArrayList<>(1);
                mMethods.put(methodName, methodsWithName);
            }
            methodsWithName.add(method);
        }
    }

    @Override
    public void hasMethod(String name, HasMethodResponse callback) {
        callback.call(mMethods.containsKey(name));
    }

    @Override
    public void getMethods(GetMethodsResponse callback) {
        Set<String> methodNames = mMethods.keySet();
        callback.call(methodNames.toArray(new String[methodNames.size()]));
    }

    @Override
    public void invokeMethod(
            String name, RemoteInvocationArgument[] arguments, InvokeMethodResponse callback) {
        Object target = mTarget.get();
        if (target == null) {
            // TODO(jbroman): Handle this.
            return;
        }

        int numArguments = arguments.length;
        Method method = findMethod(name, numArguments);
        if (method == null) {
            callback.call(makeErrorResult(RemoteInvocationError.METHOD_NOT_FOUND));
            return;
        }
        if (method.equals(sGetClassMethod)) {
            if (mAuditor != null) {
                mAuditor.onObjectGetClassInvocationAttempt();
            }
            callback.call(makeErrorResult(RemoteInvocationError.OBJECT_GET_CLASS_BLOCKED));
            return;
        }

        Object[] args = new Object[numArguments];
        for (int i = 0; i < numArguments; i++) {
            args[i] = convertArgument(arguments[i]);
        }

        Object result = null;
        try {
            result = method.invoke(target, args);
        } catch (IllegalAccessException | IllegalArgumentException | NullPointerException e) {
            // These should never happen.
            //
            // IllegalAccessException:
            //   java.lang.Class#getMethods returns only public members, so |mMethods| should never
            //   contain any method for which IllegalAccessException would be thrown.
            //
            // IllegalArgumentException:
            //   Argument coercion logic is responsible for creating objects of a suitable Java
            //   type.
            //   TODO(jbroman): Actually write said coercion logic.
            //
            // NullPointerException:
            //   A user of this class is responsible for ensuring that the target is not collected.
            throw new RuntimeException(e);
        } catch (InvocationTargetException e) {
            e.getCause().printStackTrace();
            callback.call(makeErrorResult(RemoteInvocationError.EXCEPTION_THROWN));
            return;
        }

        RemoteInvocationResult mojoResult = convertResult(result);
        callback.call(mojoResult);
    }

    @Override
    public void close() {
        // TODO(jbroman): Handle this.
    }

    @Override
    public void onConnectionError(MojoException e) {
        close();
    }

    private Method findMethod(String name, int numParameters) {
        List<Method> methods = mMethods.get(name);
        if (methods == null) {
            return null;
        }

        // LIVECONNECT_COMPLIANCE: We just take the first method with the correct
        // number of arguments, while the spec proposes using cost-based algorithm:
        // https://jdk6.java.net/plugin2/liveconnect/#OVERLOADED_METHODS
        for (Method method : methods) {
            if (method.getParameterTypes().length == numParameters) return method;
        }

        return null;
    }

    private Object convertArgument(RemoteInvocationArgument argument) {
        // TODO(jbroman): Convert arguments.
        return null;
    }

    private RemoteInvocationResult convertResult(Object result) {
        // TODO(jbroman): Convert result.
        return new RemoteInvocationResult();
    }

    private static RemoteInvocationResult makeErrorResult(int error) {
        assert error != RemoteInvocationError.OK;
        RemoteInvocationResult result = new RemoteInvocationResult();
        result.error = error;
        return result;
    }
}
