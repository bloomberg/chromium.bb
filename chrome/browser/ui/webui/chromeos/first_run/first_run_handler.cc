// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/first_run/first_run_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "content/public/browser/web_ui.h"

namespace chromeos {

FirstRunHandler::FirstRunHandler()
    : is_initialized_(false) {
}

bool FirstRunHandler::IsInitialized() {
  return is_initialized_;
}

void FirstRunHandler::AddBackgroundHole(int x, int y, int width, int height) {
  web_ui()->CallJavascriptFunction("cr.FirstRun.addHole",
                                   base::FundamentalValue(x),
                                   base::FundamentalValue(y),
                                   base::FundamentalValue(width),
                                   base::FundamentalValue(height));
}

void FirstRunHandler::RemoveBackgroundHoles() {
  web_ui()->CallJavascriptFunction("cr.FirstRun.removeHoles");
}

void FirstRunHandler::ShowStep(const std::string& name,
                               const StepPosition& position) {
  web_ui()->CallJavascriptFunction("cr.FirstRun.showStep",
                                   base::StringValue(name),
                                   *position.AsValue());
}

void FirstRunHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("initialized",
      base::Bind(&FirstRunHandler::HandleInitialized, base::Unretained(this)));
  web_ui()->RegisterMessageCallback("nextButtonClicked",
      base::Bind(&FirstRunHandler::HandleNextButtonClicked,
                 base::Unretained(this)));
}

void FirstRunHandler::HandleInitialized(const base::ListValue* args) {
  is_initialized_ = true;
  if (delegate())
    delegate()->OnActorInitialized();
}

void FirstRunHandler::HandleNextButtonClicked(const base::ListValue* args) {
  std::string step_name;
  CHECK(args->GetString(0, &step_name));
  if (delegate())
    delegate()->OnNextButtonClicked(step_name);
}

}  // namespace chromeos

