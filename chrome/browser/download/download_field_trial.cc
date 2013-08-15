// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_field_trial.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

namespace {

const char kCondition1Control[] = "Condition1Control";
const char kCondition2Control[] = "Condition2Control";
const char kCondition3Malicious[] = "Condition3Malicious";
const char kCondition4Unsafe[] = "Condition4Unsafe";
const char kCondition5Dangerous[] = "Condition5Dangerous";
const char kCondition6Harmful[] = "Condition6Harmful";
const char kCondition7DiscardSecond[] = "Condition7DiscardSecond";
const char kCondition8DiscardFirst[] = "Condition8DiscardFirst";
const char kCondition9SafeDiscard[] = "Condition9SafeDiscard";
const char kCondition10SafeDontRun[] = "Condition10SafeDontRun";

}  // namespace

const char kMalwareWarningFinchTrialName[] = "MalwareDownloadWarning";

base::string16 AssembleMalwareFinchString(
    const std::string& trial_condition,
    const base::string16& elided_filename) {
  // Sanity check to make sure we have a filename.
  base::string16 filename;
  if (elided_filename.empty()) {
    filename = ASCIIToUTF16("This file");
  } else {
    filename = ReplaceStringPlaceholders(
        ASCIIToUTF16("File '$1'"), elided_filename, NULL);
  }

  // Set the message text according to the condition.
  if (trial_condition == kCondition1Control) {
    return ASCIIToUTF16("This file appears malicious.");
  }
  base::string16 message_text;
  if (trial_condition == kCondition2Control) {
    message_text = ASCIIToUTF16("$1 appears malicious.");
  } else if (trial_condition == kCondition3Malicious) {
    message_text = ASCIIToUTF16("$1 is malicious.");
  } else if (trial_condition == kCondition4Unsafe) {
    message_text = ASCIIToUTF16("$1 is unsafe.");
  } else if (trial_condition == kCondition5Dangerous) {
    message_text = ASCIIToUTF16("$1 is dangerous.");
  } else if (trial_condition == kCondition6Harmful) {
    message_text = ASCIIToUTF16("$1 is harmful.");
  } else if (trial_condition == kCondition7DiscardSecond) {
    message_text = ASCIIToUTF16(
        "$1 is malicious. Discard this file to stay safe.");
  } else if (trial_condition == kCondition8DiscardFirst) {
    message_text = ASCIIToUTF16(
        "Discard this file to stay safe. $1 is malicious.");
  } else if (trial_condition == kCondition9SafeDiscard) {
    message_text = ASCIIToUTF16("$1 is malicious. To stay safe, discard it.");
  } else if (trial_condition == kCondition10SafeDontRun) {
    message_text = ASCIIToUTF16("$1 is malicious. To stay safe, don't run it.");
  } else {
    // We use the second control as a default for other conditions that don't
    // change the warning string.
    message_text = ASCIIToUTF16("$1 appears malicious.");
  }

  return ReplaceStringPlaceholders(message_text, filename, NULL);
}
