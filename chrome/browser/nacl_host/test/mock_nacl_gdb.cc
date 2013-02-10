// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdio>
#include <cstring>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/process_util.h"

static const char kArgs[] = "--args";
static const char kEvalCommand[] = "--eval-command";
static const char kCommand[] = "--command";
static const char kNaClIrt[] = "nacl-irt ";
static const char kPass[] = "PASS";
static const char kDump[] = "dump binary value ";
static const char kAttach[] = "attach ";

// Send message to child nacl_helper
void SendMessage(const char* arg) {
  const char* file_end = strchr(arg, ' ');
  CHECK(file_end);
  char buf = '\0';
  file_util::WriteFile(
      base::FilePath(base::FilePath::StringType(arg, file_end)), &buf, 1);
}

int main(int argc, char** argv) {
  scoped_ptr<base::Environment> env(base::Environment::Create());
  std::string mock_nacl_gdb_file;
  env->GetVar("MOCK_NACL_GDB", &mock_nacl_gdb_file);
  file_util::WriteFile(base::FilePath::FromUTF8Unsafe(mock_nacl_gdb_file),
                       kPass, strlen(kPass));
  CHECK_GE(argc, 3);
  // First argument should be --eval-command.
  CHECK_EQ(strcmp(argv[1], kEvalCommand), 0);
  // Second argument should start with nacl-irt.
  CHECK_GE(strlen(argv[2]), strlen(kNaClIrt));
  CHECK_EQ(strncmp(argv[2], kNaClIrt, strlen(kNaClIrt)), 0);
  char* irt_file_name = &argv[2][strlen(kNaClIrt)];
  FILE* irt_file = fopen(irt_file_name, "r");
  // nacl-irt parameter must be a file name.
  PCHECK(irt_file);
  fclose(irt_file);
  int i = 3;
  bool has_attach_cmd = false;
  char* message_pipe = NULL;
  // Skip additional --eval-command parameters.
  while (i < argc) {
    if (strcmp(argv[i], kArgs) == 0) {
      i++;
      break;
    }
    if (strcmp(argv[i], kEvalCommand) == 0) {
      i += 2;
      // Command line shouldn't end with --eval-command switch without value.
      CHECK_LE(i, argc);
      if (strncmp(argv[i - 1], kDump, sizeof(kDump) - 1) == 0) {
        message_pipe = argv[i - 1] + sizeof(kDump) - 1;
      } else if (strncmp(argv[i - 1], kAttach, sizeof(kAttach) - 1) == 0) {
        has_attach_cmd = true;
      }
      continue;
    }
    if (strcmp(argv[i], kCommand) == 0) {
      // Command line shouldn't end with --command switch without value.
      i += 2;
      CHECK_LE(i, argc);
      std::string nacl_gdb_script(argv[i - 1]);
      file_util::WriteFile(base::FilePath::FromUTF8Unsafe(nacl_gdb_script),
                           kPass, strlen(kPass));
      continue;
    }
    // Unknown argument.
    NOTREACHED() << "Invalid argument " << argv[i];
  }
  if (has_attach_cmd) {
    CHECK_EQ(i, argc);
    CHECK(message_pipe);
    // Test passed, so we can let NaCl launching to continue.
    SendMessage(message_pipe);
    return 0;
  }
  // --args switch must be present.
  CHECK_LT(i, argc);

  CommandLine::StringVector arguments;
  for (; i < argc; i++) {
    arguments.push_back(
        CommandLine::StringType(argv[i], argv[i] + strlen(argv[i])));
  }
  CommandLine cmd_line(arguments);
  // Process must be launched successfully.
  PCHECK(base::LaunchProcess(cmd_line, base::LaunchOptions(), NULL));
  return 0;
}
