// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FileUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.webapk.lib.common.WebApkConstants;

import java.io.File;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.Callable;

/**
 * Tests that directories for WebappActivities are managed correctly.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class WebappDirectoryManagerTest {
    private static final String TAG = "webapps_WebappDirect";

    private static final String WEBAPP_ID_1 = "webapp_1";
    private static final String WEBAPP_ID_2 = "webapp_2";
    private static final String WEBAPP_ID_3 = "webapp_3";
    private static final String WEBAPK_ID_1 = WebApkConstants.WEBAPK_ID_PREFIX + "webapp_1";
    private static final String WEBAPK_ID_2 = WebApkConstants.WEBAPK_ID_PREFIX + "webapp_2";
    private static final String WEBAPK_ID_3 = WebApkConstants.WEBAPK_ID_PREFIX + "webapp_3";

    private static class TestWebappDirectoryManager extends WebappDirectoryManager {
        private Set<Intent> mBaseIntents = new HashSet<Intent>();

        @Override
        protected Set<Intent> getBaseIntentsForAllTasks() {
            return mBaseIntents;
        }
    }

    private static class WebappMockContext extends AdvancedMockContext {
        public WebappMockContext() {
            super(InstrumentationRegistry.getInstrumentation().getTargetContext());
        }

        /** Returns a directory for test data inside the cache folder. */
        public String getBaseDirectory() {
            File cacheDirectory =
                    InstrumentationRegistry.getInstrumentation().getTargetContext().getCacheDir();
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
            Assert.assertTrue(appDirectory.exists() || appDirectory.mkdirs());
            return appDirectory;
        }
    }

    private WebappMockContext mMockContext;
    private TestWebappDirectoryManager mWebappDirectoryManager;

    @Before
    public void setUp() throws Exception {
        RecordHistogram.setDisabledForTests(true);
        mMockContext = new WebappMockContext();
        mWebappDirectoryManager = new TestWebappDirectoryManager();

        // Set up the base directories.
        File baseDirectory = new File(mMockContext.getBaseDirectory());
        FileUtils.recursivelyDeleteFile(baseDirectory);
        Assert.assertTrue(baseDirectory.mkdirs());
    }

    @After
    public void tearDown() throws Exception {
        FileUtils.recursivelyDeleteFile(new File(mMockContext.getBaseDirectory()));
        RecordHistogram.setDisabledForTests(false);
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testDeletesOwnDirectory() throws Exception {
        File webappDirectory = new File(
                mWebappDirectoryManager.getBaseWebappDirectory(mMockContext), WEBAPP_ID_1);
        Assert.assertTrue(webappDirectory.mkdirs());
        Assert.assertTrue(webappDirectory.exists());

        // Confirm that it deletes the current web app's directory.
        runCleanup();
        Assert.assertFalse(webappDirectory.exists());
    }

    /**
     * On Lollipop and higher, the {@link WebappDirectoryManager} also deletes directories for web
     * apps that no longer correspond to tasks in Recents.
     */
    @Test
    @SmallTest
    @Feature({"Webapps"})
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
        Assert.assertTrue(directory1.mkdirs());
        Assert.assertTrue(directory2.mkdirs());
        Assert.assertTrue(directory3.mkdirs());

        // Indicate that another of the web apps is listed in Recents; in real usage this web app
        // would not be in the foreground and would have persisted its state.
        mWebappDirectoryManager.mBaseIntents.add(
                new Intent(Intent.ACTION_VIEW, Uri.parse("webapp://webapp_2")));

        // Only the directory for the background web app should survive.
        runCleanup();
        Assert.assertFalse(directory1.exists());
        Assert.assertTrue(directory2.exists());
        Assert.assertFalse(directory3.exists());
    }

    /**
     * On Lollipop and higher, the {@link WebappDirectoryManager} also deletes directories for
     * *WebApks* that no longer correspond to tasks in Recents.
     */
    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testDeletesDirectoriesForDeadWebApkTasks() throws Exception {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return;

        // Track the three web app directories.
        File directory1 =
                new File(mWebappDirectoryManager.getBaseWebappDirectory(mMockContext), WEBAPK_ID_1);
        File directory2 =
                new File(mWebappDirectoryManager.getBaseWebappDirectory(mMockContext), WEBAPK_ID_2);
        File directory3 =
                new File(mWebappDirectoryManager.getBaseWebappDirectory(mMockContext), WEBAPK_ID_3);

        // Seed the directory with folders for web apps.
        Assert.assertTrue(directory1.mkdirs());
        Assert.assertTrue(directory2.mkdirs());
        Assert.assertTrue(directory3.mkdirs());

        // Indicate that another of the web apps is listed in Recents; in real usage this web app
        // would not be in the foreground and would have persisted its state.
        mWebappDirectoryManager.mBaseIntents.add(
                new Intent(Intent.ACTION_VIEW, Uri.parse("webapp://webapk-webapp_2")));

        // Only the directory for the background web app should survive.
        runCleanup();
        Assert.assertFalse(directory1.exists());
        Assert.assertTrue(directory2.exists());
        Assert.assertFalse(directory3.exists());
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testDeletesObsoleteDirectories() throws Exception {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return;

        // Seed the base directory with folders that correspond to pre-L web apps.
        File baseDirectory = new File(mMockContext.getBaseDirectory());
        File webappDirectory1 = new File(baseDirectory, "app_WebappActivity1");
        File webappDirectory6 = new File(baseDirectory, "app_WebappActivity6");
        File nonWebappDirectory = new File(baseDirectory, "app_ChromeDocumentActivity");
        Assert.assertTrue(webappDirectory1.mkdirs());
        Assert.assertTrue(webappDirectory6.mkdirs());
        Assert.assertTrue(nonWebappDirectory.mkdirs());

        // Make sure only the web app folders are deleted.
        runCleanup();
        Assert.assertFalse(webappDirectory1.exists());
        Assert.assertFalse(webappDirectory6.exists());
        Assert.assertTrue(nonWebappDirectory.exists());
    }

    private void runCleanup() throws Exception {
        final AsyncTask task =
                mWebappDirectoryManager.cleanUpDirectories(mMockContext, WEBAPP_ID_1);
        CriteriaHelper.pollInstrumentationThread(
                Criteria.equals(AsyncTask.Status.FINISHED, new Callable<AsyncTask.Status>() {
                    @Override
                    public AsyncTask.Status call() {
                        return task.getStatus();
                    }
                }));
    }
}
