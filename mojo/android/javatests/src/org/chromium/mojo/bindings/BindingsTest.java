// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import android.test.suitebuilder.annotation.SmallTest;

import junit.framework.TestCase;

import org.chromium.mojo.bindings.test.sample.SampleServiceConstants;

import java.lang.reflect.Field;
import java.lang.reflect.Modifier;

/**
 * Testing generated classes and associated features.
 */
public class BindingsTest extends TestCase {

    /**
     * Testing constants are correctly generated.
     */
    @SmallTest
    public void testConstants() throws NoSuchFieldException, SecurityException {
        assertEquals(3, SampleServiceConstants.THREE);
        Field threeField = SampleServiceConstants.class.getField("THREE");
        assertEquals(byte.class, threeField.getType());
        assertEquals(Modifier.FINAL, threeField.getModifiers() & Modifier.FINAL);
        assertEquals(Modifier.STATIC, threeField.getModifiers() & Modifier.STATIC);
    }

}
