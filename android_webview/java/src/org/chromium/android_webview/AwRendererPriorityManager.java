// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.android_webview.renderer_priority.RendererPriority;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content.browser.ChildProcessLauncher;

/**
 * Exposes an interface via which native code can manage the priority
 * of a renderer process.
 */
@JNINamespace("android_webview")
public class AwRendererPriorityManager {
    @CalledByNative
    private static void setRendererPriority(
            int pid, @RendererPriority.RendererPriorityEnum int rendererPriority) {
        // TODO(tobiasjs): handle RendererPriority.LOW separately from WAIVED.
        ChildProcessLauncher.setInForeground(pid, rendererPriority == RendererPriority.HIGH);
    }
}
