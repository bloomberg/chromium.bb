// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import org.chromium.base.Log;

import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.SequenceInputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.LinkedList;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.ScheduledExecutorService;
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
    private static final String TAG = "cr_CastCrashUploader";
    private static final String CRASH_REPORT_HOST = "clients2.google.com";
    private static final String CAST_SHELL_USER_AGENT = android.os.Build.MODEL + "/CastShell";
    // Multipart dump filename has format "[random string].dmp[pid]", e.g.
    // 20597a65-b822-008e-31f8fc8e-02bb45c0.dmp18169
    private static final String DUMP_FILE_REGEX = ".*\\.dmp\\d*";

    private final ScheduledExecutorService mExecutorService;
    private final String mCrashDumpPath;
    private final String mCrashReportUploadUrl;

    public CastCrashUploader(String crashDumpPath, boolean uploadCrashToStaging) {
        this.mCrashDumpPath = crashDumpPath;
        mCrashReportUploadUrl = uploadCrashToStaging
                ? "https://clients2.google.com/cr/staging_report"
                : "https://clients2.google.com/cr/report";
        mExecutorService = Executors.newScheduledThreadPool(1);
    }

    /**
     * Sets up a periodic uploader, that checks for new dumps to upload every 20 minutes
     */
    public void startPeriodicUpload() {
        mExecutorService.scheduleWithFixedDelay(
                new Runnable() {
                    @Override
                    public void run() {
                        queueAllCrashDumpUploads(false /* synchronous */, null);
                    }
                },
                0,  // Do first run immediately
                20, // Run once every 20 minutes
                TimeUnit.MINUTES);
    }

    public void removeCrashDumps() {
        File crashDumpDirectory = new File(mCrashDumpPath);
        for (File potentialDump : crashDumpDirectory.listFiles()) {
            if (potentialDump.getName().matches(DUMP_FILE_REGEX)) {
                potentialDump.delete();
            }
        }
    }

    /**
     * Searches for files matching the given regex in the crash dump folder, queueing each
     * one for upload.
     * @param synchronous Whether or not this function should block on queued uploads
     * @param logFile Log file to include, if any
     */
    private void queueAllCrashDumpUploads(boolean synchronous, String logFile) {
        if (mCrashDumpPath == null) return;
        Log.i(TAG, "Checking for crash dumps");

        LinkedList<Future> tasks = new LinkedList<Future>();
        File crashDumpDirectory = new File(mCrashDumpPath);
        for (File potentialDump : crashDumpDirectory.listFiles()) {
            String dumpName = potentialDump.getName();
            if (dumpName.matches(DUMP_FILE_REGEX)) {
                int dumpPid;
                try {
                    dumpPid = Integer.parseInt(
                        dumpName.substring(dumpName.lastIndexOf(".dmp") + 4));
                } catch (NumberFormatException e) {
                    // Likely resulting from failing to parse an empty string, set value to 0
                    dumpPid = 0;
                }
                // Check if the dump we found has pid matching this process, and if so include
                // this process's log.
                // TODO(gunsch): Currently, logs are lost if dump cannot be uploaded right away.
                if (dumpPid == android.os.Process.myPid()) {
                    tasks.add(queueCrashDumpUpload(potentialDump, new File(logFile)));
                } else {
                    tasks.add(queueCrashDumpUpload(potentialDump, null));
                }
            }
        }

        // Wait on tasks, if necessary.
        if (synchronous) {
            for (Future task : tasks) {
                // Wait on task. If thread received interrupt and should stop waiting, return.
                if (!waitOnTask(task)) {
                    return;
                }
            }
        }
    }

    /**
     * Enqueues a background task to upload a single crash dump file.
     */
    private Future queueCrashDumpUpload(final File dumpFile, final File logFile) {
        return mExecutorService.submit(new Runnable() {
            @Override
            public void run() {
                Log.i(TAG, "Uploading dump crash log: %s", dumpFile.getName());

                try {
                    InputStream uploadCrashDumpStream = new FileInputStream(dumpFile);
                    InputStream logFileStream = null;

                    // Dump file is already in multipart MIME format and has a boundary throughout.
                    // Scrape the first line, remove two dashes, call that the "boundary" and add it
                    // to the content-type.
                    FileInputStream dumpFileStream = new FileInputStream(dumpFile);
                    String dumpFirstLine = getFirstLine(dumpFileStream);
                    String mimeBoundary = dumpFirstLine.substring(2);

                    if (logFile != null) {
                        Log.i(TAG, "Including log file: %s", logFile.getName());
                        StringBuilder logHeader = new StringBuilder();
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

                    HttpURLConnection connection =
                            (HttpURLConnection) new URL(mCrashReportUploadUrl).openConnection();

                    // Expect a report ID as the entire response
                    try {
                        connection.setDoOutput(true);
                        connection.setRequestProperty("Content-Type",
                                "multipart/form-data; boundary=" + mimeBoundary);
                        streamCopy(uploadCrashDumpStream, connection.getOutputStream());

                        String responseLine = getFirstLine(connection.getInputStream());

                        int responseCode = connection.getResponseCode();
                        if (responseCode == HttpURLConnection.HTTP_OK
                                || responseCode == HttpURLConnection.HTTP_CREATED
                                || responseCode == HttpURLConnection.HTTP_ACCEPTED) {
                            Log.i(TAG, "Successfully uploaded to %s, report ID: %s",
                                    mCrashReportUploadUrl, responseLine);
                        } else {
                            Log.e(TAG, "Failed response (%d): %s", responseCode,
                                    connection.getResponseMessage());

                            // 400 Bad Request is returned if the dump file is malformed. If request
                            // is not malformed, short-circuit before cleanup to avoid deletion and
                            // retry later, otherwise pass through and delete malformed file.
                            if (responseCode != HttpURLConnection.HTTP_BAD_REQUEST) {
                                return;
                            }
                        }
                    } catch (FileNotFoundException fnfe) {
                        // Android's HttpURLConnection implementation fires FNFE on some errors.
                        Log.e(TAG, "Failed response: " + connection.getResponseCode(), fnfe);
                    } finally {
                        connection.disconnect();
                        dumpFileStream.close();
                        if (logFileStream != null) {
                            logFileStream.close();
                        }
                    }

                    // Delete the file so we don't re-upload it next time.
                    dumpFile.delete();

                    if (logFile != null && logFile.exists()) {
                        logFile.delete();
                    }
                } catch (IOException e) {
                    Log.e(TAG, "Error occurred trying to upload crash dump", e);
                }
            }
        });
    }

    /**
     * Copies all available data from |inStream| to |outStream|. Closes both
     * streams when done.
     *
     * @param inStream the stream to read
     * @param outStream the stream to write to
     * @throws IOException
     */
    private static void streamCopy(InputStream inStream,
            OutputStream outStream) throws IOException {
        byte[] temp = new byte[4096];
        int bytesRead = inStream.read(temp);
        while (bytesRead >= 0) {
            outStream.write(temp, 0, bytesRead);
            bytesRead = inStream.read(temp);
        }
        inStream.close();
        outStream.close();
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

    /**
     * Waits until Future is propagated
     * @return Whether thread should continue waiting
     */
    private boolean waitOnTask(Future task) {
        try {
            task.get();
            return true;
        } catch (InterruptedException e) {
            // Was interrupted while waiting, tell caller to cancel waiting
            return false;
        } catch (ExecutionException e) {
            // Task execution may have failed, but this is fine as long as it finished
            // executing
            return true;
        }
    }
}
