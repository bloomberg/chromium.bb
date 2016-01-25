// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Build;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.FileUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

import java.io.File;
import java.util.HashSet;
import java.util.Set;

/**
 * Tests that directories for WebappActivities are managed correctly.
 */
public class WebappDirectoryManagerTest extends InstrumentationTestCase {
    private static final String TAG = "webapps_WebappDirect";

    private static final String WEBAPP_ID_1 = "webapp_1";
    private static final String WEBAPP_ID_2 = "webapp_2";
    private static final String WEBAPP_ID_3 = "webapp_3";

    private static class TestWebappDirectoryManager extends WebappDirectoryManager {
        private Set<Intent> mBaseIntents = new HashSet<Intent>();

        @Override
        protected Set<Intent> getBaseIntentsForAllTasks() {
            return mBaseIntents;
        }
    }

    private class WebappMockContext extends AdvancedMockContext {
        public WebappMockContext() {
            super(getInstrumentation().getTargetContext());
        }

        /** Returns a directory for test data inside the cache folder. */
        public String getBaseDirectory() {
            File cacheDirectory = getInstrumentation().getTargetContext().getCacheDir();
            return new File(cacheDirectory, "WebappDirectoryManagerTest").getAbsolutePath();
        }

        @Override
        public ApplicationInfo getApplicationInfo() {
            ApplicationInfo mockInfo = new ApplicationInfo();
            mockInfo.dataDir = getBaseDirectory();
            return mockInfo;
        }

        @Override
        public File getDir(String name, int mode) {
            File appDirectory = new File(getApplicationInfo().dataDir, "app_" + name);
            assertTrue(appDirectory.exists() || appDirectory.mkdirs());
            return appDirectory;
        }
    }

    private WebappMockContext mMockContext;
    private TestWebappDirectoryManager mWebappDirectoryManager;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        RecordHistogram.disableForTests();
        mMockContext = new WebappMockContext();
        mWebappDirectoryManager = new TestWebappDirectoryManager();

        // Set up the base directories.
        File baseDirectory = new File(mMockContext.getBaseDirectory());
        FileUtils.recursivelyDeleteFile(baseDirectory);
        assertTrue(baseDirectory.mkdirs());
    }

    @Override
    public void tearDown() throws Exception {
        FileUtils.recursivelyDeleteFile(new File(mMockContext.getBaseDirectory()));
        super.tearDown();
    }

    @SmallTest
    public void testDeletesOwnDirectory() throws Exception {
        File webappDirectory = new File(
                mWebappDirectoryManager.getBaseWebappDirectory(mMockContext), WEBAPP_ID_1);
        assertTrue(webappDirectory.mkdirs());
        assertTrue(webappDirectory.exists());

        // Confirm that it deletes the current web app's directory.
        runCleanup();
        assertFalse(webappDirectory.exists());
    }

    /**
     * On Lollipop and higher, the {@link WebappDirectoryManager} also deletes directories for web
     * apps that no longer correspond to tasks in Recents.
     */
    @SmallTest
    public void testDeletesDirectoriesForDeadTasks() throws Exception {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return;

        // Track the three web app directories.
        File directory1 = new File(
                mWebappDirectoryManager.getBaseWebappDirectory(mMockContext), WEBAPP_ID_1);
        File directory2 = new File(
                mWebappDirectoryManager.getBaseWebappDirectory(mMockContext), WEBAPP_ID_2);
        File directory3 = new File(
                mWebappDirectoryManager.getBaseWebappDirectory(mMockContext), WEBAPP_ID_3);

        // Seed the directory with folders for web apps.
        assertTrue(directory1.mkdirs());
        assertTrue(directory2.mkdirs());
        assertTrue(directory3.mkdirs());

        // Indicate that another of the web apps is listed in Recents; in real usage this web app
        // would not be in the foreground and would have persisted its state.
        mWebappDirectoryManager.mBaseIntents.add(
                new Intent(Intent.ACTION_VIEW, Uri.parse("webapp://webapp_2")));

        // Only the directory for the background web app should survive.
        runCleanup();
        assertFalse(directory1.exists());
        assertTrue(directory2.exists());
        assertFalse(directory3.exists());
    }

    @SmallTest
    public void testDeletesObsoleteDirectories() throws Exception {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return;

        // Seed the base directory with folders that correspond to pre-L web apps.
        File baseDirectory = new File(mMockContext.getBaseDirectory());
        File webappDirectory1 = new File(baseDirectory, "app_WebappActivity1");
        File webappDirectory6 = new File(baseDirectory, "app_WebappActivity6");
        File nonWebappDirectory = new File(baseDirectory, "app_ChromeDocumentActivity");
        assertTrue(webappDirectory1.mkdirs());
        assertTrue(webappDirectory6.mkdirs());
        assertTrue(nonWebappDirectory.mkdirs());

        // Make sure only the web app folders are deleted.
        runCleanup();
        assertFalse(webappDirectory1.exists());
        assertFalse(webappDirectory6.exists());
        assertTrue(nonWebappDirectory.exists());
    }

    private void runCleanup() throws Exception {
        final AsyncTask task =
                mWebappDirectoryManager.cleanUpDirectories(mMockContext, WEBAPP_ID_1);
        CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return task.getStatus() == AsyncTask.Status.FINISHED;
            }
        });
    }
}
