// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.printing;

import android.app.Activity;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.printing.PrintDocumentAdapterWrapper;
import org.chromium.printing.PrintingController;
import org.chromium.printing.PrintingControllerImpl;

/**
 * Creates a {@link PrintingControllerImpl}.
 *
 * Also, sets the default title of {@link TabPrinter}.
 */
public class PrintingControllerFactory {
    public static PrintingController create(Activity activity) {
        if (ApiCompatibilityUtils.isPrintingSupported()) {
            String defaultJobTitle = activity.getResources().getString(R.string.menu_print);
            TabPrinter.setDefaultTitle(defaultJobTitle);

            String errorText = activity.getResources().getString(R.string.error_printing_failed);
            return PrintingControllerImpl.create(new PrintDocumentAdapterWrapper(), errorText);
        }
        return null;
    }
}
