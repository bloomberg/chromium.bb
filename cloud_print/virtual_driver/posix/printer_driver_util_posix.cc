// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <pwd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <cups/backend.h>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/string_tokenizer.h"
#include "base/values.h"

#include "cloud_print/virtual_driver/posix/printer_driver_util_posix.h"

namespace printer_driver_util {

void WriteToTemp(FILE* input_pdf, FilePath output_path) {
  FILE* output_pdf;
  char buffer[128];
  output_pdf = fopen(output_path.value().c_str(), "w");
  if (output_pdf == NULL) {
    LOG(ERROR) << "Unable to open file handle for writing output file";
    exit(CUPS_BACKEND_CANCEL);
  }
  // Read from input file or stdin and write to output file.
  while (fgets(buffer, sizeof(buffer), input_pdf) != NULL) {
    fputs(buffer, output_pdf);
  }

  LOG(INFO) << "Successfully wrote output file";
  // ensure everything is written, then close the files.
  fflush(output_pdf);
  fclose(input_pdf);
  fclose(output_pdf);
}

// Sets the UID of the process to that of the username.
void SetUser(const char* user) {
  struct passwd* calling_user = NULL;
  calling_user = getpwnam(user);
  if (calling_user == NULL) {
    LOG(ERROR) << "Unable to get calling user";
    exit(CUPS_BACKEND_CANCEL);
  }
  if (!setuid(calling_user->pw_uid) == -1)  {
    LOG(ERROR) << "Unable to set UID";
    exit(CUPS_BACKEND_CANCEL);
  }

  LOG(INFO) << "Successfully set user and group ID";
}

// Parses the options passed in on the command line to key value
// JSON pairs. Assumes that the input options string is of the
// format KEY=VALUE, with expressions being separated by spaces.
// Fails if print_ticket cannot be written to.
void GetOptions(const char* options, std::string* print_ticket) {
  if (options == NULL) {
    *(print_ticket) = "{}";
    return;
  }
  CStringTokenizer t(options, options + strlen(options), " ");
  DictionaryValue* json_options = new DictionaryValue;

  while (t.GetNext()) {
    std::string token = t.token();
    // If the token ends with a slash, that indicates
    // that the next space is actually escaped
    // So we append the next token onto this token
    // if possible. We also replace the slash with a space
    // since the JSON will expect an unescaped string.
    while (token.at(token.length()-1) == '\\') {
      if (t.GetNext()) {
        token.replace(token.length()-1, 1, " ");
        token.append(t.token());
      } else {
        break;
      }
    }
    size_t pos = token.find("=");
    if (pos == std::string::npos) {
      continue;
    }
    std::string option_name = token.substr(0, pos);
    std::string option_value = token.substr(pos+1);
    base::StringValue* val= base::Value::CreateStringValue(option_value);
    json_options->SetWithoutPathExpansion(option_name, val);
  }
  base::JSONWriter::Write(json_options, /*pretty_print=*/false, print_ticket);
  delete json_options;
}

}  // namespace printer_driver_util

