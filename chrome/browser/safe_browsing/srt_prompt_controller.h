// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SRT_PROMPT_CONTROLLER_H_
#define CHROME_BROWSER_SAFE_BROWSING_SRT_PROMPT_CONTROLLER_H_

#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"

namespace safe_browsing {

// Provides the various elements that will be displayed in the Chrome Cleaner
// UI. Also provides functions, such as |Accept()| and |Cancel()|, that should
// be called by the UI in response to user actions.
//
// Objects of this class are typically created by the SwReporterProcess class,
// which will pass in information the controller needs to be able to create
// strings to be displayed in the prompt dialog.
//
// This class manages its own lifetime and will delete itself once the Cleaner
// dialog has been dismissed and either of |Accept()| or |Cancel()| have been
// called.
class SRTPromptController {
 public:
  struct LabelInfo {
    enum LabelType {
      // Indicates that |text| should be displayed as a multi-line label.
      PARAGRAPH,
      // Indicates that |text| should be displayed as a multi-line label that is
      // indented and starts with a bullet point.
      BULLET_ITEM,
    };

    LabelInfo(LabelType type, const base::string16& text);
    ~LabelInfo();

    LabelType type;
    base::string16 text;
  };

  SRTPromptController();

  base::string16 GetWindowTitle() const;
  // The text to be shown in the Cleaner dialog's main section and will
  // always be visible while the dialog is displayed.
  std::vector<LabelInfo> GetMainText() const;
  // The text to be shown in the expandable details section of the
  // Cleaner dialog.
  std::vector<LabelInfo> GetDetailsText() const;
  // The text on the button that expands the details section.
  base::string16 GetShowDetailsLabel() const;
  // The text on the button that folds the details section.
  base::string16 GetHideDetailsLabel() const;
  base::string16 GetAcceptButtonLabel() const;

  // Called by the Cleaner dialog when the dialog has been shown. Used for
  // reporting metrics.
  void DialogShown();
  // Called by the Cleaner dialog when user accepts the prompt. Once |Accept()|
  // has been called, the controller will eventually delete itself and so no
  // member functions should be called after that.
  void Accept();
  // Called by the Cleaner dialog when the dialog is dismissed or canceled. Once
  // |Cancel()| has been called, the controller will eventually delete itself
  // and so no member functions should be called after that.
  void Cancel();

 protected:
  ~SRTPromptController();

 private:
  void OnInteractionDone();

  DISALLOW_COPY_AND_ASSIGN(SRTPromptController);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SRT_PROMPT_CONTROLLER_H_
