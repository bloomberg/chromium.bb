// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import android.app.Activity;
import android.test.ActivityInstrumentationTestCase2;

import org.chromium.base.test.util.parameter.BaseParameter;
import org.chromium.base.test.util.parameter.Parameter;
import org.chromium.base.test.util.parameter.Parameterizable;
import org.chromium.base.test.util.parameter.parameters.MethodParameter;

import java.util.HashMap;
import java.util.Map;

/**
 * Base class for all Activity-based Instrumentation tests.
 *
 * This class currently does nothing.
 *
 * @param <T> The Activity type.
 */
public class BaseActivityInstrumentationTestCase<T extends Activity>
        extends ActivityInstrumentationTestCase2<T> implements Parameterizable {
    private Parameter.Reader mParameterReader;

    /**
     * Creates a instance for running tests against an Activity of the given class.
     *
     * @param activityClass The type of activity that will be tested.
     */
    public BaseActivityInstrumentationTestCase(Class<T> activityClass) {
        super(activityClass);
    }

    /**
     * Creates the list of available parameters that inherited classes can use.
     *
     * @return a list of {@link BaseParameter} objects to set as the available parameters.
     */
    public Map<String, BaseParameter> getAvailableParameters() {
        Map<String, BaseParameter> parameters = new HashMap<>();
        parameters.put(MethodParameter.PARAMETER_TAG, new MethodParameter(getParameterReader()));
        return parameters;
    }

    /**
     * Setter method for {@link Parameter.Reader}.
     *
     * @param parameterReader the {@link Parameter.Reader} to set.
     */
    public void setParameterReader(Parameter.Reader parameterReader) {
        mParameterReader = parameterReader;
    }

    /**
     * Getter method for {@link Parameter.Reader} object to be used by test cases reading the
     * parameter.
     *
     * @return the {@link Parameter.Reader} for the current {@link
     * org.chromium.base.test.util.parameter.ParameterizedTest} being run.
     */
    protected Parameter.Reader getParameterReader() {
        return mParameterReader;
    }
}
