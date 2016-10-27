// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.app.NotificationManager;
import android.content.Context;
import android.os.Environment;
import android.text.TextUtils;
import android.util.LongSparseArray;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Manual;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.offlinepages.evaluation.OfflinePageEvaluationBridge;
import org.chromium.chrome.browser.offlinepages.evaluation.OfflinePageEvaluationBridge.OfflinePageEvaluationObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeActivityTestCaseBase;
import org.chromium.components.offlinepages.BackgroundSavePageResult;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.OutputStreamWriter;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/**
 * Tests OfflinePageBridge.SavePageLater over a batch of urls.
 * Tests against a list of top EM urls, try to call SavePageLater on each of the url. It also
 * record metrics (failure rate, time elapsed etc.) by writing metrics to a file on external
 * storage.
 * */
public class OfflinePageSavePageLaterEvaluationTest
        extends ChromeActivityTestCaseBase<ChromeActivity> {
    /**
     * Class which is used to calculate time difference.
     */
    static class TimeDelta {
        public void setStartTime(Long startTime) {
            mStartTime = startTime;
        }
        public void setEndTime(Long endTime) {
            mEndTime = endTime;
        }
        // Return time delta in milliseconds.
        public Long getTimeDelta() {
            return mEndTime - mStartTime;
        }

        private Long mStartTime, mEndTime;
    }

    static class RequestMetadata {
        public long mId;
        public OfflinePageItem mPage;
        public int mStatus;
        public TimeDelta mTimeDelta;
        public String mUrl;
    }

    private static final String TAG = "OPSPLEvaluation";
    private static final String NAMESPACE = "async_loading";
    private static final String NEW_LINE = System.getProperty("line.separator");
    private static final String INPUT_FILE_PATH = "paquete/offline_eval_urls.txt";
    private static final String RESULT_OUTPUT_FILE_PATH = "paquete/offline_eval_results.txt";
    private static final String LOG_OUTPUT_FILE_PATH = "paquete/offline_eval_logs.txt";
    private static final int PAGE_MODEL_LOAD_TIMEOUT_MS = 5000;
    private static final int GET_PAGES_TIMEOUT_MS = 5000;
    private static final int TIMEOUT_PER_URL_USUAL_CASE = 180;
    private static final int TIMEOUT_PER_URL_AUTO_SCHEDULE = 24 * 60 * 60;
    private static final String DELIMITER = ";";
    private static final long DEFAULT_TIMEOUT_PER_URL_IN_SECONDS = 180L;

    private OfflinePageEvaluationBridge mBridge;
    private OfflinePageEvaluationObserver mObserver;

    private Semaphore mDoneSemaphore;
    private List<String> mUrls;
    private int mCount;
    private boolean mIsUserRequested;
    private boolean mUseTestScheduler;

    private LongSparseArray<RequestMetadata> mRequestMetadata;
    private Long mTimeoutPerUrlInSeconds = 0L;
    private String mInputFilePath = INPUT_FILE_PATH;
    private String mResultOutputFilePath = RESULT_OUTPUT_FILE_PATH;
    private String mLogOutputFilePath = LOG_OUTPUT_FILE_PATH;
    private OutputStreamWriter mLogOutput;

    public OfflinePageSavePageLaterEvaluationTest() {
        super(ChromeActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mRequestMetadata = new LongSparseArray<RequestMetadata>();
        mCount = 0;
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
        NotificationManager notificationManager =
                (NotificationManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.NOTIFICATION_SERVICE);
        notificationManager.cancelAll();
    }

    @Override
    public void startMainActivity() throws InterruptedException {
        startMainActivityOnBlankPage();
    }

    /**
     * Get a reader for a given input file path.
     */
    private BufferedReader getInputStream(String inputFilePath) throws FileNotFoundException {
        FileReader fileReader =
                new FileReader(new File(Environment.getExternalStorageDirectory(), inputFilePath));
        BufferedReader bufferedReader = new BufferedReader(fileReader);
        return bufferedReader;
    }

    /**
     * Get a writer for given output file path.
     */
    private OutputStreamWriter getOutputStream(String outputFilePath) throws IOException {
        File outputFile = new File(Environment.getExternalStorageDirectory(), outputFilePath);
        return new FileWriter(outputFile);
    }

    /**
     * Logs error in both console and output file.
     */
    private void logError(String error) {
        Log.e(TAG, error);
        if (mLogOutput != null) {
            try {
                mLogOutput.write(error);
            } catch (Exception e) {
                Log.e(TAG, e.getMessage(), e);
            }
        }
    }

    /**
     * Initializes the evaluation bridge which will be used.
     * @param useCustomScheduler True if customized scheduler (the one with immediate scheduling)
     *                           will be used. False otherwise.
     */
    private void initializeBridgeForProfile(final boolean useTestingScheduler)
            throws InterruptedException {
        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Profile profile = Profile.getLastUsedProfile();
                mBridge = OfflinePageEvaluationBridge.getForProfile(profile, useTestingScheduler);
                if (mBridge == null) {
                    logError("OfflinePageEvaluationBridge initialization failed!");
                    return;
                }
                if (mBridge.isOfflinePageModelLoaded()) {
                    semaphore.release();
                    return;
                }
                mBridge.addObserver(new OfflinePageEvaluationObserver() {
                    @Override
                    public void offlinePageModelLoaded() {
                        semaphore.release();
                        mBridge.removeObserver(this);
                    }
                });
            }
        });
        assertTrue(semaphore.tryAcquire(PAGE_MODEL_LOAD_TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    /**
     * Set up the input/output, bridge and observer we're going to use.
     * @param useCustomScheduler True if customized scheduler (the one with immediate scheduling)
     *                           will be used. False otherwise.
     */
    protected void setUpIOAndBridge(final boolean useCustomScheduler) throws InterruptedException {
        // TODO(romax): Get the file urls from command line/user input.
        try {
            mLogOutput = getOutputStream(mLogOutputFilePath);
        } catch (IOException e) {
            Log.wtf(TAG, "Cannot set output file!");
            Log.wtf(TAG, e.getMessage(), e);
        }
        try {
            getUrlListFromInputFile(mInputFilePath);
        } catch (IOException e) {
            Log.wtf(TAG, "Cannot read input file!");
            Log.wtf(TAG, e.getMessage(), e);
        }
        assertTrue("URLs weren't loaded.", mUrls != null);
        assertFalse("No valid URLs in the input file.", mUrls.size() == 0);

        initializeBridgeForProfile(useCustomScheduler);
        mObserver = new OfflinePageEvaluationObserver() {
            public void savePageRequestAdded(SavePageRequest request) {
                RequestMetadata metadata = new RequestMetadata();
                metadata.mId = request.getRequestId();
                metadata.mUrl = request.getUrl();
                metadata.mStatus = -1;
                TimeDelta timeDelta = new TimeDelta();
                timeDelta.setStartTime(System.currentTimeMillis());
                metadata.mTimeDelta = timeDelta;
                mRequestMetadata.put(request.getRequestId(), metadata);
            }
            public void savePageRequestCompleted(SavePageRequest request, int status) {
                RequestMetadata metadata = mRequestMetadata.get(request.getRequestId());
                metadata.mTimeDelta.setEndTime(System.currentTimeMillis());
                if (metadata.mStatus == -1) {
                    mCount++;
                }
                metadata.mStatus = status;
                if (mCount == mUrls.size()) {
                    mDoneSemaphore.release();
                    return;
                }
            }
            public void savePageRequestChanged(SavePageRequest request) {}
        };
        mBridge.addObserver(mObserver);
    }

    /**
     * Calls SavePageLater on the bridge to try to offline an url.
     * @param url The url to be saved.
     * @param namespace The namespace this request belongs to.
     */
    private void savePageLater(final String url, final String namespace)
            throws InterruptedException {
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mBridge.savePageLater(url, namespace, mIsUserRequested);
            }
        });
    }

    private void processUrls(List<String> urls) throws InterruptedException, IOException {
        if (mBridge == null) {
            logError("Test initialization error, aborting. No results would be written.");
            return;
        }
        mDoneSemaphore = new Semaphore(0);
        // If no timeout value is given, set 180 seconds for each url as default.
        // TODO(romax): Find a way to get network condition and apply different default values.
        if (mTimeoutPerUrlInSeconds == 0) {
            mTimeoutPerUrlInSeconds = DEFAULT_TIMEOUT_PER_URL_IN_SECONDS;
        }
        for (String url : mUrls) {
            savePageLater(url, NAMESPACE);
        }

        if (!mDoneSemaphore.tryAcquire(urls.size() * mTimeoutPerUrlInSeconds, TimeUnit.SECONDS)) {
            writeResults(false);
        } else {
            writeResults(true);
        }
    }

    private void getUrlListFromInputFile(String inputFilePath)
            throws IOException, InterruptedException {
        mUrls = new ArrayList<String>();
        try {
            BufferedReader bufferedReader = getInputStream(inputFilePath);
            try {
                String url;
                while ((url = bufferedReader.readLine()) != null) {
                    if (!TextUtils.isEmpty(url)) {
                        mUrls.add(url);
                    }
                }
            } finally {
                if (bufferedReader != null) {
                    bufferedReader.close();
                }
            }
        } catch (FileNotFoundException e) {
            Log.e(TAG, e.getMessage(), e);
            fail(String.format("URL file %s is not found.", inputFilePath));
        }
    }

    // Translate the int value of status to BackgroundSavePageResult.
    private String statusToString(int status) {
        switch (status) {
            case BackgroundSavePageResult.SUCCESS:
                return "SUCCESS";
            case BackgroundSavePageResult.PRERENDER_FAILURE:
                return "PRERENDER_FAILURE";
            case BackgroundSavePageResult.PRERENDER_CANCELED:
                return "PRERENDER_CANCELED";
            case BackgroundSavePageResult.FOREGROUND_CANCELED:
                return "FOREGROUND_CANCELED";
            case BackgroundSavePageResult.SAVE_FAILED:
                return "SAVE_FAILED";
            case BackgroundSavePageResult.EXPIRED:
                return "EXPIRED";
            case BackgroundSavePageResult.RETRY_COUNT_EXCEEDED:
                return "RETRY_COUNT_EXCEEDED";
            case BackgroundSavePageResult.START_COUNT_EXCEEDED:
                return "START_COUNT_EXCEEDED";
            case BackgroundSavePageResult.REMOVED:
                return "REMOVED";
            case -1:
                return "NOT_COMPLETED";
            default:
                return "UNDEFINED_STATUS";
        }
    }

    /**
     * Get saved offline pages and align them with the metadata we got from testing.
     */
    private void loadSavedPages() throws InterruptedException {
        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mBridge.getAllPages(new Callback<List<OfflinePageItem>>() {
                    @Override
                    public void onResult(List<OfflinePageItem> pages) {
                        for (OfflinePageItem page : pages) {
                            mRequestMetadata.get(page.getOfflineId()).mPage = page;
                        }
                        semaphore.release();
                    }
                });
            }
        });
        assertTrue(semaphore.tryAcquire(GET_PAGES_TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    /**
     * Writes test results to output file. The format would be:
     * URL;OFFLINE_STATUS;FILE_SIZE;TIME_SINCE_TEST_START
     * If page loading failed, size and timestamp would not be written to file.
     * Examples:
     * http://indianrail.gov.in/;START_COUNT_EXCEEDED
     * http://www.21cineplex.com/;SUCCESS;1160 KB;171700
     * https://www.google.com/;SUCCESS;110 KB;273805
     * At the end of the file there will be a summary:
     * Total requested URLs: XX, Completed: XX, Failed: XX, Failure Rate: XX.XX%
     */
    private void writeResults(boolean completed) throws IOException, InterruptedException {
        loadSavedPages();
        OutputStreamWriter output = getOutputStream(mResultOutputFilePath);
        try {
            int failedCount = 0;
            if (!completed) {
                logError("Test terminated before all requests completed." + NEW_LINE);
            }
            for (int i = 0; i < mRequestMetadata.size(); i++) {
                RequestMetadata metadata = mRequestMetadata.valueAt(i);
                long requestId = metadata.mId;
                int status = metadata.mStatus;
                OfflinePageItem page = metadata.mPage;
                if (page == null) {
                    output.write(metadata.mUrl + DELIMITER + statusToString(status) + NEW_LINE);
                    if (status != -1) {
                        failedCount++;
                    }
                    continue;
                }
                output.write(metadata.mUrl + DELIMITER + statusToString(status) + DELIMITER
                        + page.getFileSize() / 1000 + " KB" + DELIMITER
                        + metadata.mTimeDelta.getTimeDelta() + NEW_LINE);
            }
            output.write(String.format(
                    "Total requested URLs: %d, Completed: %d, Failed: %d, Failure Rate: %.2f%%"
                            + NEW_LINE,
                    mUrls.size(), mCount, failedCount, (failedCount * 100.0 / mCount)));
        } catch (FileNotFoundException e) {
            Log.e(TAG, e.getMessage(), e);
        } finally {
            if (output != null) {
                output.close();
            }
        }
    }

    /**
     * The tests would terminate after #urls * mTimeoutPerUrlInSeconds even if some urls are still
     * being processed.
     */
    @Manual
    public void testFailureRateWithTimeoutPerUrl() throws IOException, InterruptedException {
        // TODO(romax) All manual setting of private attributes should be considered moving to a
        // config file or from command-line by user. Also find a better place for default values.
        mTimeoutPerUrlInSeconds = (long) (TIMEOUT_PER_URL_USUAL_CASE);
        mIsUserRequested = true;
        mResultOutputFilePath = RESULT_OUTPUT_FILE_PATH;
        mUseTestScheduler = true;
        // Use testing scheduler.
        setUpIOAndBridge(mUseTestScheduler);
        processUrls(mUrls);
    }

    @Manual
    public void testFailureRate() throws IOException, InterruptedException {
        mTimeoutPerUrlInSeconds = (long) (TIMEOUT_PER_URL_AUTO_SCHEDULE);
        mResultOutputFilePath = RESULT_OUTPUT_FILE_PATH;
        mIsUserRequested = false;
        mUseTestScheduler = true;
        // Use testing scheduler.
        setUpIOAndBridge(mUseTestScheduler);
        processUrls(mUrls);
    }

    @Manual
    public void testFailureRateWithGCMScheduler() throws IOException, InterruptedException {
        mTimeoutPerUrlInSeconds = (long) (TIMEOUT_PER_URL_AUTO_SCHEDULE);
        mResultOutputFilePath = RESULT_OUTPUT_FILE_PATH;
        mIsUserRequested = false;
        mUseTestScheduler = false;
        // Use default scheduler with GCMNetworkManager.
        setUpIOAndBridge(mUseTestScheduler);
        processUrls(mUrls);
    }
}
