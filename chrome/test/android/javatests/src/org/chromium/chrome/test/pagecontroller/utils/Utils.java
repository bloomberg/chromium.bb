// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.utils;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import java.util.Collections;
import java.util.List;

/**
 * Internal utility methods for pagecontroller.utils package.
 */

final class Utils {
    // Return empty list if t is null, else return a singleton list containing t.
    static <T> List<T> nullableIntoList(@Nullable T t) {
        return t == null ? Collections.<T>emptyList() : Collections.singletonList(t);
    }

    // Returns the index-th item in the list or null if it's out of bounds.
    static @Nullable<T> T nullableGet(@NonNull List<T> list, int index) {
        return index >= list.size() ? null : list.get(index);
    }
}
