// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

/**
 * This class lists all the unique task IDs used around in Chromium. These are listed here to ensure
 * that there is no overlap of task IDs between different users of the BackgroundTaskScheduler.
 */
public final class TaskIds {
    public static final int TEST = 0x00008378;

    private TaskIds() {}
}
