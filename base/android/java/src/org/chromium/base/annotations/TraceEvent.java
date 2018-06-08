// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.annotations;

import java.lang.annotation.Documented;
import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Any method annotated with a @Trace annotation will have its method wrapped
 * with a call to {@link org.chromium.base.TraceEvent#begin(String)} and
 * {@link org.chromium.base.TraceEvent#end(String)}.
 *
 * <pre>
 *   class Foo {
 *     &#64;TraceEvent
 *     private void bar() {
 *       doStuff();
 *     }
 *   }
 * </pre>
 *
 * Will generate:
 *
 * <pre>
 *   class Foo {
 *     &#64;TraceEvent
 *     private void bar() {
 *       TraceEvent.begin("Foo::bar");
 *       try {
 *          doStuff();
 *          TraceEvent.end("Foo::bar");
 *       } catch (Throwable e) {
 *          TraceEvent.end("bar");
 *          throw e;
 *       }
 *     }
 *   }
 * </pre>
 */
@Documented
@Retention(RetentionPolicy.RUNTIME)
@Target(ElementType.METHOD)
public @interface TraceEvent {}