// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdio>
#include <iostream>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/values.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_validator.h"

// Command line switches.
const char kSwitchErrorOnUnknownField[] = "error-on-unknown-field";
const char kSwitchErrorOnWrongRecommended[] = "error-on-wrong-recommended";
const char kSwitchErrorOnMissingField[] = "error-on-missing-field";
const char kSwitchManagedOnc[] = "managed-onc";
const char kSwitchUserPolicy[] = "user-policy";
const char kSwitchDevicePolicy[] = "device-policy";
const char kSwitchUserImport[] = "user-import";

const char* kSwitches[] = {
  kSwitchErrorOnUnknownField,
  kSwitchErrorOnWrongRecommended,
  kSwitchErrorOnMissingField,
  kSwitchManagedOnc,
  kSwitchUserPolicy,
  kSwitchDevicePolicy,
  kSwitchUserImport
};

// Return codes.
enum ReturnCode {
  kStatusValid = 0,
  kStatusWarnings = 1,
  kStatusInvalid = 2,
  kStatusJsonError = 3,
  kStatusArgumentError = 4,
};

const char kToplevelConfiguration[] = "ToplevelConfiguration";
const char kNetworkConfiguration[] = "NetworkConfiguration";
const char kCertificate[] = "Certificate";
const char* kTypes[] = {
  kToplevelConfiguration,
  kNetworkConfiguration,
  kCertificate
};

void PrintHelp() {
  fprintf(stderr,
          "Usage:\n"
          "  onc_validator [OPTION]... [TYPE] onc_file\n"
          "\n"
          "Valid TYPEs are:\n");
  for (size_t i = 0; i < arraysize(kTypes); ++i)
    fprintf(stderr, "  %s\n", kTypes[i]);

  fprintf(stderr,
          "\n"
          "Valid OPTIONs are:\n");
  for (size_t i = 0; i < arraysize(kSwitches); ++i)
    fprintf(stderr, "  --%s\n", kSwitches[i]);

  fprintf(stderr,
          "\n"
          "Exit status is one of:\n"
          "  %i  File is valid without warnings.\n"
          "  %i  File is valid with warnings,\n"
          "       i.e. there were errors which were degraded to warnings.\n"
          "  %i  File is invalid.\n"
          "  %i  File couldn't be read or is not a valid JSON dictionary.\n"
          "  %i  Some command line arguments are wrong.\n",
          kStatusValid,
          kStatusWarnings,
          kStatusInvalid,
          kStatusJsonError,
          kStatusArgumentError);
}

scoped_ptr<base::DictionaryValue> ReadDictionary(std::string filename) {
  base::FilePath path(filename);
  JSONFileValueSerializer serializer(path);
  serializer.set_allow_trailing_comma(true);

  base::DictionaryValue* dict = NULL;

  std::string json_error;
  base::Value* value = serializer.Deserialize(NULL, &json_error);
  if (!value) {
    LOG(ERROR) << "Couldn't json-deserialize file '" << filename
               << "': " << json_error;
    return make_scoped_ptr(dict);
  }

  if (!value->GetAsDictionary(&dict)) {
    LOG(ERROR) << "File '" << filename
               << "' does not contain a dictionary as expected, but type "
               << value->GetType();
  }

  return make_scoped_ptr(dict);
}

int main(int argc, const char* argv[]) {
  CommandLine::Init(argc, argv);

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  CommandLine::StringVector args = command_line.GetArgs();
  if (args.size() != 2) {
    PrintHelp();
    return kStatusArgumentError;
  }

  scoped_ptr<base::DictionaryValue> onc_object = ReadDictionary(args[1]);

  if (!onc_object)
    return kStatusJsonError;

  chromeos::onc::Validator validator(
      command_line.HasSwitch(kSwitchErrorOnUnknownField),
      command_line.HasSwitch(kSwitchErrorOnWrongRecommended),
      command_line.HasSwitch(kSwitchErrorOnMissingField),
      command_line.HasSwitch(kSwitchManagedOnc));

  if (command_line.HasSwitch(kSwitchUserPolicy))
    validator.SetOncSource(::onc::ONC_SOURCE_USER_POLICY);
  else if (command_line.HasSwitch(kSwitchDevicePolicy))
    validator.SetOncSource(::onc::ONC_SOURCE_DEVICE_POLICY);
  else if (command_line.HasSwitch(kSwitchUserImport))
    validator.SetOncSource(::onc::ONC_SOURCE_USER_IMPORT);

  std::string type_arg(args[0]);
  const chromeos::onc::OncValueSignature* signature = NULL;
  if (type_arg == kToplevelConfiguration) {
    signature = &chromeos::onc::kToplevelConfigurationSignature;
  } else if (type_arg == kNetworkConfiguration) {
    signature = &chromeos::onc::kNetworkConfigurationSignature;
  } else if (type_arg == kCertificate) {
    signature = &chromeos::onc::kCertificateSignature;
  } else {
    LOG(ERROR) << "Unknown ONC type '" << type_arg << "'";
    return kStatusArgumentError;
  }

  chromeos::onc::Validator::Result result;
  validator.ValidateAndRepairObject(signature, *onc_object, &result);

  switch(result) {
    case chromeos::onc::Validator::VALID:
      return kStatusValid;
    case chromeos::onc::Validator::VALID_WITH_WARNINGS:
      return kStatusWarnings;
    case chromeos::onc::Validator::INVALID:
      return kStatusInvalid;
    default:
      CHECK(false);
  }
}
