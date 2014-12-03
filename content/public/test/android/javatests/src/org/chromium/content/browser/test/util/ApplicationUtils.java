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
     * @return Whether the library dependencies were initialized.
     */
    public static boolean waitForLibraryDependencies(final Instrumentation instrumentation)
            throws InterruptedException {
        return CriteriaHelper.pollForUIThreadCriteria(new Criteria() {
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
