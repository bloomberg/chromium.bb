// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_FIRST_RUN_FIRST_RUN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_FIRST_RUN_FIRST_RUN_HANDLER_H_

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/ui/webui/chromeos/first_run/first_run_actor.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace chromeos {

class StepPosition;

class FirstRunHandler : public FirstRunActor,
                        public content::WebUIMessageHandler {
 public:
  FirstRunHandler();
  // Overriden from FirstRunActor.
  virtual bool IsInitialized() OVERRIDE;
  virtual void SetBackgroundVisible(bool visible) OVERRIDE;
  virtual void AddRectangularHole(int x, int y, int width, int height) OVERRIDE;
  virtual void AddRoundHole(int x, int y, float radius) OVERRIDE;
  virtual void RemoveBackgroundHoles() OVERRIDE;
  virtual void ShowStepPositioned(const std::string& name,
                                  const StepPosition& position) OVERRIDE;
  virtual void ShowStepPointingTo(const std::string& name,
                                  int x,
                                  int y,
                                  int offset) OVERRIDE;
  virtual void HideCurrentStep() OVERRIDE;
  virtual void Finalize() OVERRIDE;
  virtual bool IsFinalizing() OVERRIDE;

 private:
  // Overriden from content::WebUIMessageHandler.
  virtual void RegisterMessages() OVERRIDE;

  // Handlers for calls from JS.
  void HandleInitialized(const base::ListValue* args);
  void HandleNextButtonClicked(const base::ListValue* args);
  void HandleHelpButtonClicked(const base::ListValue* args);
  void HandleStepShown(const base::ListValue* args);
  void HandleStepHidden(const base::ListValue* args);
  void HandleFinalized(const base::ListValue* args);

  bool is_initialized_;
  bool is_finalizing_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_FIRST_RUN_FIRST_RUN_HANDLER_H_

