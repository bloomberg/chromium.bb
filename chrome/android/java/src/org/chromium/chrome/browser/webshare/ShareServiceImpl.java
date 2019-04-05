// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webshare;

import android.app.Activity;
import android.content.ComponentName;
import android.net.Uri;
import android.support.annotation.Nullable;

import org.chromium.base.ContentUriUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.share.ShareParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.mojo.system.MojoException;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.mojom.Url;
import org.chromium.webshare.mojom.ShareError;
import org.chromium.webshare.mojom.ShareService;
import org.chromium.webshare.mojom.SharedFile;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;

/**
 * Android implementation of the ShareService service defined in
 * third_party/blink/public/mojom/webshare/webshare.mojom.
 */
public class ShareServiceImpl implements ShareService {
    private final Activity mActivity;

    private static final String TAG = "share";

    // These numbers are written to histograms. Keep in sync with WebShareMethod enum in
    // histograms.xml, and don't reuse or renumber entries (except for the _COUNT entry).
    private static final int WEBSHARE_METHOD_SHARE = 0;
    // Count is technically 1, but recordEnumeratedHistogram requires a boundary of at least 2
    // (https://crbug.com/645032).
    private static final int WEBSHARE_METHOD_COUNT = 2;

    // These numbers are written to histograms. Keep in sync with WebShareOutcome enum in
    // histograms.xml, and don't reuse or renumber entries (except for the _COUNT entry).
    private static final int WEBSHARE_OUTCOME_SUCCESS = 0;
    private static final int WEBSHARE_OUTCOME_UNKNOWN_FAILURE = 1;
    private static final int WEBSHARE_OUTCOME_CANCELED = 2;
    private static final int WEBSHARE_OUTCOME_COUNT = 3;

    // These protect us if the renderer is compromised.
    private static final int MAX_SHARED_FILE_COUNT = 10;
    private static final int MAX_SHARED_FILE_BYTES = 50 * 1024 * 1024;

    private static final TaskRunner TASK_RUNNER =
            PostTask.createSequencedTaskRunner(TaskTraits.USER_BLOCKING);

    static {
        TASK_RUNNER.disableLifetimeCheck();
    }

    public ShareServiceImpl(@Nullable WebContents webContents) {
        mActivity = activityFromWebContents(webContents);
    }

    @Override
    public void close() {}

    @Override
    public void onConnectionError(MojoException e) {}

    @Override
    public void share(String title, String text, Url url, final SharedFile[] files,
            final ShareResponse callback) {
        RecordHistogram.recordEnumeratedHistogram("WebShare.ApiCount", WEBSHARE_METHOD_SHARE,
                WEBSHARE_METHOD_COUNT);

        if (mActivity == null) {
            RecordHistogram.recordEnumeratedHistogram("WebShare.ShareOutcome",
                    WEBSHARE_OUTCOME_UNKNOWN_FAILURE, WEBSHARE_OUTCOME_COUNT);
            callback.call(ShareError.INTERNAL_ERROR);
            return;
        }

        ShareHelper.TargetChosenCallback innerCallback = new ShareHelper.TargetChosenCallback() {
            @Override
            public void onTargetChosen(ComponentName chosenComponent) {
                RecordHistogram.recordEnumeratedHistogram("WebShare.ShareOutcome",
                        WEBSHARE_OUTCOME_SUCCESS, WEBSHARE_OUTCOME_COUNT);
                callback.call(ShareError.OK);
            }

            @Override
            public void onCancel() {
                RecordHistogram.recordEnumeratedHistogram("WebShare.ShareOutcome",
                        WEBSHARE_OUTCOME_CANCELED, WEBSHARE_OUTCOME_COUNT);
                callback.call(ShareError.CANCELED);
            }
        };

        final ShareParams.Builder paramsBuilder = new ShareParams.Builder(mActivity, title, url.url)
                                                          .setText(text)
                                                          .setCallback(innerCallback);

        if (files == null || files.length == 0) {
            ShareHelper.share(paramsBuilder.build());
            return;
        }

        if (files.length > MAX_SHARED_FILE_COUNT) {
            callback.call(ShareError.PERMISSION_DENIED);
            return;
        }

        for (SharedFile file : files) {
            if (isDangerousFilename(file.name)) {
                callback.call(ShareError.PERMISSION_DENIED);
                return;
            }
        }

        new AsyncTask<Boolean>() {
            @Override
            protected void onPostExecute(Boolean result) {
                if (result.equals(Boolean.FALSE)) {
                    callback.call(ShareError.INTERNAL_ERROR);
                }
            }

            @Override
            protected Boolean doInBackground() {
                ArrayList<Uri> fileUris = new ArrayList<>(files.length);
                ArrayList<BlobReceiver> blobReceivers = new ArrayList<>(files.length);
                try {
                    File sharePath = ShareHelper.getSharedFilesDirectory();

                    if (!sharePath.exists() && !sharePath.mkdir()) {
                        throw new IOException("Failed to create directory for shared file.");
                    }

                    for (int index = 0; index < files.length; ++index) {
                        File tempFile = File.createTempFile(
                                "share", FileUtils.getExtension(files[index].name), sharePath);
                        fileUris.add(ContentUriUtils.getContentUriFromFile(tempFile));
                        blobReceivers.add(new BlobReceiver(
                                new FileOutputStream(tempFile), MAX_SHARED_FILE_BYTES));
                    }

                } catch (IOException ie) {
                    Log.w(TAG, "Error creating shared file", ie);
                    return false;
                }

                paramsBuilder.setFileContentType(SharedFileCollator.commonMimeType(files));
                paramsBuilder.setFileUris(fileUris);
                SharedFileCollator collator =
                        new SharedFileCollator(paramsBuilder.build(), callback);

                for (int index = 0; index < files.length; ++index) {
                    blobReceivers.get(index).start(files[index].blob.blob, collator);
                }
                return true;
            }
        }.executeOnTaskRunner(TASK_RUNNER);
    }

    private static boolean isDangerousFilename(String name) {
        String extension = FileUtils.getExtension(name);
        return name.indexOf('/') != -1 || name.indexOf('\\') != -1 || name.indexOf('.') <= 0
                || extension.equals(".apk") || extension.equals(".dex");
    }

    @Nullable
    private static Activity activityFromWebContents(@Nullable WebContents webContents) {
        if (webContents == null) return null;

        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) return null;

        return window.getActivity().get();
    }
}
