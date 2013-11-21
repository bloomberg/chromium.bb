// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_GCP20_PROTOTYPE_PRINTER_STATE_H_
#define CLOUD_PRINT_GCP20_PROTOTYPE_PRINTER_STATE_H_

#include <string>

#include "base/memory/linked_ptr.h"
#include "base/time/time.h"
#include "cloud_print/gcp20/prototype/local_settings.h"

namespace base {
class DictionaryValue;
class FilePath;
}  // namespace base

struct PrinterState {
  enum RegistrationState {
    UNREGISTERED,
    REGISTRATION_STARTED,  // |action=start| was called,
                           // request to CloudPrint was sent.
    REGISTRATION_CLAIM_TOKEN_READY,  // The same as previous, but request
                                     // reply is already received.
    REGISTRATION_COMPLETING,  // |action=complete| was called,
                              // |complete| request was sent.
    REGISTRATION_ERROR,  // Is set when server error was occurred.
    REGISTERED,
  };

  enum ConfirmationState {
    CONFIRMATION_PENDING,
    CONFIRMATION_CONFIRMED,
    CONFIRMATION_DISCARDED,
    CONFIRMATION_TIMEOUT,
  };

  PrinterState();
  ~PrinterState();

  // Registration process info
  std::string user;
  std::string registration_token;
  std::string complete_invite_url;
  RegistrationState registration_state;
  ConfirmationState confirmation_state;

  // Printer workflow info
  std::string refresh_token;
  std::string device_id;
  std::string xmpp_jid;
  LocalSettings local_settings;
  linked_ptr<base::DictionaryValue> cdd;

  // Last valid |access_token|.
  std::string access_token;
  base::Time access_token_update;

  // Contains error if |REGISTRATION_ERROR| is set.
  std::string error_description;
};

namespace printer_state {

//
bool SaveToFile(const base::FilePath& path, const PrinterState& state);

//
bool LoadFromFile(const base::FilePath& path, PrinterState* state);

}  // namespace printer_state

#endif  // CLOUD_PRINT_GCP20_PROTOTYPE_PRINTER_STATE_H_

