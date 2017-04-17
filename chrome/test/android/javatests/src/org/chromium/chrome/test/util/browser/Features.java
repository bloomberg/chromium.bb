// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser;

import org.chromium.base.test.util.AnnotationProcessor;
import org.chromium.chrome.browser.ChromeFeatureList;

import java.lang.annotation.ElementType;
import java.lang.annotation.Inherited;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.HashMap;
import java.util.Map;

/**
 * Annotation used to set Feature flags during JUnit tests. To have an effect, the associated
 * {@link Processor} rule needs to be registered on the test class.
 *
 * Use {@link Features.Register} to specify the features to register and whether they should be
 * enabled.
 *
 * Sample code:
 *
 * <pre>
 * public class Test {
 *    &#64;Rule
 *    public Features.Processor processor = new Features.Processor();
 *
 *    &#64;Features(&#64;Features.Register(ChromeFeatureList.CHROME_HOME))
 *    public void testFoo() { ... }
 * }
 * </pre>
 * // TODO(dgn): Use repeatable annotations (Requires Java 8 / Android N SDK)
 */
@Inherited
@Retention(RetentionPolicy.RUNTIME)
@Target({ElementType.METHOD, ElementType.TYPE})
public @interface Features {
    Register[] value();

    @interface Register {
        String value();
        boolean enabled() default true;
    }

    /**
     * Add this rule to tests to activate the {@link Features} annotations and choose flags
     * to enable, or get rid of exceptions when the production code tries to check for enabled
     * features.
     */
    class Processor extends AnnotationProcessor<Features> {
        public Processor() {
            super(Features.class);
        }

        @Override
        protected void before() throws Throwable {
            Map<String, Boolean> registeredFeatures = new HashMap<>();
            Register[] values = getAnnotation().value();

            for (Register featureState : values) {
                registeredFeatures.put(featureState.value(), featureState.enabled());
            }

            ChromeFeatureList.setTestFeatures(registeredFeatures);
        }

        @Override
        protected void after() {
            ChromeFeatureList.setTestFeatures(null);
        }
    }
    }