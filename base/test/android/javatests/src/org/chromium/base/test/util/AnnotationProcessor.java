// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import android.support.annotation.Nullable;

import org.junit.rules.ExternalResource;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import java.lang.annotation.Annotation;

/**
 * Test rule that is activated when a test or its class has a specific annotation.
 * It allows to run some code before the test (and the {@link org.junit.Before}) runs,
 * and guarantees to also run code after.
 *
 * Usage:
 *
 * <pre>
 * public class Test {
 *    &#64;Rule
 *    public AnnotationProcessor<Foo> rule = new AnnotationProcessor(Foo.class) {
 *          &#64;Override
 *          protected void before() { ... }
 *
 *          &#64;Override
 *          protected void after() { ... }
 *    };
 *
 *    &#64;Test
 *    &#64;Foo
 *    public void myTest() { ... }
 * }
 * </pre>
 *
 * @param <T> type of the annotation to match on the test case.
 */
public abstract class AnnotationProcessor<T extends Annotation> extends ExternalResource {
    private final Class<T> mAnnotationClass;
    private Description mTestDescription;

    @Nullable
    private T mClassAnnotation;

    @Nullable
    private T mTestAnnotation;

    public AnnotationProcessor(Class<T> annotationClass) {
        mAnnotationClass = annotationClass;
    }

    @Override
    public Statement apply(Statement base, Description description) {
        mTestDescription = description;
        mClassAnnotation = description.getTestClass().getAnnotation(mAnnotationClass);
        mTestAnnotation = description.getAnnotation(mAnnotationClass);

        if (mClassAnnotation == null && mTestAnnotation == null) return base;

        // Return the wrapped statement to execute before() and after().
        return super.apply(base, description);
    }

    /** @return {@link Description} of the current test. */
    protected Description getTestDescription() {
        return mTestDescription;
    }

    /** @return the annotation on the test class. */
    @Nullable
    protected T getClassAnnotation() {
        return mClassAnnotation;
    }

    /** @return the annotation on the test method. */
    @Nullable
    protected T getTestAnnotation() {
        return mTestAnnotation;
    }
}
