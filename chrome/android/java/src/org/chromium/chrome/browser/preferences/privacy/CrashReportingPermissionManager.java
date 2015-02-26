// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.privacy;

/**
 * Interface for crash reporting permissions.
 */
public interface CrashReportingPermissionManager {

    /**
     * Check whether to allow uploading crash dump now based on user consent and connectivity.
     *
     * @return boolean to whether to allow uploading crash dump now.
     */
    public boolean isUploadPermitted();
}
