// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

/**
 * Delegate for survey info bar actions.
 */
public interface SurveyInfoBarDelegate {
    /**
     * Notified when the survey infobar is closed.
     */
    void onSurveyInfoBarClosed();

    /**
     * Notified when the survey is triggered via the infobar.
     */
    void onSurveyTriggered();

    /**
     * Called to supply the survey info bar with the prompt string.
     * @return The string that will be displayed on the info bar.
     */
    String getSurveyPromptString();
}
