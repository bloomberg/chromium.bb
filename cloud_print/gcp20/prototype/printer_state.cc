// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/gcp20/prototype/printer_state.h"

#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/values.h"

namespace {

const char kRegistered[] = "registered";
const char kUser[] = "user";
const char kDeviceId[] = "device_id";
const char kRefreshToken[] = "refresh_token";
const char kXmppJid[] = "xmpp_jid";
const char kAccessToken[] = "access_token";
const char kAccessTokenUpdate[] = "access_token_update";

const char kLocalSettings[] = "local_settings";
const char kCdd[] = "cdd";
const char kLocalSettingsLocalDiscovery[] = "local_discovery";
const char kLocalSettingsAccessTokenEnabled[] = "access_token_enabled";
const char kLocalSettingsLocalPrintingEnabled[] =
    "printer/local_printing_enabled";
const char kLocalSettingsXmppTimeoutValue[] = "xmpp_timeout_value";

}  // namespace

PrinterState::PrinterState()
    : registration_state(UNREGISTERED),
      confirmation_state(CONFIRMATION_PENDING) {
}

PrinterState::~PrinterState() {
}

namespace printer_state {

bool SaveToFile(const base::FilePath& path, const PrinterState& state) {
  base::DictionaryValue json;
  if (state.registration_state == PrinterState::REGISTERED) {
    json.SetBoolean(kRegistered, true);
    json.SetString(kUser, state.user);
    json.SetString(kDeviceId, state.device_id);
    json.SetString(kRefreshToken, state.refresh_token);
    json.SetString(kXmppJid, state.xmpp_jid);
    json.SetString(kAccessToken, state.access_token);
    json.SetInteger(kAccessTokenUpdate,
                    static_cast<int>(state.access_token_update.ToTimeT()));

    scoped_ptr<base::DictionaryValue> local_settings(new base::DictionaryValue);
    local_settings->SetBoolean(kLocalSettingsLocalDiscovery,
                               state.local_settings.local_discovery);
    local_settings->SetBoolean(kLocalSettingsAccessTokenEnabled,
                               state.local_settings.access_token_enabled);
    local_settings->SetBoolean(kLocalSettingsLocalPrintingEnabled,
                               state.local_settings.local_printing_enabled);
    local_settings->SetInteger(kLocalSettingsXmppTimeoutValue,
                               state.local_settings.xmpp_timeout_value);
    json.Set(kLocalSettings, local_settings.release());
  } else {
    json.SetBoolean(kRegistered, false);
  }

  if (state.cdd.get())
    json.Set(kCdd, state.cdd->DeepCopy());

  std::string json_str;
  base::JSONWriter::WriteWithOptions(&json,
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &json_str);
  int size = base::checked_cast<int>(json_str.size());
  return (base::WriteFile(path, json_str.data(), size) == size);
}

bool LoadFromFile(const base::FilePath& path, PrinterState* state) {
  std::string json_str;
  if (!base::ReadFileToString(path, &json_str)) {
    LOG(ERROR) << "Cannot open file.";
    return false;
  }

  scoped_ptr<base::Value> json_val(base::JSONReader::Read(json_str));
  base::DictionaryValue* json = NULL;
  if (!json_val || !json_val->GetAsDictionary(&json)) {
    LOG(ERROR) << "Cannot read JSON dictionary from file.";
    return false;
  }

  bool registered = false;
  if (!json->GetBoolean(kRegistered, &registered)) {
    LOG(ERROR) << "Cannot parse |registered| state.";
    return false;
  }

  if (!registered)
    return true;

  std::string user;
  if (!json->GetString(kUser, &user)) {
    LOG(ERROR) << "Cannot parse |user|.";
    return false;
  }

  std::string device_id;
  if (!json->GetString(kDeviceId, &device_id)) {
    LOG(ERROR) << "Cannot parse |device_id|.";
    return false;
  }

  std::string refresh_token;
  if (!json->GetString(kRefreshToken, &refresh_token)) {
    LOG(ERROR) << "Cannot parse |refresh_token|.";
    return false;
  }

  std::string xmpp_jid;
  if (!json->GetString(kXmppJid, &xmpp_jid)) {
    LOG(ERROR) << "Cannot parse |xmpp_jid|.";
    return false;
  }

  std::string access_token;
  if (!json->GetString(kAccessToken, &access_token)) {
    LOG(ERROR) << "Cannot parse |access_token|.";
    return false;
  }

  int access_token_update;
  if (!json->GetInteger(kAccessTokenUpdate, &access_token_update)) {
    LOG(ERROR) << "Cannot parse |access_token_update|.";
    return false;
  }

  LocalSettings local_settings;
  base::DictionaryValue* settings_dict;
  if (!json->GetDictionary(kLocalSettings, &settings_dict)) {
    LOG(WARNING) << "Cannot read |local_settings|. Reset to default.";
  } else {
    if (!settings_dict->GetBoolean(kLocalSettingsLocalDiscovery,
                                   &local_settings.local_discovery) ||
        !settings_dict->GetBoolean(kLocalSettingsAccessTokenEnabled,
                                   &local_settings.access_token_enabled) ||
        !settings_dict->GetBoolean(kLocalSettingsLocalPrintingEnabled,
                                   &local_settings.local_printing_enabled) ||
        !settings_dict->GetInteger(kLocalSettingsXmppTimeoutValue,
                                   &local_settings.xmpp_timeout_value)) {
      LOG(WARNING) << "Cannot parse |local_settings|. Reset to default.";
      local_settings = LocalSettings();
    }
  }

  base::DictionaryValue* cdd_dict = NULL;
  if (!json->GetDictionary(kCdd, &cdd_dict))
    LOG(WARNING) << "Cannot read |cdd|. Reset to default.";

  *state = PrinterState();
  state->registration_state = PrinterState::REGISTERED;
  state->user = user;
  state->device_id = device_id;
  state->refresh_token = refresh_token;
  state->xmpp_jid = xmpp_jid;
  state->access_token = access_token;
  state->access_token_update = base::Time::FromTimeT(access_token_update);
  state->local_settings = local_settings;
  if (cdd_dict)
    state->cdd.reset(cdd_dict->DeepCopy());
  return true;
}

}  // namespace printer_state

