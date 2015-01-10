// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/copresence_ui_handler.h"

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/copresence/copresence_api.h"
#include "components/copresence/proto/data.pb.h"
#include "components/copresence/public/copresence_manager.h"
#include "components/copresence/public/copresence_state.h"
#include "components/copresence/tokens.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/time_format.h"

using base::ListValue;
using base::DictionaryValue;
using content::WebUI;
using copresence::Directive;
using copresence::ReceivedToken;
using copresence::TransmittedToken;
using extensions::CopresenceService;

// TODO(ckehoe): Make debug strings translatable?

namespace {

std::string FormatInstructionType(
    copresence::TokenInstructionType directive_type) {
  switch (directive_type) {
    case copresence::TRANSMIT:
      return "Transmit";

    case copresence::RECEIVE:
      return "Receive";

    default:
      NOTREACHED();
      return "Unknown";
  }
}

std::string FormatMedium(copresence::TokenMedium medium) {
  switch (medium) {
    case copresence::AUDIO_ULTRASOUND_PASSBAND:
      return "Ultrasound";

    case copresence::AUDIO_AUDIBLE_DTMF:
      return "Audible";

    default:
      NOTREACHED();
      return "Unknown";
  }
}

std::string ConvertStatus(const TransmittedToken& token) {
  bool done = token.stop_time < base::Time::Now();
  std::string status = done ? "done" : "active";
  if (token.broadcast_confirmed)
    status += " confirmed";
  return status;
}

std::string ConvertStatus(const ReceivedToken& token) {
  switch (token.valid) {
    case ReceivedToken::VALID:
      return "valid";

    case ReceivedToken::INVALID:
      return "invalid";

    case ReceivedToken::UNKNOWN:
      return std::string();

    default:
      NOTREACHED();
      return std::string();
  }
}

template<class T>
scoped_ptr<DictionaryValue> FormatToken(const T& token) {
  scoped_ptr<DictionaryValue> js_token(new DictionaryValue);

  js_token->SetString("id", token.id);
  js_token->SetString("statuses", ConvertStatus(token));
  js_token->SetString("medium", FormatMedium(token.medium));
  DCHECK(!token.start_time.is_null());
  js_token->SetString("time",
      base::TimeFormatTimeOfDay(token.start_time));

  return js_token.Pass();
}

// Retrieve the CopresenceService, if any.
CopresenceService* GetCopresenceService(WebUI* web_ui) {
  DCHECK(web_ui);
  return CopresenceService::GetFactoryInstance()->Get(
      web_ui->GetWebContents()->GetBrowserContext());
}

// Safely retrieve the CopresenceState, if any.
copresence::CopresenceState* GetCopresenceState(CopresenceService* service) {
  // During shutdown, there may be no CopresenceService.
  return service && service->manager() ? service->manager()->state() : nullptr;
}

// Safely retrieve the CopresenceState, if any. It would be cleaner if we could
// put this into CopresenceUIHandler and call WebUIMessageHandler::web_ui()
// instead of taking an argument. However, it turns out that web_ui() returns
// null when called in the constructor. So we pass in the web_ui explicitly.
copresence::CopresenceState* GetCopresenceState(WebUI* web_ui) {
  return GetCopresenceState(GetCopresenceService(web_ui));
}

}  // namespace


// Public functions.

CopresenceUIHandler::CopresenceUIHandler(WebUI* web_ui)
    : state_(GetCopresenceState(web_ui)) {
  DCHECK(state_);
  state_->AddObserver(this);
}

CopresenceUIHandler::~CopresenceUIHandler() {
  // Check if the CopresenceService is still up before unregistering.
  state_ = GetCopresenceState(web_ui());
  if (state_)
    state_->RemoveObserver(this);
}


// Private functions.

void CopresenceUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "populateCopresenceState",
      base::Bind(&CopresenceUIHandler::HandlePopulateState,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "clearCopresenceState",
      base::Bind(&CopresenceUIHandler::HandleClearState,
                 base::Unretained(this)));
}

void CopresenceUIHandler::DirectivesUpdated() {
  ListValue js_directives;
  for (const Directive& directive : state_->active_directives()) {
    scoped_ptr<DictionaryValue> js_directive(new DictionaryValue);

    js_directive->SetString("type", FormatInstructionType(
        directive.token_instruction().token_instruction_type()));
    js_directive->SetString("medium", FormatMedium(
        directive.token_instruction().medium()));
    js_directive->SetString("duration", ui::TimeFormat::Simple(
        ui::TimeFormat::FORMAT_DURATION,
        ui::TimeFormat::LENGTH_LONG,
        base::TimeDelta::FromMilliseconds(directive.ttl_millis())));

    js_directives.Append(js_directive.release());
  }

  web_ui()->CallJavascriptFunction("refreshDirectives", js_directives);
}

void CopresenceUIHandler::TokenTransmitted(const TransmittedToken& token) {
  web_ui()->CallJavascriptFunction("updateTransmittedToken",
                                   *FormatToken(token));
}

void CopresenceUIHandler::TokenReceived(const ReceivedToken& token) {
  web_ui()->CallJavascriptFunction("updateReceivedToken",
                                   *FormatToken(token));
}

void CopresenceUIHandler::HandlePopulateState(const ListValue* args) {
  DCHECK(args->empty());
  DirectivesUpdated();
  // TODO(ckehoe): Pass tokens to JS as a batch.
  for (const auto& token_entry : state_->transmitted_tokens())
    TokenTransmitted(token_entry.second);
  for (const auto& token_entry : state_->received_tokens())
    TokenReceived(token_entry.second);
}

void CopresenceUIHandler::HandleClearState(const ListValue* args) {
  DCHECK(args->empty());

  CopresenceService* service = GetCopresenceService(web_ui());
  DCHECK(service);
  service->ResetState();

  // CopresenceService::ResetState() deletes the CopresenceState object
  // we were using. We have to get the new one and reconnect to it.
  state_ = GetCopresenceState(service);
  DCHECK(state_);
  state_->AddObserver(this);

  web_ui()->CallJavascriptFunction("clearTokens");
  DirectivesUpdated();
}
