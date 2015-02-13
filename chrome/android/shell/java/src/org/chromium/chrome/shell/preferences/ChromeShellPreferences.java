// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.shell.preferences;

import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.content.browser.BrowserStartupController;

/**
 * The Settings activity for Chrome Shell.
 */
public class ChromeShellPreferences extends Preferences {

    @Override
    protected void startBrowserProcessSync() throws ProcessInitException {
        BrowserStartupController.get(this, LibraryProcessType.PROCESS_BROWSER)
                .startBrowserProcessesSync(false);
    }

    @Override
    protected String getTopLevelFragmentName() {
        return ChromeShellMainPreferences.class.getName();
    }

    @Override
    public void showUrl(int titleResId, int urlResId) {
        // Not implemented.
    }
}