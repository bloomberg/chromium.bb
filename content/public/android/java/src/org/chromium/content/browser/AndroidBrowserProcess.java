// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;

import org.chromium.base.JNINamespace;
import org.chromium.content.common.ProcessInitException;

@JNINamespace("content") // TODO(nyquist) Remove this class, and move the functionality to
                         // BrowserStartupController.
public class AndroidBrowserProcess {

    public static final int MAX_RENDERERS_LIMIT = BrowserStartupController.MAX_RENDERERS_LIMIT;

    // Temporarily provide the old init, to simplify landing patches
    // TODO(aberent) Remove
    @Deprecated
    public static void init(Context context, int maxRendererProcesses) throws ProcessInitException {
        if (!BrowserStartupController.get(context).startBrowserProcessesSync(
                maxRendererProcesses)) {
            throw new ProcessInitException(1);
        }
    }
}
