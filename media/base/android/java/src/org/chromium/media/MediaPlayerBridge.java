// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.content.Context;
import android.media.MediaPlayer;
import android.net.Uri;
import android.text.TextUtils;

import java.util.HashMap;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

@JNINamespace("media")
class MediaPlayerBridge {
    @CalledByNative
    private static boolean setDataSource(MediaPlayer player, Context context, String url,
            String cookies, boolean hideUrlLog) {
        Uri uri = Uri.parse(url);
        HashMap headersMap = new HashMap<String, String>();
        if (hideUrlLog)
            headersMap.put("x-hide-urls-from-log", "true");
        if (!TextUtils.isEmpty(cookies))
            headersMap.put("Cookie", cookies);
        try {
            player.setDataSource(context, uri, headersMap);
            return true;
        } catch (Exception e) {
            return false;
        }
    }
}
