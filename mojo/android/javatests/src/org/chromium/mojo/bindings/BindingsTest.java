// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import android.test.suitebuilder.annotation.SmallTest;

import junit.framework.TestCase;

import org.chromium.mojo.bindings.test.mojom.imported.Color;
import org.chromium.mojo.bindings.test.mojom.imported.Point;
import org.chromium.mojo.bindings.test.mojom.imported.Shape;
import org.chromium.mojo.bindings.test.mojom.imported.Thing;
import org.chromium.mojo.bindings.test.mojom.sample.Bar;
import org.chromium.mojo.bindings.test.mojom.sample.DefaultsTest;
import org.chromium.mojo.bindings.test.mojom.sample.Enum;
import org.chromium.mojo.bindings.test.mojom.sample.Foo;
import org.chromium.mojo.bindings.test.mojom.sample.InterfaceConstants;
import org.chromium.mojo.bindings.test.mojom.sample.SampleServiceConstants;
import org.chromium.mojo.system.MessagePipeHandle;

import java.lang.reflect.Field;
import java.lang.reflect.Modifier;

/**
 * Testing generated classes and associated features.
 */
public class BindingsTest extends TestCase {

    private static <T> void checkConstantField(
            Field field, Class<T> expectedClass, T value) throws IllegalAccessException {
        assertEquals(expectedClass, field.getType());
        assertEquals(Modifier.FINAL, field.getModifiers() & Modifier.FINAL);
        assertEquals(Modifier.STATIC, field.getModifiers() & Modifier.STATIC);
        assertEquals(value, field.get(null));
    }

    private static <T> void checkField(Field field, Class<T> expectedClass,
            Object object, T value) throws IllegalArgumentException, IllegalAccessException {
        assertEquals(expectedClass, field.getType());
        assertEquals(0, field.getModifiers() & Modifier.FINAL);
        assertEquals(0, field.getModifiers() & Modifier.STATIC);
        assertEquals(value, field.get(object));
    }

    /**
     * Testing constants are correctly generated.
     */
    @SmallTest
    public void testConstants() throws NoSuchFieldException, SecurityException,
            IllegalAccessException {
        checkConstantField(SampleServiceConstants.class.getField("TWELVE"), byte.class, (byte) 12);
        checkConstantField(InterfaceConstants.class.getField("LONG"), long.class, 4405L);
    }

    /**
     * Testing enums are correctly generated.
     */
    @SmallTest
    public void testEnums() throws NoSuchFieldException, SecurityException,
            IllegalAccessException {
        checkConstantField(Color.class.getField("COLOR_RED"), int.class, 0);
        checkConstantField(Color.class.getField("COLOR_BLACK"), int.class, 1);

        checkConstantField(Enum.class.getField("ENUM_VALUE"), int.class, 0);

        checkConstantField(Shape.class.getField("SHAPE_RECTANGLE"), int.class, 1);
        checkConstantField(Shape.class.getField("SHAPE_CIRCLE"), int.class, 2);
        checkConstantField(Shape.class.getField("SHAPE_TRIANGLE"), int.class, 3);
    }

    /**
     * Testing default values on structs.
     *
     * @throws IllegalAccessException
     * @throws IllegalArgumentException
     */
    @SmallTest
    public void testStructDefaults() throws NoSuchFieldException, SecurityException,
            IllegalArgumentException, IllegalAccessException {
        // Check default values.
        DefaultsTest test = new DefaultsTest();

        checkField(DefaultsTest.class.getField("a0"), byte.class, test, (byte) -12);
        checkField(DefaultsTest.class.getField("a1"), byte.class, test, (byte) 12);
        checkField(DefaultsTest.class.getField("a2"), short.class, test, (short) 1234);
        checkField(DefaultsTest.class.getField("a3"), short.class, test, (short) 34567);
        checkField(DefaultsTest.class.getField("a4"), int.class, test, 123456);
        checkField(DefaultsTest.class.getField("a6"), long.class, test, 111111111111L);
        checkField(DefaultsTest.class.getField("a8"), int.class, test, 0x12345);
        checkField(DefaultsTest.class.getField("a9"), int.class, test, -0x12345);
        checkField(DefaultsTest.class.getField("a10"), int.class, test, 1234);
        checkField(DefaultsTest.class.getField("a11"), boolean.class, test, true);
        checkField(DefaultsTest.class.getField("a12"), boolean.class, test, false);
        checkField(DefaultsTest.class.getField("a13"), float.class, test, (float) 123.25);
        checkField(DefaultsTest.class.getField("a14"), double.class, test, 1234567890.123);
        checkField(DefaultsTest.class.getField("a15"), double.class, test, 1E10);
        checkField(DefaultsTest.class.getField("a16"), double.class, test, -1.2E+20);
        checkField(DefaultsTest.class.getField("a17"), double.class, test, +1.23E-20);
        checkField(DefaultsTest.class.getField("a18"), byte[].class, test, null);
        checkField(DefaultsTest.class.getField("a19"), String.class, test, null);
        checkField(DefaultsTest.class.getField("a20"), int.class, test, Bar.Type.TYPE_BOTH);
        checkField(DefaultsTest.class.getField("a21"), Point.class, test, null);

        assertNotNull(test.a22);
        checkField(DefaultsTest.class.getField("a22"), Thing.class, test, test.a22);
    }

    /**
     * Testing generation of the Foo class.
     *
     * @throws IllegalAccessException
     */
    @SmallTest
    public void testFooGeneration() throws NoSuchFieldException, SecurityException,
            IllegalAccessException {
        // Checking Foo constants.
        checkConstantField(Foo.class.getField("FOOBY"), String.class, "Fooby");

        // Checking Foo default values.
        Foo foo = new Foo();
        checkField(Foo.class.getField("name"), String.class, foo, Foo.FOOBY);

        assertNotNull(foo.source);
        assertFalse(foo.source.isValid());
        checkField(Foo.class.getField("source"), MessagePipeHandle.class, foo, foo.source);
    }
}
