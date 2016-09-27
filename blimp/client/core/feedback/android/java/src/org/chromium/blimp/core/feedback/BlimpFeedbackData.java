// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.blimp.core.feedback;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.util.HashMap;
import java.util.Map;

/**
 * BlimpFeedbackData is used to gather data that would be helpful for an end user feedback
 * report. It is created and populated from native code.
 */
@JNINamespace("blimp::client")
public class BlimpFeedbackData {
    /**
     * Create a BlimpFeedbackData object.
     */
    @CalledByNative
    private static BlimpFeedbackData create(String[] keys, String[] values) {
        return new BlimpFeedbackData(keys, values);
    }

    private final Map<String, String> mData;

    private BlimpFeedbackData(String[] keys, String[] values) {
        if (keys == null || values == null || keys.length != values.length) {
            throw new IllegalStateException("Invalid feedback data.");
        }
        mData = new HashMap<>();
        for (int i = 0; i < keys.length; ++i) {
            mData.put(keys[i], values[i]);
        }
    }

    /**
     * Builds and returns the feedback data map.
     * @return the feedback data.
     */
    public Map<String, String> asMap() {
        return mData;
    }
}
