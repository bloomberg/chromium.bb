// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.Manifest;
import android.content.pm.PackageManager;

import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.chrome.browser.externalauth.ExternalAuthUtils;
import org.chromium.content.app.ContentApplication;
import org.chromium.content.browser.BrowserStartupController;

/**
 * App-specific version of ChromeBrowserProvider that deals with app-side initialization,
 * notifications and operations depending on the sync status.
 *
 * TODO(newt): merge with ChromeBrowserProvider after upstreaming.
 */
public class ChromeBrowserProviderStaging extends ChromeBrowserProvider {

    @Override
    public boolean onCreate() {
        ContentApplication.initCommandLine(getContext());

        BrowserStartupController.get(getContext(), LibraryProcessType.PROCESS_BROWSER)
                .addStartupCompletedObserver(
                        new BrowserStartupController.StartupCallback() {
                            @Override
                            public void onSuccess(boolean alreadyStarted) {
                                // TODO(knn): Investigate why this is required.
                                ensureNativeChromeLoadedOnUIThread();
                            }

                            @Override
                            public void onFailure() {
                            }
                        });

        return super.onCreate();
    }

    @SuppressFBWarnings("DM_EXIT")
    @Override
    protected boolean ensureNativeChromeLoadedOnUIThread() {
        if (isNativeSideInitialized()) return true;
        // Only start the GoogleServicesManager if we were initialized via a call to the browser
        // provider.  If it wasn't, let the Chrome browser worry about sequencing the
        // initialization.
        boolean shouldStartGoogleServicesManager = mContentProviderApiCalled;
        try {
            ((ChromiumApplication) getContext().getApplicationContext())
                    .startBrowserProcessesAndLoadLibrariesSync(shouldStartGoogleServicesManager);
        } catch (ProcessInitException e) {
            // Chrome browser runs in the background, so exit silently; but do exit, since
            // otherwise the next attempt to use Chrome will find a broken JNI.
            System.exit(-1);
        }
        return super.ensureNativeChromeLoadedOnUIThread();
    }

    @Override
    protected boolean hasReadAccess() {
        return hasPermission(Manifest.permission.READ_HISTORY_BOOKMARKS);
    }

    @Override
    protected boolean hasWriteAccess() {
        return hasPermission(Manifest.permission.WRITE_HISTORY_BOOKMARKS);
    }

    private boolean hasPermission(String permission) {
        boolean hasPermission = getContext().checkCallingOrSelfPermission(permission)
                == PackageManager.PERMISSION_GRANTED;
        boolean isSystemOrGoogleCaller = ExternalAuthUtils.getInstance().isCallerValid(
                getContext(), ExternalAuthUtils.FLAG_SHOULD_BE_GOOGLE_SIGNED
                        | ExternalAuthUtils.FLAG_SHOULD_BE_SYSTEM);
        return hasPermission || isSystemOrGoogleCaller;
    }
}
