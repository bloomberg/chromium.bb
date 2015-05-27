// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.res.AssetFileDescriptor;
import android.database.Cursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.util.Log;

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * Content provider for the OSS licenses file.
 */
public class LicenseContentProvider extends ContentProvider {
    public static final String LICENSES_URI_SUFFIX = "LicenseContentProvider/webview_licenses";
    public static final String LICENSES_CONTENT_TYPE = "text/html";

    @Override
    public boolean onCreate() {
        return true;
    }

    @Override
    public AssetFileDescriptor openAssetFile(Uri uri, String mode) {
        if (uri != null && uri.toString().endsWith(LICENSES_URI_SUFFIX)) {
            try {
                return extractAsset("webview_licenses.notice");
            } catch (IOException e) {
                Log.e("WebView", "Failed to open the license file", e);
            }
        }
        return null;
    }

    // This is to work around the known limitation of AssetManager.openFd to refuse
    // opening files that are compressed in the apk file.
    private AssetFileDescriptor extractAsset(String name) throws IOException {
        File extractedFile = new File(getContext().getCacheDir(), name);
        if (!extractedFile.exists()) {
            InputStream inputStream = null;
            OutputStream outputStream = null;
            try {
                inputStream = getContext().getAssets().open(name);
                outputStream = new BufferedOutputStream(
                        new FileOutputStream(extractedFile.getAbsolutePath()));
                copyStreams(inputStream, outputStream);
            } finally {
                if (inputStream != null) inputStream.close();
                if (outputStream != null) outputStream.close();
            }
        }
        ParcelFileDescriptor parcelFd =
                ParcelFileDescriptor.open(extractedFile, ParcelFileDescriptor.MODE_READ_ONLY);
        if (parcelFd != null) {
            return new AssetFileDescriptor(parcelFd, 0, parcelFd.getStatSize());
        }
        return null;
    }

    private static void copyStreams(InputStream in, OutputStream out) throws IOException {
        byte[] buffer = new byte[8192];
        int c;
        while ((c = in.read(buffer)) != -1) {
            out.write(buffer, 0, c);
        }
    }

    @Override
    public String getType(Uri uri) {
        if (uri != null && uri.toString().endsWith(LICENSES_URI_SUFFIX)) {
            return LICENSES_CONTENT_TYPE;
        }
        return null;
    }

    @Override
    public int update(Uri uri, ContentValues values, String where,
                      String[] whereArgs) {
        throw new UnsupportedOperationException();
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        throw new UnsupportedOperationException();
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        throw new UnsupportedOperationException();
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection,
                        String[] selectionArgs, String sortOrder) {
        throw new UnsupportedOperationException();
    }
}
