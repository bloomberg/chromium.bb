// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util.parameter;

import android.app.Activity;
import android.support.test.filters.SmallTest;

import org.chromium.base.test.BaseActivityInstrumentationTestCase;
import org.chromium.base.test.util.parameter.parameters.MethodParameter;

/**
 * Tester class for the {@link ParameterizedTest} annotation with parameters
 * applied to a TestCase class.
 */
@ParameterizedTest(parameters = {
        @Parameter(tag = MethodParameter.PARAMETER_TAG,
                arguments = {
                        @Parameter.Argument(name = "c_string", stringVar = "value"),
                        @Parameter.Argument(name = "c_int", intVar = 0)})})
public class ParameterizedTestClassAnnotationParametersTest extends
        BaseActivityInstrumentationTestCase<Activity> {
    public ParameterizedTestClassAnnotationParametersTest() {
        super(Activity.class);
    }

    @SmallTest
    public void testClassParametersFromClassAnnotation() {
        String expectedString = "value";
        String actualString = getArgument("c_string").stringVar();
        assertEquals(mismatchMessage("c_string"), expectedString, actualString);
        int expectedInt = 0;
        int actualInt = getArgument("c_int").intVar();
        assertEquals(mismatchMessage("c_int"), expectedInt, actualInt);
    }

    @SmallTest
    @ParameterizedTest(parameters = {
            @Parameter(tag = MethodParameter.PARAMETER_TAG,
                    arguments = {@Parameter.Argument(name = "m_string", stringVar = "value")})})
    public void testMethodParametersWithOneStringValue() {
        // Method parameter overrides class parameter.
        assertNotNull("m_string parameter should exist", getArgument("m_string"));
        String expected = "value";
        String actual = getArgument("m_string").stringVar();
        assertEquals(mismatchMessage("m_string"), expected, actual);
        assertNull("c_string parameter should not exist", getArgument("c_string"));
        assertNull("c_int parameter should not exist", getArgument("c_int"));
    }

    private static String mismatchMessage(String name) {
        return String.format("The ParameterArgument %s does not match expected value.", name);
    }

    private Parameter.Argument getArgument(String name) {
        return getParameterReader().getParameterArgument(MethodParameter.PARAMETER_TAG, name);
    }
}
