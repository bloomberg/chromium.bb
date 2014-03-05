// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.shell.sync;

import android.app.Application;
import android.content.Context;

import org.chromium.chrome.browser.sync.ChromiumSyncAdapter;
import org.chromium.chrome.shell.ChromeShellApplication;

public class ChromeShellSyncAdapter extends ChromiumSyncAdapter {
    public ChromeShellSyncAdapter(Context appContext, Application application) {
        super(appContext, application);
    }

    @Override
    protected boolean useAsyncStartup() {
        return true;
    }

    @Override
    protected void initCommandLine() {
        ChromeShellApplication.initCommandLine();
    }
}
