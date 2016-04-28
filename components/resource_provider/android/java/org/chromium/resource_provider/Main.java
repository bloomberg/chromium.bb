// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.resource_provider;

import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * This class does setup for resource_provider.
 */
@JNINamespace("resource_provider")
public final class Main {
    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "resource_provider";

    private Main() {}

    @SuppressWarnings("unused")
    @CalledByNative
    private static void init(Context context) {
        ContextUtils.initApplicationContext(context.getApplicationContext());
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX, context);
    }
}
