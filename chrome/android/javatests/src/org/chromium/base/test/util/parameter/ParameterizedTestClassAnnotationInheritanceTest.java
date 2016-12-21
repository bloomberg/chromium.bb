// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util.parameter;

import android.support.test.filters.SmallTest;

import org.chromium.base.test.util.parameter.parameters.MethodParameter;

/**
 * Tester class for inheritance of the {@link ParameterizedTest} annotation applied to a TestCase
 * class.
 */
public class ParameterizedTestClassAnnotationInheritanceTest extends
        ParameterizedTestClassAnnotationTest {
    public ParameterizedTestClassAnnotationInheritanceTest() {
        super();
    }

    @SmallTest
    public void testNoParameterizedTestAnnotation() {
        super.testNoParameterizedTestAnnotation();
    }

    @SmallTest
    @ParameterizedTest()
    public void testEmptyParameterizedTestAnnotation() {
        super.testEmptyParameterizedTestAnnotation();
    }

    @SmallTest
    @ParameterizedTest(parameters = {
            @Parameter(tag = MethodParameter.PARAMETER_TAG,
                    arguments = {@Parameter.Argument(name = "string", stringVar = "value")})})
    public void testMethodParametersWithOneStringValue() {
        super.testMethodParametersWithOneStringValue();
    }

    @SmallTest
    @ParameterizedTest.Set(tests = {
            @ParameterizedTest(parameters = {
                    @Parameter(
                            tag = MethodParameter.PARAMETER_TAG,
                            arguments = {
                                    @Parameter.Argument(name = "string", stringVar = "s_val")})})})
    public void testParameterizedSetOverridesParameterizedTest() {
        super.testParameterizedSetOverridesParameterizedTest();
    }
}
