// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.content.Context;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.BaseChromiumApplication;
import org.chromium.base.CommandLineInitUtil;
import org.chromium.base.ContextUtils;
import org.chromium.base.DiscardableReferencePool;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.build.BuildHooks;
import org.chromium.build.BuildHooksAndroid;
import org.chromium.build.BuildHooksConfig;
import org.chromium.chrome.browser.crash.PureJavaExceptionHandler;
import org.chromium.chrome.browser.crash.PureJavaExceptionReporter;
import org.chromium.chrome.browser.document.DocumentActivity;
import org.chromium.chrome.browser.document.IncognitoDocumentActivity;
import org.chromium.chrome.browser.init.InvalidStartupDialog;
import org.chromium.chrome.browser.metrics.UmaUtils;
import org.chromium.chrome.browser.tabmodel.document.ActivityDelegateImpl;
import org.chromium.chrome.browser.tabmodel.document.DocumentTabModelSelector;
import org.chromium.chrome.browser.tabmodel.document.StorageDelegate;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;

/**
 * Basic application functionality that should be shared among all browser applications that use
 * chrome layer.
 */
@MainDex
public class ChromeApplication extends BaseChromiumApplication {
    private static final String COMMAND_LINE_FILE = "chrome-command-line";
    private static final String TAG = "ChromiumApplication";

    private static DocumentTabModelSelector sDocumentTabModelSelector;
    private DiscardableReferencePool mReferencePool;

    // Called by the framework for ALL processes. Runs before ContentProviders are created.
    // Quirk: context.getApplicationContext() returns null during this method.
    @Override
    protected void attachBaseContext(Context context) {
        UmaUtils.recordMainEntryPointTime();
        super.attachBaseContext(context);
        ContextUtils.initApplicationContext(this);

        if (ContextUtils.isMainProcess()) {
            // Renderers and GPU process have command line passed to them via IPC
            // (see ChildProcessService.java).
            CommandLineInitUtil.initCommandLine(COMMAND_LINE_FILE);

            // Requires command-line flags.
            TraceEvent.maybeEnableEarlyTracing();
            TraceEvent.begin("ChromeApplication.attachBaseContext");

            // Register for activity lifecycle callbacks. Must be done before any activities are
            // created and is needed only by processes that use the ApplicationStatus api (which for
            // Chrome is just the browser process).
            ApplicationStatus.initialize(this);

            // Only browser process requires custom resources.
            BuildHooksAndroid.initCustomResources(this);

            // Not losing much to not cover the below conditional since it just has simple setters.
            TraceEvent.end("ChromeApplication.attachBaseContext");
        }

        if (!ContextUtils.isIsolatedProcess()) {
            // Incremental install disables process isolation, so things in this block will actually
            // be run for incremental apks, but not normal apks.
            PureJavaExceptionHandler.installHandler();
            if (BuildHooksConfig.REPORT_JAVA_ASSERT) {
                BuildHooks.setReportAssertionCallback(
                        PureJavaExceptionReporter::reportJavaException);
            }
        }
    }

    @Override
    public void onTrimMemory(int level) {
        super.onTrimMemory(level);
        // The conditions are expressed using ranges to capture intermediate levels possibly added
        // to the API in the future.
        if ((level >= TRIM_MEMORY_RUNNING_LOW && level < TRIM_MEMORY_UI_HIDDEN)
                || level >= TRIM_MEMORY_MODERATE) {
            if (mReferencePool != null) mReferencePool.drain();
        }
    }

    /**
     * Shows an error dialog following a startup error, and then exits the application.
     * @param e The exception reported by Chrome initialization.
     */
    public static void reportStartupErrorAndExit(final ProcessInitException e) {
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (ApplicationStatus.getStateForActivity(activity) == ActivityState.DESTROYED) {
            return;
        }
        InvalidStartupDialog.show(activity, e.getErrorCode());
    }

    /**
     * Returns the singleton instance of the DocumentTabModelSelector.
     * TODO(dfalcantara): Find a better place for this once we differentiate between activity and
     *                    application-level TabModelSelectors.
     * @return The DocumentTabModelSelector for the application.
     */
    public static DocumentTabModelSelector getDocumentTabModelSelector() {
        ThreadUtils.assertOnUiThread();
        if (sDocumentTabModelSelector == null) {
            ActivityDelegateImpl activityDelegate = new ActivityDelegateImpl(
                    DocumentActivity.class, IncognitoDocumentActivity.class);
            sDocumentTabModelSelector = new DocumentTabModelSelector(activityDelegate,
                    new StorageDelegate(), new TabDelegate(false), new TabDelegate(true));
        }
        return sDocumentTabModelSelector;
    }

    /**
     * @return The DiscardableReferencePool for the application.
     */
    public DiscardableReferencePool getReferencePool() {
        ThreadUtils.assertOnUiThread();
        if (mReferencePool == null) {
            mReferencePool = new DiscardableReferencePool();
        }
        return mReferencePool;
    }
}
