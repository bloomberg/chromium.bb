// Copyright 2016 The Chromium Authors. All rights reserved.
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
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStreamWriter;

import java.util.ArrayList;
import java.util.List;
import java.util.Properties;
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
    private static final String DELIMITER = ";";
    private static final String CONFIG_FILE_PATH = "paquete/test_config";
    private static final String SAVED_PAGES_EXTERNAL_PATH = "paquete/archives";
    private static final String INPUT_FILE_PATH = "paquete/offline_eval_urls.txt";
    private static final String LOG_OUTPUT_FILE_PATH = "paquete/offline_eval_logs.txt";
    private static final String RESULT_OUTPUT_FILE_PATH = "paquete/offline_eval_results.txt";
    private static final int GET_PAGES_TIMEOUT_MS = 30000;
    private static final int PAGE_MODEL_LOAD_TIMEOUT_MS = 30000;
    private static final int REMOVE_REQUESTS_TIMEOUT_MS = 30000;

    private OfflinePageEvaluationBridge mBridge;
    private OfflinePageEvaluationObserver mObserver;

    private Semaphore mDoneSemaphore;
    private List<String> mUrls;
    private int mCount;
    private boolean mIsUserRequested;
    private boolean mUseTestScheduler;

    private LongSparseArray<RequestMetadata> mRequestMetadata;
    // TODO(romax): Use actual policy to determine the timeout.
    private Long mTimeoutPerUrlInSeconds = 0L;
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
        NotificationManager notificationManager =
                (NotificationManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.NOTIFICATION_SERVICE);
        notificationManager.cancelAll();
        final Semaphore mClearingSemaphore = new Semaphore(0);
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mBridge.getRequestsInQueue(new Callback<SavePageRequest[]>() {
                    @Override
                    public void onResult(SavePageRequest[] results) {
                        ArrayList<Long> ids = new ArrayList<Long>(results.length);
                        for (int i = 0; i < results.length; i++) {
                            ids.add(results[i].getRequestId());
                        }
                        mBridge.removeRequestsFromQueue(ids, new Callback<Integer>() {
                            @Override
                            public void onResult(Integer removedCount) {
                                mClearingSemaphore.release();
                            }
                        });
                    }
                });
            }
        });
        checkTrue(mClearingSemaphore.tryAcquire(REMOVE_REQUESTS_TIMEOUT_MS, TimeUnit.MILLISECONDS),
                "Timed out when clearing remaining requests!");
        super.tearDown();
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
     * Get the directory on external storage for storing saved pages.
     */
    private File getExternalArchiveDir() {
        File externalArchiveDir =
                new File(Environment.getExternalStorageDirectory(), SAVED_PAGES_EXTERNAL_PATH);
        try {
            // Clear the old archive folder.
            if (externalArchiveDir.exists()) {
                String[] files = externalArchiveDir.list();
                if (files != null) {
                    for (String file : files) {
                        File currentFile = new File(externalArchiveDir.getPath(), file);
                        if (!currentFile.delete()) {
                            logError(file + " cannot be deleted when clearing previous archives.");
                        }
                    }
                }
            }
            if (!externalArchiveDir.mkdir()) {
                logError("Cannot create directory on external storage to store saved pages.");
            }
        } catch (SecurityException e) {
            logError("Failed to delete or create external archive folder!");
        }
        return externalArchiveDir;
    }

    /**
     * Logs error in both console and output file.
     */
    private void logError(String error) {
        Log.e(TAG, error);
        if (mLogOutput != null) {
            try {
                mLogOutput.write(error + NEW_LINE);
                mLogOutput.flush();
            } catch (Exception e) {
                Log.e(TAG, e.getMessage(), e);
            }
        }
    }

    /**
     * Assert the condition is true, otherwise abort the test and log.
     */
    private void checkTrue(boolean condition, String message) {
        if (!condition) {
            logError(message);
            if (mLogOutput != null) {
                try {
                    mLogOutput.close();
                } catch (IOException e) {
                    Log.e(TAG, e.getMessage(), e);
                }
            }
            fail();
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
        checkTrue(semaphore.tryAcquire(PAGE_MODEL_LOAD_TIMEOUT_MS, TimeUnit.MILLISECONDS),
                "Timed out when loading OfflinePageModel!");
    }

    /**
     * Set up the input/output, bridge and observer we're going to use.
     * @param useCustomScheduler True if customized scheduler (the one with immediate scheduling)
     *                           will be used. False otherwise.
     */
    protected void setUpIOAndBridge(final boolean useCustomScheduler) throws InterruptedException {
        try {
            mLogOutput = getOutputStream(LOG_OUTPUT_FILE_PATH);
        } catch (IOException e) {
            Log.wtf(TAG, "Cannot set output file!");
            Log.wtf(TAG, e.getMessage(), e);
        }
        try {
            getUrlListFromInputFile(INPUT_FILE_PATH);
        } catch (IOException e) {
            Log.wtf(TAG, "Cannot read input file!");
            Log.wtf(TAG, e.getMessage(), e);
        }
        checkTrue(mUrls != null, "URLs weren't loaded.");
        checkTrue(mUrls.size() > 0, "No valid URLs in the input file.");

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
        checkTrue(semaphore.tryAcquire(GET_PAGES_TIMEOUT_MS, TimeUnit.MILLISECONDS),
                "Timed out when getting all offline pages");
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
        OutputStreamWriter output = getOutputStream(RESULT_OUTPUT_FILE_PATH);
        try {
            int failedCount = 0;
            if (!completed) {
                logError("Test terminated before all requests completed.");
            }
            File externalArchiveDir = getExternalArchiveDir();
            for (int i = 0; i < mRequestMetadata.size(); i++) {
                RequestMetadata metadata = mRequestMetadata.valueAt(i);
                long requestId = metadata.mId;
                int status = metadata.mStatus;
                String url = metadata.mUrl;
                OfflinePageItem page = metadata.mPage;
                if (page == null) {
                    output.write(url + DELIMITER + statusToString(status) + NEW_LINE);
                    if (status != -1) {
                        failedCount++;
                    }
                    continue;
                }
                output.write(metadata.mUrl + DELIMITER + statusToString(status) + DELIMITER
                        + page.getFileSize() / 1000 + " KB" + DELIMITER
                        + metadata.mTimeDelta.getTimeDelta() + NEW_LINE);
                // Move the page to external storage if external archive exists.
                File originalPage = new File(page.getFilePath());
                File externalPage = new File(externalArchiveDir, originalPage.getName());
                if (!OfflinePageUtils.copyToShareableLocation(originalPage, externalPage)) {
                    logError("Saved page for url " + page.getUrl() + " cannot be moved.");
                }
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
            if (mLogOutput != null) {
                mLogOutput.close();
            }
        }
    }

    /**
     * Method to parse config files for test parameters.
     */
    public void parseConfigFile() throws IOException {
        Properties properties = new Properties();
        InputStream inputStream = null;
        try {
            File configFile = new File(Environment.getExternalStorageDirectory(), CONFIG_FILE_PATH);
            inputStream = new FileInputStream(configFile);
            properties.load(inputStream);
            mIsUserRequested = Boolean.parseBoolean(properties.getProperty("IsUserRequested"));
            mTimeoutPerUrlInSeconds =
                    Long.parseLong(properties.getProperty("TimeoutPerUrlInSeconds"));
            mUseTestScheduler = Boolean.parseBoolean(properties.getProperty("UseTestScheduler"));
        } catch (FileNotFoundException e) {
            Log.e(TAG, e.getMessage(), e);
            fail(String.format(
                    "Config file %s is not found, aborting the test.", CONFIG_FILE_PATH));
        } finally {
            if (inputStream != null) {
                inputStream.close();
            }
        }
    }

    /**
     * The test is the entry point for all kinds of testing of SavePageLater.
     * It is encouraged to use run_offline_page_evaluation_test.py to run this test.
     */
    @Manual
    public void testFailureRateWithTimeout() throws IOException, InterruptedException {
        parseConfigFile();
        setUpIOAndBridge(mUseTestScheduler);
        processUrls(mUrls);
    }
}
