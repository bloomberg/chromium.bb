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

void FirstRunHandler::SetBackgroundVisible(bool visible) {
  web_ui()->CallJavascriptFunction("cr.FirstRun.setBackgroundVisible",
                                   base::FundamentalValue(visible));
}

void FirstRunHandler::AddRectangularHole(int x, int y, int width, int height) {
  web_ui()->CallJavascriptFunction("cr.FirstRun.addRectangularHole",
                                   base::FundamentalValue(x),
                                   base::FundamentalValue(y),
                                   base::FundamentalValue(width),
                                   base::FundamentalValue(height));
}

void FirstRunHandler::AddRoundHole(int x, int y, float radius) {
  web_ui()->CallJavascriptFunction("cr.FirstRun.addRoundHole",
                                   base::FundamentalValue(x),
                                   base::FundamentalValue(y),
                                   base::FundamentalValue(radius));
}

void FirstRunHandler::RemoveBackgroundHoles() {
  web_ui()->CallJavascriptFunction("cr.FirstRun.removeHoles");
}

void FirstRunHandler::ShowStepPositioned(const std::string& name,
                                         const StepPosition& position) {
  web_ui()->CallJavascriptFunction("cr.FirstRun.showStep",
                                   base::StringValue(name),
                                   *position.AsValue());
}

void FirstRunHandler::ShowStepPointingTo(const std::string& name,
                                         int x,
                                         int y,
                                         int offset) {
  scoped_ptr<base::Value> null(base::Value::CreateNullValue());
  base::ListValue point_with_offset;
  point_with_offset.AppendInteger(x);
  point_with_offset.AppendInteger(y);
  point_with_offset.AppendInteger(offset);
  web_ui()->CallJavascriptFunction("cr.FirstRun.showStep",
                                   base::StringValue(name),
                                   *null,
                                   point_with_offset);
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

