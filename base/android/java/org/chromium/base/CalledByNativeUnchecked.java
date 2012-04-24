// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

// It only makes sense to use this annotation on methods that declare a throws... spec.
// However, note that the exception received native side maybe an 'unchecked' (RuntimeExpception)
// such as NullPointerException, so the native code should differentiate these cases.
// Usage of this should be very rare; where possible handle exceptions in the Java side and use a
// return value to indicate success / failure.

@Target(ElementType.METHOD)
@Retention(RetentionPolicy.RUNTIME)
public @interface CalledByNativeUnchecked {
    public String value() default "";
}
