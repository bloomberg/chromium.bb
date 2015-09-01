// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import java.lang.reflect.Array;
import java.lang.reflect.Field;

/**
 * Reflection helper methods.
 */
final class Reflect {
    /**
     * Sets the value of an object's field (even if it's not visible).
     *
     * @param instance The object containing the field to set.
     * @param name The name of the field to set.
     * @param value The new value for the field.
     */
    static void setField(Object instance, String name, Object value)
            throws NoSuchFieldException {
        Field field = findField(instance, name);
        try {
            field.setAccessible(true);
            field.set(instance, value);
        } catch (IllegalAccessException e) {
            // This shouldn't happen.
        }
    }

    /**
     * Retrieves the value of an object's field (even if it's not visible).
     *
     * @param instance The object containing the field to set.
     * @param name The name of the field to set.
     * @return The field's value. Primitive values are returned as their boxed type.
     */
    static Object getField(Object instance, String name) throws NoSuchFieldException {
        Field field = findField(instance, name);
        try {
            field.setAccessible(true);
            return field.get(instance);
        } catch (IllegalAccessException e) {
            // This shouldn't happen.
        }
        return null;
    }

    /**
     * Concatenates two arrays into a new array. The arrays must be of the same type.
     */
    static Object[] concatArrays(Object[] left, Object[] right) {
        Object[] result = (Object[]) (Array.newInstance(
                left.getClass().getComponentType(), left.length + right.length));
        System.arraycopy(left, 0, result, 0, left.length);
        System.arraycopy(right, 0, result, left.length, right.length);
        return result;
    }

    /**
     * Finds the Field with the given name for the given object, traversing superclasses if
     * necessary.
     */
    private static Field findField(Object instance, String name) throws NoSuchFieldException {
        for (Class<?> clazz = instance.getClass(); clazz != null; clazz = clazz.getSuperclass()) {
            try {
                return clazz.getDeclaredField(name);
            } catch (NoSuchFieldException e) {
            }
        }
        throw new NoSuchFieldException("Field " + name + " not found in " + instance.getClass());
    }
}
