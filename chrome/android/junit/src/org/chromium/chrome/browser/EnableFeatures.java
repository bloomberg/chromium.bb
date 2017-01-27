// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.testing.local.AnnotationProcessor;

import java.lang.annotation.ElementType;
import java.lang.annotation.Inherited;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/**
 * Annotation used to set Feature flags during JUnit tests. To have an effect, the associated
 * {@link Processor} rule needs to be registered on the test class.
 *
 * Sample code:
 *
 * <pre>
 * public class Test {
 *    &#64;Rule
 *    public EnableFeatures.Processor processor = new EnableFeatures.Processor();
 *
 *    &#64;EnableFeatures(ChromeFeatureList.NTP_SNIPPETS_OFFLINE_BADGE)
 *    public void testFoo() { ... }
 * }
 * </pre>
 */
@Inherited
@Retention(RetentionPolicy.RUNTIME)
@Target({ElementType.METHOD, ElementType.TYPE})
public @interface EnableFeatures {

    String[] value();

    /**
     * Add this rule to tests to activate the {@link EnableFeatures} annotations and choose flags
     * to enable, or get rid of exceptions when the production code tries to check for enabled
     * features.
     */
    public static class Processor extends AnnotationProcessor<EnableFeatures> {
        public Processor() {
            super(EnableFeatures.class);
        }

        @Override
        protected void before() throws Throwable {
            Set<String> enabledFeatures = new HashSet<>();
            String[] values = getAnnotation().value();
            enabledFeatures.addAll(Arrays.asList(values));
            ChromeFeatureList.setTestEnabledFeatures(enabledFeatures);
        }

        @Override
        protected void after() {
            ChromeFeatureList.setTestEnabledFeatures(null);
        }
    }
}