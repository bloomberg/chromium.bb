// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import android.test.suitebuilder.annotation.SmallTest;

import junit.framework.TestCase;

import org.chromium.mojo.HandleMock;
import org.chromium.mojo.bindings.test.mojom.imported.Color;
import org.chromium.mojo.bindings.test.mojom.imported.Point;
import org.chromium.mojo.bindings.test.mojom.imported.Shape;
import org.chromium.mojo.bindings.test.mojom.imported.Thing;
import org.chromium.mojo.bindings.test.mojom.sample.Bar;
import org.chromium.mojo.bindings.test.mojom.sample.Bar.Type;
import org.chromium.mojo.bindings.test.mojom.sample.DefaultsTest;
import org.chromium.mojo.bindings.test.mojom.sample.Enum;
import org.chromium.mojo.bindings.test.mojom.sample.Foo;
import org.chromium.mojo.bindings.test.mojom.sample.InterfaceConstants;
import org.chromium.mojo.bindings.test.mojom.sample.SampleServiceConstants;
import org.chromium.mojo.bindings.test.mojom.test_structs.EmptyStruct;
import org.chromium.mojo.system.DataPipe.ConsumerHandle;
import org.chromium.mojo.system.DataPipe.ProducerHandle;
import org.chromium.mojo.system.MessagePipeHandle;

import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.util.Arrays;

/**
 * Testing generated classes and associated features.
 */
public class BindingsTest extends TestCase {

    /**
     * Create a new typical Bar instance.
     */
    private static Bar newBar() {
        Bar bar = new Bar();
        bar.alpha = (byte) 0x01;
        bar.beta = (byte) 0x02;
        bar.gamma = (byte) 0x03;
        bar.type = Type.BOTH;
        return bar;
    }

    /**
     * Check that 2 Bar instances are equals.
     */
    private static void assertBarEquals(Bar bar, Bar bar2) {
        if (bar == bar2) {
            return;
        }
        assertTrue(bar != null && bar2 != null);
        assertEquals(bar.alpha, bar2.alpha);
        assertEquals(bar.beta, bar2.beta);
        assertEquals(bar.gamma, bar2.gamma);
        assertEquals(bar.type, bar2.type);
    }

    /**
     * Create a new typical Foo instance.
     */
    private static Foo createFoo() {
        Foo foo = new Foo();
        foo.name = "HELLO WORLD";
        foo.arrayOfArrayOfBools = new boolean[][] {
            { true, false, true }, { }, { }, { false }, { true } };
        foo.bar = newBar();
        foo.a = true;
        foo.c = true;
        foo.data = new byte[] { 0x01, 0x02, 0x03 };
        foo.extraBars = new Bar[] { newBar(), newBar() };
        String[][][] strings = new String[3][2][1];
        for (int i0 = 0; i0 < strings.length; ++i0) {
            for (int i1 = 0; i1 < strings[i0].length; ++i1) {
                for (int i2 = 0; i2 < strings[i0][i1].length; ++i2) {
                    strings[i0][i1][i2] = "Hello(" + i0 + ", " + i1 + ", " + i2 + ")";
                }
            }
        }
        foo.multiArrayOfStrings = strings;
        ConsumerHandle[] inputStreams = new ConsumerHandle[5];
        for (int i = 0; i < inputStreams.length; ++i) {
            inputStreams[i] = new HandleMock();
        }
        foo.inputStreams = inputStreams;
        ProducerHandle[] outputStreams = new ProducerHandle[3];
        for (int i = 0; i < outputStreams.length; ++i) {
            outputStreams[i] = new HandleMock();
        }
        foo.outputStreams = outputStreams;
        foo.source = new HandleMock();
        return foo;
    }

    /**
     * Check that 2 Foo instances are equals.
     */
    private static void assertFooEquals(Foo foo1, Foo foo2) {
        assertEquals(foo1.a, foo2.a);
        assertEquals(foo1.b, foo2.b);
        assertEquals(foo1.c, foo2.c);
        assertEquals(foo1.name, foo2.name);
        assertEquals(foo1.x, foo2.x);
        assertEquals(foo1.y, foo2.y);
        TestCase.assertTrue(Arrays.deepEquals(foo1.arrayOfArrayOfBools, foo2.arrayOfArrayOfBools));
        assertBarEquals(foo1.bar, foo2.bar);
        assertTrue(Arrays.equals(foo1.data, foo2.data));
        TestCase.assertTrue(Arrays.deepEquals(foo1.multiArrayOfStrings, foo2.multiArrayOfStrings));
        assertEquals(foo1.source, foo2.source);
        TestCase.assertTrue(Arrays.deepEquals(foo1.inputStreams, foo2.inputStreams));
        TestCase.assertTrue(Arrays.deepEquals(foo1.outputStreams, foo2.outputStreams));
        if (foo1.extraBars != foo2.extraBars) {
            assertEquals(foo1.extraBars.length, foo2.extraBars.length);
            for (int i = 0; i < foo1.extraBars.length; ++i) {
                assertBarEquals(foo1.extraBars[i], foo2.extraBars[i]);
            }
        }
    }

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
        checkConstantField(Color.class.getField("RED"), int.class, 0);
        checkConstantField(Color.class.getField("BLACK"), int.class, 1);

        checkConstantField(Enum.class.getField("VALUE"), int.class, 0);

        checkConstantField(Shape.class.getField("RECTANGLE"), int.class, 1);
        checkConstantField(Shape.class.getField("CIRCLE"), int.class, 2);
        checkConstantField(Shape.class.getField("TRIANGLE"), int.class, 3);
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
        checkField(DefaultsTest.class.getField("a5"), int.class, test, (int) 3456789012L);
        checkField(DefaultsTest.class.getField("a6"), long.class, test, -111111111111L);
        // -8446744073709551617 == 9999999999999999999 - 2 ^ 64.
        checkField(DefaultsTest.class.getField("a7"), long.class, test, -8446744073709551617L);
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
        checkField(DefaultsTest.class.getField("a20"), int.class, test, Bar.Type.BOTH);
        checkField(DefaultsTest.class.getField("a21"), Point.class, test, null);

        assertNotNull(test.a22);
        checkField(DefaultsTest.class.getField("a22"), Thing.class, test, test.a22);
        checkField(DefaultsTest.class.getField("a23"), long.class, test, -1L);
        checkField(DefaultsTest.class.getField("a24"), long.class, test, 0x123456789L);
        checkField(DefaultsTest.class.getField("a25"), long.class, test, -0x123456789L);
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

    /**
     * Testing serialization of the Foo class.
     */
    @SmallTest
    public void testFooSerialization() {
        // Checking serialization and deserialization of a Foo object.
        Foo typicalFoo = createFoo();
        Message serializedFoo = typicalFoo.serialize(null);
        Foo deserializedFoo = Foo.deserialize(serializedFoo);
        assertFooEquals(typicalFoo, deserializedFoo);
    }

    /**
     * Testing serialization of the EmptyStruct class.
     */
    @SmallTest
    public void testEmptyStructSerialization() {
        // Checking serialization and deserialization of a EmptyStruct object.
        Message serializedStruct = new EmptyStruct().serialize(null);
        EmptyStruct emptyStruct = EmptyStruct.deserialize(serializedStruct);
        assertNotNull(emptyStruct);
    }

}
