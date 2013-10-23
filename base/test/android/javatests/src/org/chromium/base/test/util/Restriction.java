// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * An annotation for listing restrictions for a test method. For example, if a test method is only
 * applicable on a phone with small memory:
 *     @Restriction({"Phone", "smallMemory"})
 * Test classes are free to define restrictions and enforce them using reflection at runtime.
 */
@Target(ElementType.METHOD)
@Retention(RetentionPolicy.RUNTIME)
public @interface Restriction {
    /**
     * @return A list of restrictions.
     */
    public String[] value();
}