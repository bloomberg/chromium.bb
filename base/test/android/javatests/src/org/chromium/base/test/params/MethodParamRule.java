// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.params;

import org.junit.rules.MethodRule;
import org.junit.runners.model.FrameworkMethod;
import org.junit.runners.model.Statement;

import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;

import java.util.List;

/**
 * Abstract base class for rules that are applied to test methods using
 * {@link org.chromium.base.test.params.ParameterAnnotations.MethodParameter method parameters}.
 */
public abstract class MethodParamRule implements MethodRule {
    @Override
    public Statement apply(final Statement base, FrameworkMethod method, Object target) {
        UseMethodParameter useMethodParameter = method.getAnnotation(UseMethodParameter.class);
        if (useMethodParameter == null) return base;
        String parameterName = useMethodParameter.value();

        if (!(method instanceof ParameterizedFrameworkMethod)) return base;
        ParameterSet parameters = ((ParameterizedFrameworkMethod) method).getParameterSet();
        List<Object> values = parameters.getValues();

        return applyParameterAndValues(base, target, parameterName, values);
    }

    protected abstract Statement applyParameterAndValues(
            final Statement base, Object target, String parameterName, List<Object> values);
}
