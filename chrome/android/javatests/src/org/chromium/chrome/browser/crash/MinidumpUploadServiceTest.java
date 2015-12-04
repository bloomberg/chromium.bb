// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Handler;
import android.os.HandlerThread;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.net.NetworkChangeNotifier;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Testcase for {@link MinidumpUploadService}.
 */
public class MinidumpUploadServiceTest extends CrashTestCase {

    private static final int CHECK_INTERVAL_MS = 250;
    private static final int MAX_TIMEOUT_MS = 20000;
    private static final String BOUNDARY = "TESTBOUNDARY";

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testFindAndUploadLastCrash() throws IOException {
        // Setup prerequisites.
        final AtomicInteger numServiceStarts = new AtomicInteger(0);
        final File minidumpFile = new File(mCrashDir, "chromium_renderer-123.dmp");
        MinidumpPreparationContext context = new MinidumpPreparationContext(
                getInstrumentation().getTargetContext()) {
            @Override
            public ComponentName startService(Intent intentToCheck) {
                String filePath =
                        intentToCheck.getStringExtra(MinidumpUploadService.FILE_TO_UPLOAD_KEY);
                assertEquals("Minidump path should be the absolute path",
                        minidumpFile.getAbsolutePath(), filePath);
                assertEquals("Should only call service once", 1,
                        numServiceStarts.incrementAndGet());
                assertEquals("Action should be correct",
                        MinidumpUploadService.ACTION_UPLOAD, intentToCheck.getAction());
                return new ComponentName(getPackageName(), MinidumpUploadService.class.getName());
            }

        };
        MinidumpUploadService service = new TestMinidumpUploadService(context);
        setUpMinidumpFile(minidumpFile, BOUNDARY);

        // Run test.
        Intent findAndUploadLastCrashIntent =
                MinidumpUploadService.createFindAndUploadLastCrashIntent(context);
        service.onCreate();
        findAndUploadLastCrashIntent.putExtra(
                MinidumpUploadService.FINISHED_LOGCAT_EXTRACTION_KEY, true);
        service.onHandleIntent(findAndUploadLastCrashIntent);

        // Verify.
        assertTrue("Minidump file should exist", minidumpFile.isFile());
        assertEquals("Should have called startService() once", 1, numServiceStarts.intValue());
    }

    private static class TestMinidumpUploadService extends MinidumpUploadService {
        private TestMinidumpUploadService() {}
        private TestMinidumpUploadService(Context context) {
            attachBaseContext(context);
        }

        private void attachBaseContextLate(Context base) {
            super.attachBaseContext(base);
        }
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testFindAndUploadLastCrashNoFile() {
        // Setup prerequisites.
        MinidumpPreparationContext context = new MinidumpPreparationContext(
                getInstrumentation().getTargetContext()) {
            @Override
            public ComponentName startService(Intent intentToCheck) {
                fail("Should not start service");
                return new ComponentName(getPackageName(), MinidumpUploadService.class.getName());
            }

        };
        MinidumpUploadService service = new TestMinidumpUploadService(context);

        // Run test.
        Intent findAndUploadLastCrashIntent =
                MinidumpUploadService.createFindAndUploadLastCrashIntent(context);
        findAndUploadLastCrashIntent.putExtra(
                MinidumpUploadService.FINISHED_LOGCAT_EXTRACTION_KEY, true);
        service.onCreate();
        service.onHandleIntent(findAndUploadLastCrashIntent);

        // Verification is done by the test NOT failing with fail(...) in the
        // MinidumpPreparationContext startService(...) method.
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testFindAndUploadAllCrashes() throws IOException {
        // Setup prerequisites.
        final AtomicInteger numServiceStarts = new AtomicInteger(0);
        final File[] minidumpFiles = {
            new File(mCrashDir, "chromium_renderer-111.dmp1"),
            new File(mCrashDir, "chromium_renderer-222.dmp2"),
            new File(mCrashDir, "chromium_renderer-333.dmp3"),
        };
        MinidumpPreparationContext context = new MinidumpPreparationContext(
                getInstrumentation().getTargetContext()) {
            @Override
            public ComponentName startService(Intent intentToCheck) {
                String filePath =
                        intentToCheck.getStringExtra(MinidumpUploadService.FILE_TO_UPLOAD_KEY);
                // Assuming numServicesStart value corresponds to minidumpFiles index.
                assertEquals("Minidump path should be the absolute path",
                        minidumpFiles[numServiceStarts.intValue()].getAbsolutePath(), filePath);
                assertTrue("Should not call service more than number of files",
                        numServiceStarts.incrementAndGet() <= minidumpFiles.length);
                assertEquals("Action should be correct",
                        MinidumpUploadService.ACTION_UPLOAD, intentToCheck.getAction());
                return new ComponentName(getPackageName(), MinidumpUploadService.class.getName());
            }

        };
        MinidumpUploadService service = new TestMinidumpUploadService(context);
        for (File minidumpFile : minidumpFiles) {
            setUpMinidumpFile(minidumpFile, BOUNDARY);
        }

        // Run test.
        Intent findAndUploadAllCrashesIntent =
                MinidumpUploadService.createFindAndUploadAllCrashesIntent(context);
        findAndUploadAllCrashesIntent.putExtra(
                MinidumpUploadService.FINISHED_LOGCAT_EXTRACTION_KEY, true);
        service.onCreate();
        service.onHandleIntent(findAndUploadAllCrashesIntent);

        // Verify.
        for (File minidumpFile : minidumpFiles) {
            assertTrue("Minidump file should exist: " + minidumpFile, minidumpFile.isFile());
        }
        assertEquals("Should have called startService() same number of times as there are files",
                minidumpFiles.length, numServiceStarts.intValue());
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testUploadCrash() throws IOException, InterruptedException {
        List<CountedMinidumpUploadCallable> callables =
                new ArrayList<CountedMinidumpUploadCallable>();
        callables.add(new CountedMinidumpUploadCallable(
                "chromium_renderer-111.dmp1",
                MinidumpUploadCallable.UPLOAD_SUCCESS,
                false));
        runUploadCrashTest(callables);
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testUploadCrashWithThreeFails() throws IOException, InterruptedException {
        // Create |MAX_TRIES_ALLOWED| callables.
        final List<CountedMinidumpUploadCallable> callables =
                new ArrayList<CountedMinidumpUploadCallable>();
        for (int i = 0; i < MinidumpUploadService.MAX_TRIES_ALLOWED; i++) {
            callables.add(new CountedMinidumpUploadCallable(
                    "chromium_renderer-111.dmp1" + (i > 0 ? ".try" + i : "") ,
                    MinidumpUploadCallable.UPLOAD_FAILURE,
                    true));
        }
        runUploadCrashTest(callables);
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testUploadCrashWithOneFailWithNetwork() throws IOException, InterruptedException {
        List<CountedMinidumpUploadCallable> callables =
                new ArrayList<CountedMinidumpUploadCallable>();
        callables.add(new CountedMinidumpUploadCallable(
                "chromium_renderer-111.dmp1",
                MinidumpUploadCallable.UPLOAD_FAILURE,
                true));
        callables.add(new CountedMinidumpUploadCallable(
                "chromium_renderer-111.dmp1.try1",
                MinidumpUploadCallable.UPLOAD_SUCCESS,
                true));
        runUploadCrashTest(callables);
    }

    @SmallTest
    @Feature({"Android-AppBase"})
    public void testUploadCrashWithOneFailNoNetwork() throws IOException, InterruptedException {
        List<CountedMinidumpUploadCallable> callables =
                new ArrayList<CountedMinidumpUploadCallable>();
        callables.add(new CountedMinidumpUploadCallable(
                "chromium_renderer-111.dmp1",
                MinidumpUploadCallable.UPLOAD_FAILURE,
                false));
        runUploadCrashTest(callables);
    }

    @SuppressFBWarnings("RV_RETURN_VALUE_IGNORED_BAD_PRACTICE")
    private void runUploadCrashTest(final List<CountedMinidumpUploadCallable> callables)
            throws IOException, InterruptedException {
        // Setup prerequisites.
        // This version of the service overrides the createMinidumpUploadCallable(...) to be able
        // to return fake ones. It also ensures that the service never tries to create a callable
        // too many times.
        TestMinidumpUploadService service = new TestMinidumpUploadService() {
            int mIndex = 0;
            boolean mTriggerNetworkChange = false;

            @Override
            MinidumpUploadCallable createMinidumpUploadCallable(File minidumpFile, File logfile) {
                if (mIndex >= callables.size()) {
                    fail("Should not create callable number " + mIndex);
                }
                CountedMinidumpUploadCallable callable = callables.get(mIndex++);
                if (callable.mTriggerNetworkChange) {
                    mTriggerNetworkChange = true;
                }
                return callable;
            }

            @Override
            protected void onHandleIntent(Intent intent) {
                try {
                    runTestOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            // Set up basically a fake.
                            if (!NetworkChangeNotifier.isInitialized()) {
                                NetworkChangeNotifier.init(getApplicationContext());
                            }
                        }
                    });
                } catch (Throwable t) {
                    t.printStackTrace();
                    fail("Failed to set up NetworkChangeNotifier");
                }

                super.onHandleIntent(intent);

                if (mTriggerNetworkChange) {
                    mTriggerNetworkChange = false;
                    try {
                        runTestOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                NetworkChangeNotifier.setAutoDetectConnectivityState(false);
                                // Quickly force the state to be connected and back to disconnected.
                                // An event should be triggered for retry logics.
                                NetworkChangeNotifier.forceConnectivityState(false);
                                NetworkChangeNotifier.forceConnectivityState(true);
                            }
                        });
                    } catch (Throwable t) {
                        t.printStackTrace();
                        fail("Failed to trigger NetworkChangeNotifier");
                    }
                }
            }
        };
        // Create a context that supports call to startService(...), where it runs the new service
        // calls on a handler thread. We pass in the MinidumpUploadService as an argument so we
        // can call it directly without going through the Android framework.
        final MinidumpPreparationContext context = new MinidumpPreparationContext(
                getInstrumentation().getTargetContext(), service) {
            Handler mHandler;
            {
                HandlerThread handlerThread =
                        new HandlerThread("MinidumpUploadServiceTest Handler Thread");
                handlerThread.start();
                mHandler = new Handler(handlerThread.getLooper());
            }

            @Override
            public ComponentName startService(final Intent intentToCheck) {
                assertTrue(MinidumpUploadService.ACTION_UPLOAD.equals(intentToCheck.getAction())
                        || MinidumpUploadService.ACTION_FIND_ALL.equals(intentToCheck.getAction()));
                // Post to the handler thread to run the retry intent.
                mHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        mService.onHandleIntent(intentToCheck);
                    }
                });
                return new ComponentName(getPackageName(), MinidumpUploadService.class.getName());
            }

        };
        // We need the context before we can attach it to the service, so since Context is
        // dependent on the service, we do this after context creation.
        service.attachBaseContextLate(context);
        // Create the file used for uploading.
        File minidumpFile = new File(mCrashDir, "chromium_renderer-111.dmp1");
        minidumpFile.createNewFile();
        File logfile = new File(mCacheDir, CrashFileManager.CRASH_DUMP_LOGFILE);
        setUpMinidumpFile(minidumpFile, BOUNDARY);

        // Run test.
        Intent uploadIntent =
                MinidumpUploadService.createUploadIntent(context, minidumpFile, logfile);
        uploadIntent.putExtra(
                MinidumpUploadService.FINISHED_LOGCAT_EXTRACTION_KEY, true);
        service.onCreate();
        service.onHandleIntent(uploadIntent);

        // Verify asynchronously.
        CriteriaHelper.pollForCriteria(new Criteria("All callables should have a call-count of 1") {
            @Override
            public boolean isSatisfied() {
                for (CountedMinidumpUploadCallable callable : callables) {
                    if (callable.mCalledCount != 1) {
                        return false;
                    }
                }
                return true;
            }
        }, MAX_TIMEOUT_MS, CHECK_INTERVAL_MS);
    }

    /**
     * This tests how we instantiate crash uploading from Main, so it is just a sanity check.
     */
    @SmallTest
    @Feature({"Android-AppBase"})
    public void testTryUploadAllCrashDumpsHelperMethod() {
        // Setup prerequisites.
        final String startServiceFlag = "startServiceFlag";
        MinidumpPreparationContext context = new MinidumpPreparationContext(
                getInstrumentation().getTargetContext()) {
            @Override
            public ComponentName startService(Intent intentToCheck) {
                assertEquals(MinidumpUploadService.ACTION_FIND_ALL, intentToCheck.getAction());
                setFlag(startServiceFlag);
                return new ComponentName(getPackageName(), MinidumpUploadService.class.getName());
            }
        };

        // Run test.
        MinidumpUploadService.tryUploadAllCrashDumps(context);

        // Verify.
        assertTrue("Should have called startService(...)", context.isFlagSet(startServiceFlag));
    }

    private class MinidumpPreparationContext extends AdvancedMockContext {
        /**
         * Field used in overridden versions of startService() so we can support retries.
         */
        protected MinidumpUploadService mService;

        public MinidumpPreparationContext(Context targetContext) {
            this(targetContext, null);
        }

        public MinidumpPreparationContext(Context targetContext, MinidumpUploadService service) {
            super(targetContext);
            mService = service;
        }

        @Override
        public File getCacheDir() {
            return mCacheDir;
        }
    }

    /**
     * A fake callable, that just counts the number of times it is called.
     *
     * It can be constructed with the wanted return-value of the call()-method.
     */
    private static class CountedMinidumpUploadCallable extends MinidumpUploadCallable {
        private int mCalledCount;
        @MinidumpUploadCallable.MinidumpUploadStatus private final int mResult;
        private final boolean mTriggerNetworkChange;

        /**
         * Creates a fake callable, that just counts the number of times it is called.
         *
         * @param result the value to return from the call()-method.
         * @param networkChange Should trigger a network change after this callable is finished.
         *     This essentially triggers a retry if result is set to fail.
         */
        private CountedMinidumpUploadCallable(
                String fileName, int result, boolean networkChange) {
            super(new File(fileName), null, null, null);
            this.mResult = result;
            this.mTriggerNetworkChange = networkChange;
        }

        @Override
        public Integer call() {
            ++mCalledCount;
            return mResult;
        }
    }
}
