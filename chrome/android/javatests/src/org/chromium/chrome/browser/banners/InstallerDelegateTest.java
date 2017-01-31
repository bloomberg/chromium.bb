// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.banners;

import android.content.pm.PackageInfo;
import android.os.HandlerThread;
import android.support.test.filters.SmallTest;
import android.test.InstrumentationTestCase;
import android.test.mock.MockPackageManager;

import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

/**
 * Tests the InstallerDelegate to make sure that it functions correctly and responds to changes
 * in the PackageManager.
 */
public class InstallerDelegateTest extends InstrumentationTestCase
        implements InstallerDelegate.Observer{
    private static final String MOCK_PACKAGE_NAME = "mock.package.name";

    /**
     * Returns a mocked set of installed packages.
     */
    public static class TestPackageManager extends MockPackageManager {
        public boolean isInstalled = false;

        @Override
        public PackageInfo getPackageInfo(String packageName, int flags)
                throws NameNotFoundException {
            if (!isInstalled) {
                throw new NameNotFoundException();
            }
            PackageInfo packageInfo = new PackageInfo();
            packageInfo.packageName = MOCK_PACKAGE_NAME;
            return packageInfo;
        }
    }

    private TestPackageManager mPackageManager;
    private InstallerDelegate mTestDelegate;
    private HandlerThread mThread;

    // Variables for tracking the result.
    private boolean mResultFinished;
    private InstallerDelegate mResultDelegate;
    private boolean mResultSuccess;
    private boolean mInstallStarted;

    @Override
    public void onInstallFinished(InstallerDelegate delegate, boolean success) {
        mResultDelegate = delegate;
        mResultSuccess = success;
        mResultFinished = true;
        assertTrue(mInstallStarted);
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();

        mPackageManager = new TestPackageManager();

        // Create a thread for the InstallerDelegate to run on.  We need this thread because the
        // InstallerDelegate's handler fails to be processed otherwise.
        mThread = new HandlerThread("InstallerDelegateTest thread");
        mThread.start();
        mTestDelegate = new InstallerDelegate(
                mThread.getLooper(), mPackageManager, this, MOCK_PACKAGE_NAME);

        // Clear out the results from last time.
        mResultDelegate = null;
        mResultSuccess = false;
        mResultFinished = false;
    }

    @Override
    public void tearDown() throws Exception {
        mThread.quit();
        super.tearDown();
    }

    private void startMonitoring() {
        mTestDelegate.start();
        mInstallStarted = true;
    }

    private void checkResults(boolean expectedResult) {
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return !mTestDelegate.isRunning() && mResultFinished;
            }
        });

        assertEquals(expectedResult, mResultSuccess);
        assertEquals(mTestDelegate, mResultDelegate);
    }

    /**
     * Tests what happens when the InstallerDelegate detects that the package has successfully
     * been installed.
     */
    @SmallTest
    public void testInstallSuccessful() {
        mTestDelegate.setTimingForTests(1, 5000);
        startMonitoring();

        assertFalse(mResultSuccess);
        assertNull(mResultDelegate);
        assertFalse(mResultFinished);

        mPackageManager.isInstalled = true;
        checkResults(true);
    }

    /**
     * Tests what happens when the InstallerDelegate task is canceled.
     */
    @SmallTest
    public void testInstallWaitUntilCancel() {
        mTestDelegate.setTimingForTests(1, 5000);
        startMonitoring();

        assertFalse(mResultSuccess);
        assertNull(mResultDelegate);
        assertFalse(mResultFinished);

        mTestDelegate.cancel();
        checkResults(false);
    }

    /**
     * Tests what happens when the InstallerDelegate times out.
     */
    @SmallTest
    public void testInstallTimeout() {
        mTestDelegate.setTimingForTests(1, 50);
        startMonitoring();
        checkResults(false);
    }

    /**
     * Makes sure that the runnable isn't called until returning from start().
     */
    @SmallTest
    @RetryOnFailure
    public void testRunnableRaceCondition() {
        mPackageManager.isInstalled = true;
        mTestDelegate.setTimingForTests(1, 5000);
        startMonitoring();
        checkResults(true);
    }
}
