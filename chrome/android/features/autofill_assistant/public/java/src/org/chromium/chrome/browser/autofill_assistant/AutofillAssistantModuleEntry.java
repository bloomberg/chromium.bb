// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.os.Bundle;

import java.util.Map;

/**
 * Interface for base module to start the autofill assistant
 * experience in dynamic feature module.
 */
interface AutofillAssistantModuleEntry {
    /**
     * Show the onboarding screen and run {@code onAccept} if user agreed to proceed.
     */
    void showOnboarding(String experimentIds, Runnable onAccept);

    /**
     * Launches Autofill Assistant on the current web contents, expecting autostart.
     */
    void start(String initialUrl, Map<String, String> parameters, String experimentIds,
            Bundle intentExtras);
}