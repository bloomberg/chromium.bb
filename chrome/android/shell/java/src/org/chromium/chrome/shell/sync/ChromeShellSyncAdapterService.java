// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.shell.sync;

import android.app.Application;
import android.content.Context;

import org.chromium.chrome.browser.sync.ChromiumSyncAdapter;
import org.chromium.chrome.browser.sync.ChromiumSyncAdapterService;

public class ChromeShellSyncAdapterService extends ChromiumSyncAdapterService {
    @Override
    protected ChromiumSyncAdapter createChromiumSyncAdapter(
            Context context, Application application) {
        return new ChromeShellSyncAdapter(context, getApplication());
    }
}
