// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEND_FEEDBACK_EXPERIMENT_H_
#define CHROME_BROWSER_UI_SEND_FEEDBACK_EXPERIMENT_H_

// This file exists to facilitate a field trial which modifies the description
// and/or location of the Report Feedback menu option, and corresponding option
// on the chrome://help page. (crbug.com/169339)
namespace chrome {

// Returns true if the alternative (Send Feedback) text should be
// used in place of the original (Report an Issue...).
bool UseAlternateSendFeedbackText();

// Returns true if the alternative menu location for the Send Feedback/Report
// an Issue... option should be used.
bool UseAlternateSendFeedbackLocation();

// Return the ID for the desired string to be used for a menu item
// (See UseAlternateText).
int GetSendFeedbackMenuLabelID();

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_SEND_FEEDBACK_EXPERIMENT_H_
