// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util.parameter;

import android.app.Activity;
import android.support.test.filters.SmallTest;

import org.chromium.base.test.BaseActivityInstrumentationTestCase;
import org.chromium.base.test.util.parameter.parameters.MethodParameter;

/**
 * Tester class for the {@link ParameterizedTest.Set} annotation applied to a TestCase class.
 */
@ParameterizedTest.Set(tests = {
        @ParameterizedTest(parameters = {
                @Parameter(
                        tag = MethodParameter.PARAMETER_TAG,
                        arguments = {
                                @Parameter.Argument(name = "c_string1", stringVar = "testvalue"),
                                @Parameter.Argument(name = "c_string2", stringVar = "blahblah"),
                                @Parameter.Argument(name = "c_int1", intVar = 4),
                                @Parameter.Argument(name = "c_int2", intVar = 0)})})})
public class ParameterizedTestClassAnnotationParametersTestSetTest extends
        BaseActivityInstrumentationTestCase<Activity> {
    public ParameterizedTestClassAnnotationParametersTestSetTest() {
        super(Activity.class);
    }

    @SmallTest
    public void testMethodParametersFromClassAnnotation() {
        assertEquals("c_string1 variable should equals \"testvalue\"", "testvalue",
                getArgument("c_string1").stringVar());
        assertEquals("c_string2 variable should equals \"blahblah\"", "blahblah",
                getArgument("c_string2").stringVar());
        assertEquals("c_int1 variable should equals 4", 4, getArgument("c_int1").intVar());
        assertEquals("c_int2 variable should equals 0", 0, getArgument("c_int2").intVar());
    }

    @SmallTest
    @ParameterizedTest.Set(tests = {
            @ParameterizedTest(parameters = {
                    @Parameter(
                            tag = MethodParameter.PARAMETER_TAG,
                            arguments = {
                                    @Parameter.Argument(name = "m_string", stringVar = "value"),
                                    @Parameter.Argument(name = "m_int", intVar = 0)})})})
    public void testClassAndMethodParameterSets() {
        // Method parameter overrides class parameter.
        assertNotNull("m_string parameter should exist", getArgument("m_string"));
        String expectedString = "value";
        String actualString = getArgument("m_string").stringVar();
        assertEquals(mismatchMessage("m_string"), expectedString, actualString);
        int expectedInt = 0;
        int actualInt = getArgument("m_int").intVar();
        assertEquals(mismatchMessage("m_int"), expectedInt, actualInt);
        assertNull("c_string1 parameter should not exist", getArgument("c_string1"));
        assertNull("c_string2 parameter should not exist", getArgument("c_string2"));
        assertNull("c_int1 parameter should not exist", getArgument("c_int1"));
        assertNull("c_int2 parameter should not exist", getArgument("c_int2"));
    }

    @SmallTest
    @ParameterizedTest(parameters = {
            @Parameter(tag = MethodParameter.PARAMETER_TAG,
                    arguments = {@Parameter.Argument(name = "c_string1", stringVar = "t_val")})})
    public void testParameterizedSetOverridesParameterizedTest() {
        String expected = "testvalue";
        String actual = getArgument("c_string1").stringVar();
        assertEquals("Expected the value set via @ParameterizedTest.Set", expected, actual);
    }

    private static String mismatchMessage(String name) {
        return String.format("The ParameterArgument %s does not match expected value.", name);
    }

    private Parameter.Argument getArgument(String name) {
        return getParameterReader().getParameterArgument(MethodParameter.PARAMETER_TAG, name);
    }
}
