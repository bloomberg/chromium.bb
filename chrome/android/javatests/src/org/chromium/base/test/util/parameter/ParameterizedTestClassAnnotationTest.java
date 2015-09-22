// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util.parameter;

import android.app.Activity;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.BaseActivityInstrumentationTestCase;
import org.chromium.base.test.util.parameter.parameters.MethodParameter;

/**
 * Tester class for the {@link ParameterizedTest} annotation applied to a TestCase class.
 */
@ParameterizedTest(parameters = {@Parameter(tag = MethodParameter.PARAMETER_TAG)})
public class ParameterizedTestClassAnnotationTest extends
        BaseActivityInstrumentationTestCase<Activity> {
    public ParameterizedTestClassAnnotationTest() {
        super(Activity.class);
    }

    @SmallTest
    public void testNoParameterizedTestAnnotation() {
        // The test is parametrized because the entire test case class is parametrized.
        assertTrue("This is not a parameterized test.", getParameterReader()
                .isParameterizedTest());
        assertEquals(0,
                getParameterReader().getParameter(
                        MethodParameter.PARAMETER_TAG).arguments().length);
        assertNull("someParameter should not exist.", getParameterReader()
                .getParameter("someParameter"));
        assertNull("someParameterArgument should not exist.", getParameterReader()
                .getParameterArgument("someParameter", "someParameterArgument"));
    }

    @SmallTest
    @ParameterizedTest()
    public void testEmptyParameterizedTestAnnotation() {
        assertTrue("This is not a parameterized test.", getParameterReader()
                .isParameterizedTest());
        assertNull("someParameter should not exist.", getParameterReader()
                .getParameter("someParameter"));
        assertNull("someParameterArgument should not exist.", getParameterReader()
                .getParameterArgument("someParameter", "someParameterArgument"));
    }

    @SmallTest
    @ParameterizedTest(parameters = {
            @Parameter(tag = MethodParameter.PARAMETER_TAG,
                    arguments = {@Parameter.Argument(name = "string", stringVar = "value")})})
    public void testMethodParametersWithOneStringValue() {
        if (getArgument("string") != null) {
            // An invocation with a test method parameter.
            String expected = "value";
            String actual = getArgument("string").stringVar();
            assertEquals(mismatchMessage("string"), expected, actual);
        } else {
            assertEquals(0,
                    getParameterReader().getParameter(
                            MethodParameter.PARAMETER_TAG).arguments().length);
        }
        assertNull("someParameter should not exist.", getParameterReader()
                .getParameter("someParameter"));
        assertNull("someParameterArgument should not exist.", getParameterReader()
                .getParameterArgument("someParameter", "someParameterArgument"));
    }

    private static String mismatchMessage(String name) {
        return String.format("The ParameterArgument %s does not match expected value.", name);
    }

    private Parameter.Argument getArgument(String name) {
        return getParameterReader().getParameterArgument(MethodParameter.PARAMETER_TAG, name);
    }
}
