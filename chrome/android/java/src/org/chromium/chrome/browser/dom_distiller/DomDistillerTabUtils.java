// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import org.chromium.base.JNINamespace;
import org.chromium.content_public.browser.WebContents;

/**
 * A helper class for using the DOM Distiller.
 */
@JNINamespace("android")
public class DomDistillerTabUtils {

    private DomDistillerTabUtils() {
    }

    /**
     * Creates a new WebContents and navigates the {@link WebContents} to view the URL of the
     * current page, while in the background starts distilling the current page. This method takes
     * ownership over the old WebContents after swapping in the new one.
     *
     * @param webContents the WebContents to distill.
     */
    public static void distillCurrentPageAndView(WebContents webContents) {
        nativeDistillCurrentPageAndView(webContents);
    }

    private static native void nativeDistillCurrentPageAndView(WebContents webContents);
}
