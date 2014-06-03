// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import android.test.suitebuilder.annotation.SmallTest;

import junit.framework.TestCase;

import org.chromium.mojo.bindings.test.sample.InterfaceConstants;
import org.chromium.mojo.bindings.test.sample.SampleServiceConstants;

import java.lang.reflect.Field;
import java.lang.reflect.Modifier;

/**
 * Testing generated classes and associated features.
 */
public class BindingsTest extends TestCase {

    private static void checkConstantField(Field field, Class<?> expectedClass)
    {
        assertEquals(expectedClass, field.getType());
        assertEquals(Modifier.FINAL, field.getModifiers() & Modifier.FINAL);
        assertEquals(Modifier.STATIC, field.getModifiers() & Modifier.STATIC);
    }

    /**
     * Testing constants are correctly generated.
     */
    @SmallTest
    public void testConstants() throws NoSuchFieldException, SecurityException {
        assertEquals(3, SampleServiceConstants.THREE);
        checkConstantField(SampleServiceConstants.class.getField("THREE"), byte.class);

        assertEquals(4405, InterfaceConstants.LONG);
        checkConstantField(InterfaceConstants.class.getField("LONG"), long.class);
    }

}
