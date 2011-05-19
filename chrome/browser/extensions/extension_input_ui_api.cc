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
#include "third_party/cros/chromeos_input_method_ui.h"

namespace events {

const char kOnUpdateAuxiliaryText[] =
    "experimental.inputUI.onUpdateAuxiliaryText";
const char kOnUpdateLookupTable[] = "experimental.inputUI.onUpdateLookupTable";
const char kOnSetCursorLocation[] = "experimental.inputUI.onSetCursorLocation";

}  // namespace events

class InputUiController {
 public:
  explicit InputUiController(ExtensionInputUiEventRouter* router);
  ~InputUiController();

  void CandidateClicked(int index, int button, int flags);
  void CursorUp();
  void CursorDown();
  void PageUp();
  void PageDown();

 private:
  // The function is called when |HideAuxiliaryText| signal is received in
  // libcros. |ui_controller| is a void pointer to this object.
  static void OnHideAuxiliaryText(void* ui_controller);

  // The function is called when |HideLookupTable| signal is received in
  // libcros. |ui_controller| is a void pointer to this object.
  static void OnHideLookupTable(void* ui_controller);

  // The function is called when |SetCursorLocation| signal is received
  // in libcros. |ui_controller| is a void pointer to this object.
  static void OnSetCursorLocation(void* ui_controller,
                                  int x,
                                  int y,
                                  int width,
                                  int height);

  // The function is called when |UpdateAuxiliaryText| signal is received
  // in libcros. |ui_controller| is a void pointer to this object.
  static void OnUpdateAuxiliaryText(void* ui_controller,
                                    const std::string& utf8_text,
                                    bool visible);

  // The function is called when |UpdateLookupTable| signal is received
  // in libcros. |ui_controller| is a void pointer to this object.
  static void OnUpdateLookupTable(void* ui_controller,
      const chromeos::InputMethodLookupTable& lookup_table);

  // This function is called by libcros when ibus connects or disconnects.
  // |ui_controller| is a void pointer to this object.
  static void OnConnectionChange(void* ui_controller, bool connected);

  ExtensionInputUiEventRouter* router_;
  chromeos::InputMethodUiStatusConnection* ui_status_connection_;

  DISALLOW_COPY_AND_ASSIGN(InputUiController);
};

InputUiController::InputUiController(
    ExtensionInputUiEventRouter* router) :
  router_(router),
  ui_status_connection_(NULL) {
  if (!chromeos::CrosLibrary::Get()->EnsureLoaded())
    return;

  chromeos::InputMethodUiStatusMonitorFunctions functions;
  functions.hide_auxiliary_text =
      &InputUiController::OnHideAuxiliaryText;
  functions.hide_lookup_table =
      &InputUiController::OnHideLookupTable;
  functions.set_cursor_location =
      &InputUiController::OnSetCursorLocation;
  functions.update_auxiliary_text =
      &InputUiController::OnUpdateAuxiliaryText;
  functions.update_lookup_table =
      &InputUiController::OnUpdateLookupTable;
  ui_status_connection_ = chromeos::MonitorInputMethodUiStatus(functions, this);

  if (!ui_status_connection_)
    LOG(ERROR) << "chromeos::MonitorInputMethodUiStatus() failed!";
}

InputUiController::~InputUiController() {
  if (ui_status_connection_)
    chromeos::DisconnectInputMethodUiStatus(ui_status_connection_);
}

void InputUiController::CandidateClicked(
    int index, int button, int flags) {
  chromeos::NotifyCandidateClicked(ui_status_connection_, index, button, flags);
}

void InputUiController::CursorUp() {
  chromeos::NotifyCursorUp(ui_status_connection_);
}

void InputUiController::CursorDown() {
  chromeos::NotifyCursorDown(ui_status_connection_);
}

void InputUiController::PageUp() {
  chromeos::NotifyPageUp(ui_status_connection_);
}

void InputUiController::PageDown() {
  chromeos::NotifyPageDown(ui_status_connection_);
}

void InputUiController::OnHideAuxiliaryText(
    void* ui_controller) {
  InputUiController *self = static_cast<InputUiController*>(ui_controller);
  self->router_->OnHideAuxiliaryText();
}

void InputUiController::OnHideLookupTable(
    void* ui_controller) {
  InputUiController *self = static_cast<InputUiController*>(ui_controller);
  self->router_->OnHideLookupTable();
}

void InputUiController::OnSetCursorLocation(
    void* ui_controller,
    int x, int y, int width, int height) {
  InputUiController *self = static_cast<InputUiController*>(ui_controller);
  self->router_->OnSetCursorLocation(x, y, width, height);
}

void InputUiController::OnUpdateAuxiliaryText(
    void* ui_controller,
    const std::string& utf8_text,
    bool visible) {
  InputUiController *self = static_cast<InputUiController*>(ui_controller);
  if (!visible) {
    self->router_->OnHideAuxiliaryText();
  } else {
    self->router_->OnUpdateAuxiliaryText(utf8_text);
  }
}

void InputUiController::OnUpdateLookupTable(
    void* ui_controller,
    const chromeos::InputMethodLookupTable& lookup_table) {
  InputUiController *self = static_cast<InputUiController*>(ui_controller);
    self->router_->OnUpdateLookupTable(lookup_table);
}

ExtensionInputUiEventRouter*
ExtensionInputUiEventRouter::GetInstance() {
  return Singleton<ExtensionInputUiEventRouter>::get();
}

ExtensionInputUiEventRouter::ExtensionInputUiEventRouter()
  : profile_(NULL),
    ui_controller_(NULL) {
}

ExtensionInputUiEventRouter::~ExtensionInputUiEventRouter() {
}

void ExtensionInputUiEventRouter::Init() {
  if (ui_controller_.get() == NULL) {
    ui_controller_.reset(new InputUiController(this));
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
    ui_controller_->CandidateClicked(index, button, 0);
}

void ExtensionInputUiEventRouter::CursorUp(Profile* profile,
    const std::string& extension_id) {
    if (profile_ != profile || extension_id_ != extension_id) {
      DLOG(WARNING) << "called from unregistered extension";
    }
    ui_controller_->CursorUp();
}

void ExtensionInputUiEventRouter::CursorDown(Profile* profile,
    const std::string& extension_id) {
    if (profile_ != profile || extension_id_ != extension_id) {
      DLOG(WARNING) << "called from unregistered extension";
    }
    ui_controller_->CursorDown();
}

void ExtensionInputUiEventRouter::PageUp(Profile* profile,
    const std::string& extension_id) {
    if (profile_ != profile || extension_id_ != extension_id) {
      DLOG(WARNING) << "called from unregistered extension";
    }
    ui_controller_->PageUp();
}

void ExtensionInputUiEventRouter::PageDown(Profile* profile,
    const std::string& extension_id) {
    if (profile_ != profile || extension_id_ != extension_id) {
      DLOG(WARNING) << "called from unregistered extension";
    }
    ui_controller_->PageDown();
}

void ExtensionInputUiEventRouter::OnHideAuxiliaryText() {
  OnUpdateAuxiliaryText("");
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
    const std::string& utf8_text) {
  if (profile_ == NULL || extension_id_.empty())
    return;

  ListValue args;
  args.Append(Value::CreateStringValue(utf8_text));

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);
  profile_->GetExtensionEventRouter()->DispatchEventToExtension(
    extension_id_, events::kOnUpdateAuxiliaryText, json_args, profile_, GURL());
}

void ExtensionInputUiEventRouter::OnUpdateLookupTable(
    const chromeos::InputMethodLookupTable& lookup_table) {
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

