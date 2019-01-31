// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.base.task.TaskPriority;
import org.chromium.base.task.TaskTraits;
import org.chromium.content_public.browser.BrowserTaskExecutor;

/**
 * Provides the implementation needed in UiThreadTaskTraits.
 */
public class UiThreadTaskTraitsImpl {
    private UiThreadTaskTraitsImpl() {}

    // Corresponds to content::BrowserTaskTraitsExtension.
    public static final byte EXTENSION_ID = 1;

    // Keep in sync with content::BrowserTaskTraitsExtension::Serialize.
    private static final byte NESTING_INDEX = 1;

    private static final byte[] sDefaultExtensionData = getDefaultExtesionData();

    public static final TaskTraits DEFAULT = new TaskTraits(EXTENSION_ID, sDefaultExtensionData);
    public static final TaskTraits BEST_EFFORT = DEFAULT.taskPriority(TaskPriority.BEST_EFFORT);
    public static final TaskTraits USER_VISIBLE = DEFAULT.taskPriority(TaskPriority.USER_VISIBLE);
    public static final TaskTraits USER_BLOCKING = DEFAULT.taskPriority(TaskPriority.USER_BLOCKING);

    static {
        BrowserTaskExecutor.register();
    }

    private static byte[] getDefaultExtesionData() {
        byte extensionData[] = new byte[TaskTraits.EXTENSION_STORAGE_SIZE];

        // Note we don't specify the UI thread directly here because it's ID 0 and the array is
        // initialized to zero.

        // TODO(crbug.com/876272) Remove this if possible.
        extensionData[NESTING_INDEX] = 1; // Allow the task to run in a nested RunLoop.
        return extensionData;
    }
}
