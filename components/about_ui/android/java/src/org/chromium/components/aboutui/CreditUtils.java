// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.aboutui;

import org.chromium.base.annotations.JNINamespace;

/** Credits-related utilities. */
@JNINamespace("about_ui")
public class CreditUtils {
    private CreditUtils() {}

    /** Writes the chrome://credits HTML to the given descriptor. */
    public static native void nativeWriteCreditsHtml(int fd);
}
