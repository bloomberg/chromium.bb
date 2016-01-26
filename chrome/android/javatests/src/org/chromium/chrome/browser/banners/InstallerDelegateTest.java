// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.banners;

import android.content.pm.PackageInfo;
import android.os.HandlerThread;
import android.test.InstrumentationTestCase;
import android.test.mock.MockPackageManager;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.util.ArrayList;
import java.util.List;

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
        public List<PackageInfo> getInstalledPackages(int flags) {
            List<PackageInfo> packages = new ArrayList<PackageInfo>();

            if (isInstalled) {
                PackageInfo info = new PackageInfo();
                info.packageName = MOCK_PACKAGE_NAME;
                packages.add(info);
            }

            return packages;
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

    private void startMonitoring() throws InterruptedException {
        mTestDelegate.start();
        mInstallStarted = true;
    }

    private void checkResults(boolean expectedResult) throws InterruptedException {
        CriteriaHelper.pollForCriteria(new Criteria() {
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
    public void testInstallSuccessful() throws InterruptedException {
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
    public void testInstallWaitUntilCancel() throws InterruptedException {
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
    public void testInstallTimeout() throws InterruptedException {
        mTestDelegate.setTimingForTests(1, 50);
        startMonitoring();
        checkResults(false);
    }

    /**
     * Makes sure that the runnable isn't called until returning from start().
     */
    @SmallTest
    public void testRunnableRaceCondition() throws InterruptedException {
        mPackageManager.isInstalled = true;
        mTestDelegate.setTimingForTests(1, 5000);
        startMonitoring();
        checkResults(true);
    }
}
