// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.test.util;

import android.graphics.Bitmap;
import android.graphics.Bitmap.CompressFormat;
import android.graphics.BitmapFactory;
import android.util.Log;

import org.chromium.base.test.util.UrlUtils;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.util.Arrays;

/**
 * Set of utility functions shared between tests managing bookmarks.
 */
public class BookmarkUtils {
    private static final String TAG = "BookmarkUtils";

    /**
     * Checks if two byte arrays are equal. Used to compare icons.
     * @return True if equal, false otherwise.
     */
    public static boolean byteArrayEqual(byte[] byte1, byte[] byte2) {
        if (byte1 == null && byte2 != null) {
            return byte2.length == 0;
        }
        if (byte2 == null && byte1 != null) {
            return byte1.length == 0;
        }
        return Arrays.equals(byte1, byte2);
    }

    /**
     * Retrieves a byte array with the decoded image data of an icon.
     * @return Data of the icon.
     */
    public static byte[] getIcon(String testPath) {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        try {
            InputStream faviconStream = (InputStream) (new URL(
                    UrlUtils.getTestFileUrl(testPath))).getContent();
            Bitmap faviconBitmap = BitmapFactory.decodeStream(faviconStream);
            faviconBitmap.compress(CompressFormat.PNG, 0, bos);
        } catch (IOException e) {
            Log.e(TAG, "Error trying to get the icon '" + testPath + "': " + e.getMessage());
        }
        return bos.toByteArray();
    }
}
