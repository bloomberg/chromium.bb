// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import org.junit.runners.model.InitializationError;
import org.robolectric.internal.bytecode.InstrumentationConfiguration;

import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Custom Robolectric test runner that instruments the com.google.android.gms.gcm package
 * so Shadows of those classes can be created.
 */
public class OfflinePageTestRunner extends LocalRobolectricTestRunner {

    /**
     * OfflinePageTestRunner constructor
     */
    public OfflinePageTestRunner(Class<?> testClass) throws InitializationError {
        super(testClass);
    }

    @Override
    public InstrumentationConfiguration createClassLoaderConfig() {
        InstrumentationConfiguration.Builder builder = InstrumentationConfiguration.newBuilder();
        builder.addInstrumentedPackage("com.google.android.gms.common");
        builder.addInstrumentedPackage("com.google.android.gms.gcm");
        return builder.build();
    }
}
