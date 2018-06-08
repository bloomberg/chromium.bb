// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.bytecode;

import org.objectweb.asm.Type;

import java.lang.reflect.Array;
import java.util.HashMap;
import java.util.Map;

/**
 * Utility methods for working with {@link Class}es.
 */
public class ClassUtils {
    private static final Map<Type, Class<?>> PRIMITIVE_CLASSES;
    static {
        PRIMITIVE_CLASSES = new HashMap<>();
        PRIMITIVE_CLASSES.put(Type.BOOLEAN_TYPE, Boolean.TYPE);
        PRIMITIVE_CLASSES.put(Type.BYTE_TYPE, Byte.TYPE);
        PRIMITIVE_CLASSES.put(Type.CHAR_TYPE, Character.TYPE);
        PRIMITIVE_CLASSES.put(Type.DOUBLE_TYPE, Double.TYPE);
        PRIMITIVE_CLASSES.put(Type.FLOAT_TYPE, Float.TYPE);
        PRIMITIVE_CLASSES.put(Type.INT_TYPE, Integer.TYPE);
        PRIMITIVE_CLASSES.put(Type.LONG_TYPE, Long.TYPE);
        PRIMITIVE_CLASSES.put(Type.SHORT_TYPE, Short.TYPE);
    }

    private ClassLoader mClassLoader;

    ClassUtils(ClassLoader classLoader) {
        mClassLoader = classLoader;
    }

    /**
     * Loads a class using an internal name (fully qualified name for a type with dots replaced by
     * slashes) or standard class name with dots.
     *
     * @param className name of the class to load
     * @return Loaded Class object.
     */
    public Class<?> loadClass(String className) {
        try {
            return mClassLoader.loadClass(className.replace('/', '.'));
        } catch (ClassNotFoundException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Checks if one class is the subclass of another.
     *
     * @return true if candidate is a subclass of other.
     */
    public boolean isSubClass(String candidate, String other) {
        Class<?> candidateClazz = loadClass(candidate);
        Class<?> parentClazz = loadClass(other);
        return parentClazz.isAssignableFrom(candidateClazz);
    }

    /**
     * Convert from a Java ASM {@link Type} to a Java {@link Class} object.
     *
     * @param type Java ASM type that represents a primitive or class (not a method).
     * @return The resulting Class.
     */
    public Class<?> convert(Type type) {
        if (PRIMITIVE_CLASSES.containsKey(type)) {
            return PRIMITIVE_CLASSES.get(type);
        }
        String className = type.toString();

        // Process arrays
        if (className.startsWith("[")) {
            Class<?> innerClass = convert(Type.getType(className.substring(1)));
            return Array.newInstance(innerClass, 0).getClass();
        }

        // Remove special characters
        if (className.startsWith("L") && className.endsWith(";")) {
            className = className.substring(1, className.length() - 1);
        }

        return loadClass(className);
    }
}
