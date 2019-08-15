// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer;

import android.app.Activity;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.VisibleForTesting;

import java.util.List;

/** Dummy fallback of ActivityObserver. */
public class ModuleActivityObserver implements ApplicationStatus.ActivityStateListener {
    @Override
    public void onActivityStateChange(Activity activity, int newState) {
        // Do nothing.
    }

    public void onModuleInstalled() {
        // Do nothing.
    }

    @VisibleForTesting
    public ModuleInstaller getModuleInstaller() {
        return null;
    }

    @VisibleForTesting
    public List<Activity> getRunningActivities() {
        return null;
    }

    @VisibleForTesting
    public int getStateForActivity(Activity activity) {
        return 1;
    }
}