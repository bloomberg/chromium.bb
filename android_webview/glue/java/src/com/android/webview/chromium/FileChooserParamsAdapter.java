// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.webkit.WebChromeClient.FileChooserParams;

import org.chromium.android_webview.AwContentsClient;

public class FileChooserParamsAdapter extends FileChooserParams {
    private AwContentsClient.FileChooserParams mParams;

    public static Uri[] parseFileChooserResult(int resultCode, Intent intent) {
        if (resultCode == Activity.RESULT_CANCELED) {
            return null;
        }
        Uri result = intent == null || resultCode != Activity.RESULT_OK ? null
                : intent.getData();

        Uri[] uris = null;
        if (result != null) {
            uris = new Uri[1];
            uris[0] = result;
        }
        return uris;
    }

    FileChooserParamsAdapter(AwContentsClient.FileChooserParams params, Context context) {
        mParams = params;
    }

    @Override
    public int getMode() {
        return mParams.mode;
    }

    @Override
    public String[] getAcceptTypes() {
        if (mParams.acceptTypes == null)
            return new String[0];
        return mParams.acceptTypes.split(";");
    }

    @Override
    public boolean isCaptureEnabled() {
        return mParams.capture;
    }

    @Override
    public CharSequence getTitle() {
        return mParams.title;
    }

    @Override
    public String getFilenameHint() {
        return mParams.defaultFilename;
    }

    @Override
    public Intent createIntent() {
        // TODO: Move this code to Aw. Once code is moved
        // and merged to M37 get rid of this.
        String mimeType = "*/*";
        if (mParams.acceptTypes != null && !mParams.acceptTypes.trim().isEmpty())
            mimeType = mParams.acceptTypes.split(";")[0];

        Intent i = new Intent(Intent.ACTION_GET_CONTENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType(mimeType);
        return i;
    }
}
