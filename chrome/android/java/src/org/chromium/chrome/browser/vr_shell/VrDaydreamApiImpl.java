// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr_shell;

import android.app.PendingIntent;
import android.content.Context;

import com.google.vr.ndk.base.DaydreamApi;

import org.chromium.base.annotations.UsedByReflection;

/**
 * A wrapper for DaydreamApi.
 */
@UsedByReflection("VrShellDelegate.java")
public class VrDaydreamApiImpl implements VrDaydreamApi {
    private DaydreamApi mDaydreamApi;

    @UsedByReflection("VrShellDelegate.java")
    public VrDaydreamApiImpl(Context context) {
        mDaydreamApi = DaydreamApi.create(context);
    }

    @Override
    public void registerDaydreamIntent(PendingIntent pendingIntent) {
        if (mDaydreamApi != null) {
            mDaydreamApi.registerDaydreamIntent(pendingIntent);
        }
    }
    @Override
    public void unregisterDaydreamIntent() {
        if (mDaydreamApi != null) {
            mDaydreamApi.unregisterDaydreamIntent();
        }
    }

    @Override
    public void close() {
        if (mDaydreamApi != null) {
            mDaydreamApi.close();
        }
        mDaydreamApi = null;
    }
}
