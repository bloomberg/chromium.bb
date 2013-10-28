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
 *     @Restriction({RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_SMALL_MEMORY})
 * Test classes are free to define restrictions and enforce them using reflection at runtime.
 */
@Target(ElementType.METHOD)
@Retention(RetentionPolicy.RUNTIME)
public @interface Restriction {
    /** Specifies the test is only valid on phone form factors. */
    public static final String RESTRICTION_TYPE_PHONE = "Phone";

    /** Specifies the test is only valid on tablet form factors. */
    public static final String RESTRICTION_TYPE_TABLET = "Tablet";

    /** Specifies the test is only valid on devices with a small amount of memory. */
    public static final String RESTRICTION_TYPE_SMALL_MEMORY = "Small_Memory";

    /**
     * @return A list of restrictions.
     */
    public String[] value();
}