// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.DownloadInfo.Builder;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.Random;

/**
 * Tests for (@link DownloadInfo}.
 */
public class DownloadInfoTest extends InstrumentationTestCase {
    /**
     * Class to store getter/setter information. Stores the method name of a getter and setter
     * without prefix, so "int getFoo()" and "void setFoo(int)" both have same AccessorSignature
     * {"Foo", int}.
     */
    static class AccessorSignature {
        final String mMethodNameWithoutPrefix;
        final Class<?> mReturnTypeOrParam;

        AccessorSignature(String methodNameWithoutPrefix, Class<?> returnTypeOrParam) {
            mMethodNameWithoutPrefix = methodNameWithoutPrefix;
            mReturnTypeOrParam = returnTypeOrParam;
        }

        @Override
        public int hashCode() {
            return mMethodNameWithoutPrefix.hashCode() * 31
                    + mReturnTypeOrParam.hashCode();
        }

        @Override
        public boolean equals(Object obj) {
            if (obj == this) {
                return true;
            }
            if (obj instanceof AccessorSignature) {
                AccessorSignature other = (AccessorSignature) obj;
                return other.mReturnTypeOrParam == mReturnTypeOrParam
                       && other.mMethodNameWithoutPrefix.equals(mMethodNameWithoutPrefix);
            }
            return false;
        }

        @Override
        public String toString() {
            return "{ " + mMethodNameWithoutPrefix + ", " + mReturnTypeOrParam.getName() + "}";
        }
    }

    /**
     * Returns an AccessorInfo object for the method if it is a getter. A getter for the class has
     * signature: Type getFoo(), where Type is a String or a primitive type other than boolean.
     * Boolean getters are an exception and have special prefixes. In case of a boolean getter, the
     * signature is 'boolean isFoo()' or 'boolean hasFoo', i.e. boolean getters start with "is" or
     * "has".
     * @param method the method that is a getter
     * @return AccessorInfo for the method if it is a getter, null otherwise.
     */
    AccessorSignature getGetterInfo(Method method) {
        // A getter is of format Type getFoo(): i.e. 0 params and one
        // return type.
        if (method.getParameterTypes().length == 0) {
            // Based on return type extract the name of the getter.
            Class<?> returnType = method.getReturnType();
            if (returnType.isPrimitive() || returnType == String.class) {
                String methodName = method.getName();
                if (returnType.equals(Boolean.TYPE)) {
                    if (methodName.matches("(is|has).*")) {
                        return new AccessorSignature(methodName.replaceFirst("is|has", ""),
                                returnType);
                    }
                } else {
                    if (methodName.startsWith("get")) {
                        return new AccessorSignature(methodName.substring(3), returnType);
                    }
                }
            }
        }
        return null;
    }

    /**
     * Returns an AccessorInfo object for the method if it is a setter. A setter for the class has
     * signature: Type setFoo(), where Type is a String or a primitive type.
     * @param method the method that is a getter
     * @return AccessorInfo for the method if it is a getter, null otherwise.
     */
    AccessorSignature getSetterInfo(Method method) {
        if (method.getParameterTypes().length == 1) {
            Class<?> parameter = method.getParameterTypes()[0];
            String methodName = method.getName();
            if (methodName.startsWith("set")) {
                if (parameter.equals(Boolean.TYPE)) {
                    // Boolean setters are of form setIsFoo or setHasFoo.
                    return new AccessorSignature(
                            methodName.replaceFirst("set(Is|Has)", ""),
                            parameter);
                } else {
                    return new AccessorSignature(methodName.substring(3), parameter);
                }
            }
        }
        return null;
    }

    /**
     * Invoke a method via reflection and rethrow the exception. Makes findbugs happy.
     * @param method Method to invoke.
     * @param instance class instance on which method should be invoked.
     * @return return value of invocation.
     */
    Object invokeMethod(Method method, Object instance, Object... args) throws Exception {
        try {
            return method.invoke(instance, args);
        } catch (IllegalArgumentException e) {
            throw e;
        } catch (IllegalAccessException e) {
            throw e;
        } catch (InvocationTargetException e) {
            throw e;
        }
    }

    @SmallTest
    @Feature({"Downloads"})
    public void testBuilderHasCorrectSetters() {
        HashMap<AccessorSignature, Method> downloadInfoGetters =
                new HashMap<AccessorSignature, Method>();
        HashMap<AccessorSignature, Method> builderSetters =
                new HashMap<AccessorSignature, Method>();
        for (Method m : DownloadInfo.class.getMethods()) {
            AccessorSignature info = getGetterInfo(m);
            if (info != null) {
                downloadInfoGetters.put(info, m);
            }
        }
        assertTrue("There should be at least one getter.",
                downloadInfoGetters.size() > 0);
        for (Method m : Builder.class.getMethods()) {
            AccessorSignature info = getSetterInfo(m);
            if (info != null) {
                builderSetters.put(info, m);
            }
        }

        // Make sure we have a setter for each getter and vice versa.
        assertEquals("Mismatch between getters and setters.",
                downloadInfoGetters.keySet(), builderSetters.keySet());

        // Generate specific values for fields and verify that they are correctly set. For boolean
        // fields set them all to true. For integers generate random numbers.
        Random random = new Random();
        HashMap<AccessorSignature, Object> valuesForBuilder =
                new HashMap<DownloadInfoTest.AccessorSignature, Object>();
        for (AccessorSignature signature : builderSetters.keySet()) {
            if (signature.mReturnTypeOrParam.equals(String.class)) {
                String value = signature.mMethodNameWithoutPrefix
                        + Integer.toString(random.nextInt());
                valuesForBuilder.put(signature, value);
            } else if (signature.mReturnTypeOrParam.equals(Boolean.TYPE)) {
                valuesForBuilder.put(signature, Boolean.TRUE);
            } else {
                // This is a primitive type that is not boolean, probably an integer.
                valuesForBuilder.put(signature, Integer.valueOf(random.nextInt(100)));
            }
        }

        Builder builder = new Builder();
        // Create a DownloadInfo object with these values.
        for (AccessorSignature signature : builderSetters.keySet()) {
            Method setter = builderSetters.get(signature);
            try {
                invokeMethod(setter, builder, valuesForBuilder.get(signature));
            } catch (Exception e) {
                fail("Exception while setting value in the setter. Signature: " + signature
                        + " value:" + valuesForBuilder.get(signature) + ":" + e);
            }
        }
        DownloadInfo downloadInfo = builder.build();
        for (AccessorSignature signature : downloadInfoGetters.keySet()) {
            Method getter = downloadInfoGetters.get(signature);
            try {
                Object returnValue = invokeMethod(getter, downloadInfo);
                assertEquals(signature.toString(),
                        valuesForBuilder.get(signature).toString(), returnValue.toString());
            } catch (Exception e) {
                fail("Exception while getting value from getter. Signature: " + signature
                        + " value:" + valuesForBuilder.get(signature));
            }
        }

        // Test DownloadInfo.fromDownloadInfo copies all fields.
        DownloadInfo newDownloadInfo = Builder.fromDownloadInfo(downloadInfo).build();
        for (AccessorSignature signature : downloadInfoGetters.keySet()) {
            Method getter = downloadInfoGetters.get(signature);
            try {
                Object returnValue1 = invokeMethod(getter, downloadInfo);
                Object returnValue2 = invokeMethod(getter, newDownloadInfo);
                assertEquals(signature.toString(), returnValue1, returnValue2);
            } catch (Exception e) {
                fail("Exception while getting value from getter. Signature: " + signature
                        + " value:" + valuesForBuilder.get(signature) + ":" + e);
            }
        }
    }
}
