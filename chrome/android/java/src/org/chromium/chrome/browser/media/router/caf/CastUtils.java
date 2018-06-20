// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router.caf;

import com.google.android.gms.cast.framework.CastContext;

import org.chromium.base.ContextUtils;

/** Utility methods for Cast. */
public class CastUtils {
    public static CastContext getCastContext() {
        return CastContext.getSharedInstance(ContextUtils.getApplicationContext());
    }
}
