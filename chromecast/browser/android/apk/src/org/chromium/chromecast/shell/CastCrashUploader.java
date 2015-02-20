// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.net.http.AndroidHttpClient;
import android.util.Log;

import org.apache.http.HttpHost;
import org.apache.http.HttpResponse;
import org.apache.http.HttpStatus;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.entity.InputStreamEntity;

import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.SequenceInputStream;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

/**
 * Crash crashdump uploader. Scans the crash dump location provided by CastCrashReporterClient
 * for dump files, attempting to upload all crash dumps to the crash server.
 *
 * Uploading is intended to happen in a background thread, and this method will likely be called
 * on startup, looking for crash dumps from previous runs, since Chromium's crash code
 * explicitly blocks any post-dump hooks or uploading for Android builds.
 */
public final class CastCrashUploader {
    private static final String TAG = "CastCrashUploader";
    private static final String CRASH_REPORT_HOST = "clients2.google.com";
    private static final String CAST_SHELL_USER_AGENT = android.os.Build.MODEL + "/CastShell";

    private final ExecutorService mExecutorService;
    private final String mCrashDumpPath;
    private final String mCrashReportUploadUrl;

    public CastCrashUploader(String crashDumpPath, boolean uploadCrashToStaging) {
        this.mCrashDumpPath = crashDumpPath;
        mCrashReportUploadUrl = uploadCrashToStaging
                ? "http://clients2.google.com/cr/staging_report"
                : "http://clients2.google.com/cr/report";
        mExecutorService = Executors.newFixedThreadPool(1);
    }

    public void removeCrashDumpsSync() {
        mExecutorService.submit(new Runnable() {
            @Override
            public void run() {
                File crashDumpDirectory = new File(mCrashDumpPath);
                for (File potentialDump : crashDumpDirectory.listFiles()) {
                    if (potentialDump.getName().matches(".*\\.dmp\\d*")) {
                        potentialDump.delete();
                    }
                }
            }
        });
        waitForTasksToFinish();
    }

    /**
     * Synchronously uploads the crash dump from the current process, along with an attached
     * log file.
     * @param logFilePath Full path to the log file for the current process.
     */
    public void uploadCurrentProcessDumpSync(String logFilePath) {
        int pid = android.os.Process.myPid();
        Log.d(TAG, "Immediately attempting a crash upload with logs, looking for: .dmp" + pid);

        queueAllCrashDumpUpload(".*\\.dmp" + pid, new File(logFilePath));
        waitForTasksToFinish();
    }

    private void waitForTasksToFinish() {
        try {
            mExecutorService.shutdown();
            boolean finished = mExecutorService.awaitTermination(60, TimeUnit.SECONDS);
            if (!finished) {
                Log.d(TAG, "Crash dump handling did not finish executing in time. Exiting.");
            }
        } catch (InterruptedException e) {
            Log.e(TAG, "Interrupted waiting for asynchronous execution", e);
        }
    }

    /**
     * Scans the given location for crash dump files and queues background
     * uploads of each file.
     */
    public void uploadRecentCrashesAsync() {
        mExecutorService.submit(new Runnable() {
            @Override
            public void run() {
                // Multipart dump filename has format "[random string].dmp[pid]", e.g.
                // 20597a65-b822-008e-31f8fc8e-02bb45c0.dmp18169
                queueAllCrashDumpUpload(".*\\.dmp\\d*", null);
            }
        });
    }

    /**
     * Searches for files matching the given regex in the crash dump folder, queueing each
     * one for upload.
     * @param dumpFileRegex Regex to test against the name of each file in the crash dump folder.
     * @param logFile Log file to include, if any.
     */
    private void queueAllCrashDumpUpload(String dumpFileRegex, File logFile) {
        if (mCrashDumpPath == null) return;
        final AndroidHttpClient httpClient = AndroidHttpClient.newInstance(CAST_SHELL_USER_AGENT);
        File crashDumpDirectory = new File(mCrashDumpPath);
        for (File potentialDump : crashDumpDirectory.listFiles()) {
            if (potentialDump.getName().matches(dumpFileRegex)) {
                queueCrashDumpUpload(httpClient, potentialDump, logFile);
            }
        }

        // When finished uploading all of the queued dumps, release httpClient
        mExecutorService.submit(new Runnable() {
            @Override
            public void run() {
                httpClient.close();
            }
        });
    }

    /**
     * Enqueues a background task to upload a single crash dump file.
     */
    private void queueCrashDumpUpload(final AndroidHttpClient httpClient, final File dumpFile,
            final File logFile) {
        mExecutorService.submit(new Runnable() {
            @Override
            public void run() {
                Log.d(TAG, "Uploading dump crash log: " + dumpFile.getName());

                try {
                    // Dump file is already in multipart MIME format and has a boundary throughout.
                    // Scrape the first line, remove two dashes, call that the "boundary" and add it
                    // to the content-type.
                    FileInputStream dumpFileStream = new FileInputStream(dumpFile);
                    String dumpFirstLine = getFirstLine(dumpFileStream);
                    String mimeBoundary = dumpFirstLine.substring(2);

                    InputStream uploadCrashDumpStream = new FileInputStream(dumpFile);
                    InputStream logFileStream = null;

                    if (logFile != null) {
                        Log.d(TAG, "Including log file: " + logFile.getName());
                        StringBuffer logHeader = new StringBuffer();
                        logHeader.append(dumpFirstLine);
                        logHeader.append("\n");
                        logHeader.append(
                                "Content-Disposition: form-data; name=\"log\"; filename=\"log\"\n");
                        logHeader.append("Content-Type: text/plain\n\n");
                        InputStream logHeaderStream =
                                new ByteArrayInputStream(logHeader.toString().getBytes());
                        logFileStream = new FileInputStream(logFile);

                        // Upload: prepend the log file for uploading
                        uploadCrashDumpStream = new SequenceInputStream(
                                new SequenceInputStream(logHeaderStream, logFileStream),
                                uploadCrashDumpStream);
                    }

                    InputStreamEntity entity = new InputStreamEntity(uploadCrashDumpStream, -1);
                    entity.setContentType("multipart/form-data; boundary=" + mimeBoundary);

                    HttpPost uploadRequest = new HttpPost(mCrashReportUploadUrl);
                    uploadRequest.setEntity(entity);
                    HttpResponse response =
                            httpClient.execute(new HttpHost(CRASH_REPORT_HOST), uploadRequest);

                    // Expect a report ID as the entire response
                    String responseLine = getFirstLine(response.getEntity().getContent());

                    int statusCode = response.getStatusLine().getStatusCode();
                    if (statusCode != HttpStatus.SC_OK) {
                        Log.e(TAG, "Failed response (" + statusCode + "): " + responseLine);

                        // 400 Bad Request is returned if the dump file is malformed. Delete file
                        // to prevent re-attempting later.
                        if (statusCode == HttpStatus.SC_BAD_REQUEST) {
                            dumpFile.delete();
                        }
                        return;
                    }

                    Log.d(TAG, "Successfully uploaded as report ID: " + responseLine);

                    // Delete the file so we don't re-upload it next time.
                    dumpFileStream.close();
                    dumpFile.delete();
                } catch (IOException e) {
                    Log.e(TAG, "File I/O error trying to upload crash dump", e);
                }
            }
        });
    }

    /**
     * Gets the first line from an input stream, opening and closing readers to do so.
     * We're targeting Java 6, so this is still tedious to do.
     * @return First line of the input stream.
     * @throws IOException
     */
    private String getFirstLine(InputStream inputStream) throws IOException {
        InputStreamReader streamReader = null;
        BufferedReader reader = null;
        try {
            streamReader = new InputStreamReader(inputStream);
            reader = new BufferedReader(streamReader);
            return reader.readLine();
        } finally {
            if (reader != null) {
                reader.close();
            }
            if (streamReader != null) {
                streamReader.close();
            }
        }
    }
}
