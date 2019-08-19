// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.observers;

import android.app.Activity;

import org.chromium.components.module_installer.ModuleInstaller;

import java.util.List;

/** Interface outlining the necessary strategy to load activities and install modules. */
interface ObserverStrategy {
    public ModuleInstaller getModuleInstaller();
    public List<Activity> getRunningActivities();
    public int getStateForActivity(Activity activity);
}