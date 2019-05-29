// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.OpenableColumns;
import android.text.TextUtils;

import org.json.JSONArray;
import org.json.JSONException;

import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.net.MimeTypeFilter;
import org.chromium.webapk.lib.common.WebApkMetaDataKeys;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Locale;

/**
 * Computes data for Post Share Target.
 */
public class WebApkShareTargetUtil {
    private static final String TAG = "WebApkShareTargetUtil";

    // A class containing data required to generate a share target post request.
    protected static class PostData {
        public boolean isMultipartEncoding;
        public ArrayList<String> names;
        public ArrayList<Boolean> isValueFileUri;
        public ArrayList<String> values;
        public ArrayList<String> filenames;
        public ArrayList<String> types;

        public PostData(boolean isMultipartEncoding) {
            this.isMultipartEncoding = isMultipartEncoding;
            names = new ArrayList<>();
            isValueFileUri = new ArrayList<>();
            values = new ArrayList<>();
            filenames = new ArrayList<>();
            types = new ArrayList<>();
        }

        private void addPlainText(String name, String value) {
            names.add(name);
            isValueFileUri.add(false);
            values.add(value);
            filenames.add("");
            types.add("text/plain");
        }

        private void addFileUri(String name, String fileUri, String fileName, String type) {
            names.add(name);
            isValueFileUri.add(true);
            values.add(fileUri);
            filenames.add(fileName);
            types.add(type);
        }
    }

    private static String getFileTypeFromContentUri(Uri uri) {
        return ContextUtils.getApplicationContext().getContentResolver().getType(uri);
    }

    private static String getFileNameFromContentUri(Uri uri) {
        if (uri.getScheme().equals("content")) {
            try (Cursor cursor = ContextUtils.getApplicationContext().getContentResolver().query(
                         uri, null, null, null, null)) {
                if (cursor != null && cursor.moveToFirst()) {
                    String result =
                            cursor.getString(cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME));
                    if (result != null) {
                        return result;
                    }
                }
            }
        }

        return uri.getPath();
    }

    public static String[] decodeJsonStringArray(String encodedJsonArray) {
        if (encodedJsonArray == null) {
            return null;
        }

        try {
            JSONArray jsonArray = new JSONArray(encodedJsonArray);
            String[] originalData = new String[jsonArray.length()];
            for (int i = 0; i < jsonArray.length(); i++) {
                originalData[i] = jsonArray.getString(i);
            }
            return originalData;
        } catch (JSONException e) {
        }
        return null;
    }

    public static String[][] decodeJsonAccepts(String encodedAcceptsArray) {
        if (encodedAcceptsArray == null) {
            return null;
        }
        try {
            JSONArray jsonArray = new JSONArray(encodedAcceptsArray);
            String[][] originalData = new String[jsonArray.length()][];
            for (int i = 0; i < jsonArray.length(); i++) {
                String[] childArr = new String[jsonArray.getJSONArray(i).length()];
                for (int j = 0; j < childArr.length; j++) {
                    childArr[j] = jsonArray.getJSONArray(i).getString(j);
                }
                originalData[i] = childArr;
            }
            return originalData;
        } catch (JSONException e) {
        }

        return null;
    }

    protected static boolean methodFromShareTargetMetaDataIsPost(Bundle metaData) {
        String method = IntentUtils.safeGetString(metaData, WebApkMetaDataKeys.SHARE_METHOD);
        return method != null && "POST".equals(method.toUpperCase(Locale.ENGLISH));
    }

    protected static void addFilesToMultipartPostData(PostData postData,
            String[] shareTargetParamsFileNames, String[][] shareTargetParamsFileAccepts,
            ArrayList<Uri> shareFiles) {
        if (shareTargetParamsFileNames == null || shareTargetParamsFileAccepts == null
                || shareFiles == null) {
            return;
        }

        if (shareTargetParamsFileNames.length != shareTargetParamsFileAccepts.length) {
            return;
        }

        try (StrictModeContext strictModeContextUnused = StrictModeContext.allowDiskReads()) {
            for (Uri fileUri : shareFiles) {
                String fileType = getFileTypeFromContentUri(fileUri);
                String fileName = getFileNameFromContentUri(fileUri);

                if (fileType == null || fileName == null) {
                    continue;
                }

                for (int i = 0; i < shareTargetParamsFileNames.length; i++) {
                    String[] mimeTypeList = shareTargetParamsFileAccepts[i];
                    MimeTypeFilter mimeTypeFilter =
                            new MimeTypeFilter(Arrays.asList(mimeTypeList), false);
                    if (mimeTypeFilter.accept(fileUri, fileType)) {
                        postData.addFileUri(shareTargetParamsFileNames[i], fileUri.toString(),
                                fileName, fileType);
                        break;
                    }
                }
            }
        }
    }

    protected static PostData computePostData(String shareTargetActivityName,
            WebApkInfo.ShareTarget shareTarget, WebApkInfo.ShareData shareData) {
        if (shareTarget == null || !shareTarget.isShareMethodPost() || shareData == null
                || !shareData.shareActivityClassName.equals(shareTargetActivityName)) {
            return null;
        }

        PostData postData = new PostData(shareTarget.isShareEncTypeMultipart());

        if (!TextUtils.isEmpty(shareTarget.getParamTitle()) && shareData.subject != null) {
            postData.addPlainText(shareTarget.getParamTitle(), shareData.subject);
        }

        if (!TextUtils.isEmpty(shareTarget.getParamText()) && shareData.text != null) {
            postData.addPlainText(shareTarget.getParamText(), shareData.text);
        }

        if (!postData.isMultipartEncoding) {
            return postData;
        }

        addFilesToMultipartPostData(postData, shareTarget.getFileNames(),
                shareTarget.getFileAccepts(), shareData.files);

        return postData;
    }
}
