// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_input_ui_api.h"

#include <algorithm>
#include <string>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/profiles/profile.h"
#include "third_party/cros/chromeos_cros_api.h"

namespace events {

const char kOnUpdateAuxiliaryText[] =
    "experimental.input.ui.onUpdateAuxiliaryText";
const char kOnUpdateLookupTable[] = "experimental.input.ui.onUpdateLookupTable";
const char kOnSetCursorLocation[] = "experimental.input.ui.onSetCursorLocation";

}  // namespace events

ExtensionInputUiEventRouter*
ExtensionInputUiEventRouter::GetInstance() {
  return Singleton<ExtensionInputUiEventRouter>::get();
}

ExtensionInputUiEventRouter::ExtensionInputUiEventRouter()
  : profile_(NULL),
    ibus_ui_controller_(NULL) {
}

ExtensionInputUiEventRouter::~ExtensionInputUiEventRouter() {
}

void ExtensionInputUiEventRouter::Init() {
  if (ibus_ui_controller_.get() == NULL) {
    ibus_ui_controller_.reset(
        chromeos::input_method::IBusUiController::Create());
    // The observer should be added before Connect() so we can capture the
    // initial connection change.
    ibus_ui_controller_->AddObserver(this);
    ibus_ui_controller_->Connect();
  }
}

void ExtensionInputUiEventRouter::Register(
    Profile* profile, const std::string& extension_id) {
  profile_ = profile;
  extension_id_ = extension_id;
}

void ExtensionInputUiEventRouter::CandidateClicked(Profile* profile,
    const std::string& extension_id, int index, int button) {
    if (profile_ != profile || extension_id_ != extension_id) {
      DLOG(WARNING) << "called from unregistered extension";
    }
    ibus_ui_controller_->NotifyCandidateClicked(index, button, 0);
}

void ExtensionInputUiEventRouter::CursorUp(Profile* profile,
    const std::string& extension_id) {
    if (profile_ != profile || extension_id_ != extension_id) {
      DLOG(WARNING) << "called from unregistered extension";
    }
    ibus_ui_controller_->NotifyCursorUp();
}

void ExtensionInputUiEventRouter::CursorDown(Profile* profile,
    const std::string& extension_id) {
    if (profile_ != profile || extension_id_ != extension_id) {
      DLOG(WARNING) << "called from unregistered extension";
    }
    ibus_ui_controller_->NotifyCursorDown();
}

void ExtensionInputUiEventRouter::PageUp(Profile* profile,
    const std::string& extension_id) {
    if (profile_ != profile || extension_id_ != extension_id) {
      DLOG(WARNING) << "called from unregistered extension";
    }
    ibus_ui_controller_->NotifyPageUp();
}

void ExtensionInputUiEventRouter::PageDown(Profile* profile,
    const std::string& extension_id) {
    if (profile_ != profile || extension_id_ != extension_id) {
      DLOG(WARNING) << "called from unregistered extension";
    }
    ibus_ui_controller_->NotifyPageDown();
}

void ExtensionInputUiEventRouter::OnHideAuxiliaryText() {
  OnUpdateAuxiliaryText("", false);
}

void ExtensionInputUiEventRouter::OnHideLookupTable() {
  if (profile_ == NULL || extension_id_.empty())
    return;

  DictionaryValue* dict = new DictionaryValue();
  dict->SetBoolean("visible", false);

  ListValue *candidates = new ListValue();
  dict->Set("candidates", candidates);

  ListValue args;
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  profile_->GetExtensionEventRouter()->DispatchEventToExtension(
    extension_id_, events::kOnUpdateLookupTable, json_args, profile_, GURL());
}

void ExtensionInputUiEventRouter::OnHidePreeditText() {
}

void ExtensionInputUiEventRouter::OnSetCursorLocation(
    int x, int y, int width, int height) {

  if (profile_ == NULL || extension_id_.empty())
    return;

  ListValue args;
  args.Append(Value::CreateIntegerValue(x));
  args.Append(Value::CreateIntegerValue(y));
  args.Append(Value::CreateIntegerValue(width));
  args.Append(Value::CreateIntegerValue(height));

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  profile_->GetExtensionEventRouter()->DispatchEventToExtension(
    extension_id_, events::kOnSetCursorLocation, json_args, profile_, GURL());
}

void ExtensionInputUiEventRouter::OnUpdateAuxiliaryText(
    const std::string& utf8_text,
    bool visible) {
  if (profile_ == NULL || extension_id_.empty())
    return;

  ListValue args;
  args.Append(Value::CreateStringValue(visible ? utf8_text : ""));

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  profile_->GetExtensionEventRouter()->DispatchEventToExtension(
    extension_id_, events::kOnUpdateAuxiliaryText, json_args, profile_, GURL());
}

void ExtensionInputUiEventRouter::OnUpdateLookupTable(
    const chromeos::input_method::InputMethodLookupTable& lookup_table) {
  if (profile_ == NULL || extension_id_.empty())
    return;

  DictionaryValue* dict = new DictionaryValue();
  dict->SetBoolean("visible", lookup_table.visible);

  if (lookup_table.visible) {
  }

  ListValue *candidates = new ListValue();

  size_t page = lookup_table.cursor_absolute_index / lookup_table.page_size;
  size_t begin = page * lookup_table.page_size;
  size_t end = std::min(begin + lookup_table.page_size,
      lookup_table.candidates.size());

  for (size_t i = begin; i < end; i++) {
    candidates->Append(Value::CreateStringValue(lookup_table.candidates[i]));
  }

  dict->Set("candidates", candidates);

  ListValue args;
  args.Append(dict);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  profile_->GetExtensionEventRouter()->DispatchEventToExtension(
    extension_id_, events::kOnUpdateLookupTable, json_args, profile_, GURL());
}

void ExtensionInputUiEventRouter::OnUpdatePreeditText(
    const std::string& utf8_text,
    unsigned int cursor,
    bool visible) {
}

void ExtensionInputUiEventRouter::OnConnectionChange(bool connected) {
}

bool RegisterInputUiFunction::RunImpl() {
  ExtensionInputUiEventRouter::GetInstance()->Register(
      profile(), extension_id());
  return true;
}

bool CandidateClickedInputUiFunction::RunImpl() {
  int index = 0;
  int button = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &index));
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(1, &button));

  ExtensionInputUiEventRouter::GetInstance()->CandidateClicked(
      profile(), extension_id(), index, button);

  return true;
}

bool CursorUpInputUiFunction::RunImpl() {
  ExtensionInputUiEventRouter::GetInstance()->CursorUp(
      profile(), extension_id());
  return true;
}

bool CursorDownInputUiFunction::RunImpl() {
  ExtensionInputUiEventRouter::GetInstance()->CursorDown(
      profile(), extension_id());
  return true;
}

bool PageUpInputUiFunction::RunImpl() {
  ExtensionInputUiEventRouter::GetInstance()->PageUp(
      profile(), extension_id());
  return true;
}

bool PageDownInputUiFunction::RunImpl() {
  ExtensionInputUiEventRouter::GetInstance()->PageDown(
      profile(), extension_id());
  return true;
}
