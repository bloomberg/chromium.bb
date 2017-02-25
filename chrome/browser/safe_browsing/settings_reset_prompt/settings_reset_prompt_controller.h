// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_SETTINGS_RESET_PROMPT_CONTROLLER_H_
#define CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_SETTINGS_RESET_PROMPT_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"

class Browser;

namespace safe_browsing {

class SettingsResetPromptModel;

// The |SettingsResetPromptController| class is responsible for providing the
// text that will be displayed in the settings reset dialog. The controller's
// |Accept()| and |Cancel()| functions will be called based on user
// interaction. Objects of this class will delete themselves after |Accept()| or
// |Cancel()| has been called.
class SettingsResetPromptController {
 public:
  // A simple struct representing text to be displayed in the dialog.
  struct LabelInfo {
    enum LabelType {
      // Strings of type PARAGRAPH will be displayed by multiline labels.
      PARAGRAPH,
      // Strings of type BULLET_ITEM will be displayed as a (possibly elided)
      // single-line label starting with a bullet point.
      BULLET_ITEM,
    };

    LabelInfo(LabelType type, const base::string16& text);
    // Convenience constructor for displaying resource strings.
    LabelInfo(LabelType type, int message_id);
    ~LabelInfo();

    LabelType type;
    base::string16 text;
  };

  // A controller should be created only if |model->ShouldPromptforReset()|
  // is true.
  explicit SettingsResetPromptController(
      std::unique_ptr<SettingsResetPromptModel> model);
  static void ShowSettingsResetPrompt(
      Browser* browser,
      SettingsResetPromptController* controller);

  base::string16 GetWindowTitle() const;
  base::string16 GetButtonLabel() const;
  base::string16 GetShowDetailsLabel() const;
  base::string16 GetHideDetailsLabel() const;
  const std::vector<LabelInfo>& GetMainText() const;
  const std::vector<LabelInfo>& GetDetailsText() const;
  // |Accept()| will be called by the dialog when the user clicks the main
  // button, after which the dialog will be closed.
  void Accept();
  // |Cancel()| will be called by the dialog when the user clicks the dismiss
  // button on the top right, after which the dialog will be closed.
  void Cancel();

 private:
  ~SettingsResetPromptController();
  void InitMainText();
  void InitDetailsText();
  // Function to be called sometime after |Accept()| or |Cancel()| has been
  // called to perform any final tasks (such as metrcis reporting) and delete
  // this object.
  void OnInteractionDone();

  std::unique_ptr<SettingsResetPromptModel> model_;
  std::vector<SettingsResetPromptController::LabelInfo> details_text_;
  std::vector<SettingsResetPromptController::LabelInfo> main_text_;

  DISALLOW_COPY_AND_ASSIGN(SettingsResetPromptController);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_SETTINGS_RESET_PROMPT_CONTROLLER_H_
