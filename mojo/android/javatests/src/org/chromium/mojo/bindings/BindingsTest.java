// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import android.test.suitebuilder.annotation.SmallTest;

import junit.framework.TestCase;

import org.chromium.mojo.bindings.test.imported.Color;
import org.chromium.mojo.bindings.test.imported.Shape;
import org.chromium.mojo.bindings.test.sample.Enum;
import org.chromium.mojo.bindings.test.sample.InterfaceConstants;
import org.chromium.mojo.bindings.test.sample.SampleServiceConstants;

import java.lang.reflect.Field;
import java.lang.reflect.Modifier;

/**
 * Testing generated classes and associated features.
 */
public class BindingsTest extends TestCase {

    private static void checkConstantField(Field field, Class<?> expectedClass) {
        assertEquals(expectedClass, field.getType());
        assertEquals(Modifier.FINAL, field.getModifiers() & Modifier.FINAL);
        assertEquals(Modifier.STATIC, field.getModifiers() & Modifier.STATIC);
    }

    /**
     * Testing constants are correctly generated.
     */
    @SmallTest
    public void testConstants() throws NoSuchFieldException, SecurityException {
        assertEquals(12, SampleServiceConstants.TWELVE);
        checkConstantField(SampleServiceConstants.class.getField("TWELVE"), byte.class);

        assertEquals(4405, InterfaceConstants.LONG);
        checkConstantField(InterfaceConstants.class.getField("LONG"), long.class);
    }

    /**
     * Testing enums are correctly generated.
     */
    @SmallTest
    public void testEnums() throws NoSuchFieldException, SecurityException {
        assertEquals(0, Color.COLOR_RED);
        assertEquals(1, Color.COLOR_BLACK);
        checkConstantField(Color.class.getField("COLOR_BLACK"), int.class);
        checkConstantField(Color.class.getField("COLOR_RED"), int.class);

        assertEquals(0, Enum.ENUM_VALUE);
        checkConstantField(Enum.class.getField("ENUM_VALUE"), int.class);

        assertEquals(1, Shape.SHAPE_RECTANGLE);
        assertEquals(2, Shape.SHAPE_CIRCLE);
        assertEquals(3, Shape.SHAPE_TRIANGLE);
        checkConstantField(Shape.class.getField("SHAPE_RECTANGLE"), int.class);
        checkConstantField(Shape.class.getField("SHAPE_CIRCLE"), int.class);
        checkConstantField(Shape.class.getField("SHAPE_TRIANGLE"), int.class);
    }

}
