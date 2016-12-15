// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.test.util;

import android.app.Instrumentation;
import android.content.Context;

import org.chromium.content.app.ContentApplication;

/**
 * Test utilities for dealing with applications.
 */
public class ApplicationUtils {

    /**
     * Wait for the library dependencies to be initialized for the application being
     * instrumented.
     *
     * @param instrumentation The test instrumentation.
     */
    public static void waitForLibraryDependencies(final Instrumentation instrumentation) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Context context = instrumentation.getTargetContext();
                if (context == null) return false;
                if (context.getApplicationContext() == null) return false;
                ContentApplication application =
                        (ContentApplication) context.getApplicationContext();
                return application.areLibraryDependenciesInitialized();
            }
        });
    }

}
