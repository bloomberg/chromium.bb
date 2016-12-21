// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util.parameter;

import android.app.Activity;
import android.support.test.filters.SmallTest;

import org.chromium.base.test.BaseActivityInstrumentationTestCase;
import org.chromium.base.test.util.parameter.parameters.MethodParameter;

/**
 * Tester class for priority of {@link ParameterizedTest} and {@link ParameterizedTest.Set}
 * annotations applied to a TestCase class.
 */
@ParameterizedTest(parameters = {
        @Parameter(tag = MethodParameter.PARAMETER_TAG,
                arguments = {@Parameter.Argument(name = "string", stringVar = "t_val")})})
@ParameterizedTest.Set(tests = {
        @ParameterizedTest(parameters = {
                @Parameter(
                        tag = MethodParameter.PARAMETER_TAG,
                        arguments = {
                                @Parameter.Argument(name = "string", stringVar = "s_val")})})})
public class ParameterizedTestClassAnnotationParameterSetAndTest extends
        BaseActivityInstrumentationTestCase<Activity> {
    public ParameterizedTestClassAnnotationParameterSetAndTest() {
        super(Activity.class);
    }

    @SmallTest
    public void testParameterizedSetOverridesParameterizedTest() {
        String expected = "s_val";
        String actual = getArgument("string").stringVar();
        assertEquals("Expected the value set via @ParameterizedTest.Set", expected, actual);
    }

    private Parameter.Argument getArgument(String name) {
        return getParameterReader().getParameterArgument(MethodParameter.PARAMETER_TAG, name);
    }
}
