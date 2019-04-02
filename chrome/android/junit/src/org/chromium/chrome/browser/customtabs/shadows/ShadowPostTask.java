// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.shadows;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

/**
 * TODO(pshmakov): make a more elaborate shadow, or perhaps a non-static wrapper for PostTask
 * to be injected.
 */
@Implements(PostTask.class)
public class ShadowPostTask {

    @Implementation
    public static void postDelayedTask(TaskTraits taskTraits, Runnable task, long delay) {
    }
}
