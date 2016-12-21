// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util.parameter;

import android.support.test.filters.SmallTest;

import org.chromium.base.test.util.parameter.parameters.MethodParameter;

/**
 * Tester class for inheritance of the {@link ParameterizedTest.Set} annotation applied to a
 * TestCase class.
 */
public class ParameterizedTestClassAnnotationParametersTestSetInheritanceTest extends
        ParameterizedTestClassAnnotationParametersTestSetTest {
    public ParameterizedTestClassAnnotationParametersTestSetInheritanceTest() {
        super();
    }

    @SmallTest
    public void testMethodParametersFromClassAnnotation() {
        super.testMethodParametersFromClassAnnotation();
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
        super.testClassAndMethodParameterSets();
    }

    @SmallTest
    @ParameterizedTest(parameters = {
            @Parameter(tag = MethodParameter.PARAMETER_TAG,
                    arguments = {@Parameter.Argument(name = "c_string1", stringVar = "t_val")})})
    public void testParameterizedSetOverridesParameterizedTest() {
        super.testParameterizedSetOverridesParameterizedTest();
    }
}
