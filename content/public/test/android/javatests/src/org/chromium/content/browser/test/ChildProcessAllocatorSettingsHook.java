// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.test;

import android.content.Context;

import org.chromium.base.test.BaseTestResult.PreTestHook;
import org.chromium.content.browser.ChildConnectionAllocator;

import java.lang.reflect.Method;

/** PreTestHook used to register the ChildProcessAllocatorSettings annotation. */
public final class ChildProcessAllocatorSettingsHook implements PreTestHook {
    @Override
    public void run(Context targetContext, Method testMethod) {
        ChildProcessAllocatorSettings annotation =
                testMethod.getAnnotation(ChildProcessAllocatorSettings.class);
        if (annotation != null) {
            ChildConnectionAllocator.setSanboxServicesSettingsForTesting(
                    annotation.sandboxedServiceCount(), annotation.sandboxedServiceName());
        }
    }
}
