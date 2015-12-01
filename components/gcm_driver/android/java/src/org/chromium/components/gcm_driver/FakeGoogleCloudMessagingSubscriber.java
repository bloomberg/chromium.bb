// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.gcm_driver;

import android.os.Bundle;

import org.chromium.base.VisibleForTesting;

import java.io.IOException;

import javax.annotation.Nullable;

/**
 * A fake subscriber for test purposes.
 */
public class FakeGoogleCloudMessagingSubscriber implements GoogleCloudMessagingSubscriber {
    private static final String FAKE_REGISTRATION_ID = "FAKE_REGISTRATION_ID";
    private String mLastSubscribeSource;
    private String mLastSubscribeSubtype;

    @Override
    public String subscribe(String source, String subtype, @Nullable Bundle data)
            throws IOException {
        mLastSubscribeSource = source;
        mLastSubscribeSubtype = subtype;
        return FAKE_REGISTRATION_ID;
    }

    @Override
    public void unsubscribe(String source, String subtype, @Nullable Bundle data)
            throws IOException {
        // No need to do anything here yet.
    }

    /**
     * The source (sender id) passed to #subscribe the last time it was called, or null if it was
     * never called.
     */
    @Nullable
    @VisibleForTesting
    public String getLastSubscribeSource() {
        return mLastSubscribeSource;
    }

    /**
     * The subtype (app id) passed to #subscribe the last time it was called, or null if it was
     * never called.
     */
    @Nullable
    @VisibleForTesting
    public String getLastSubscribeSubtype() {
        return mLastSubscribeSubtype;
    }
}
