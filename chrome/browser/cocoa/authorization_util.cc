// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/authorization_util.h"

#include <sys/wait.h>

#include <string>

#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "base/string_util.h"

namespace authorization_util {

OSStatus ExecuteWithPrivilegesAndGetPID(AuthorizationRef authorization,
                                        const char* tool_path,
                                        AuthorizationFlags options,
                                        const char** arguments,
                                        FILE** pipe,
                                        pid_t* pid) {
  // pipe may be NULL, but this function needs one.  In that case, use a local
  // pipe.
  FILE* local_pipe;
  FILE** pipe_pointer;
  if (pipe) {
    pipe_pointer = pipe;
  } else {
    pipe_pointer = &local_pipe;
  }

  // AuthorizationExecuteWithPrivileges wants |char* const*| for |arguments|,
  // but it doesn't actually modify the arguments, and that type is kind of
  // silly and callers probably aren't dealing with that.  Put the cast here
  // to make things a little easier on callers.
  OSStatus status = AuthorizationExecuteWithPrivileges(authorization,
                                                       tool_path,
                                                       options,
                                                       (char* const*)arguments,
                                                       pipe_pointer);
  if (status != errAuthorizationSuccess) {
    return status;
  }

  int line_pid = -1;
  size_t line_length = 0;
  char* line_c = fgetln(*pipe_pointer, &line_length);
  if (line_c) {
    if (line_length > 0 && line_c[line_length - 1] == '\n') {
      // line_c + line_length is the start of the next line if there is one.
      // Back up one character.
      --line_length;
    }
    std::string line(line_c, line_length);
    if (!StringToInt(line, &line_pid)) {
      // StringToInt may have set line_pid to something, but if the conversion
      // was imperfect, use -1.
      LOG(ERROR) << "ExecuteWithPrivilegesAndGetPid: funny line: " << line;
      line_pid = -1;
    }
  } else {
    LOG(ERROR) << "ExecuteWithPrivilegesAndGetPid: no line";
  }

  if (!pipe) {
    fclose(*pipe_pointer);
  }

  if (pid) {
    *pid = line_pid;
  }

  return status;
}

OSStatus ExecuteWithPrivilegesAndWait(AuthorizationRef authorization,
                                      const char* tool_path,
                                      AuthorizationFlags options,
                                      const char** arguments,
                                      FILE** pipe,
                                      int* exit_status) {
  pid_t pid;
  OSStatus status = ExecuteWithPrivilegesAndGetPID(authorization,
                                                   tool_path,
                                                   options,
                                                   arguments,
                                                   pipe,
                                                   &pid);
  if (status != errAuthorizationSuccess) {
    return status;
  }

  // exit_status may be NULL, but this function needs it.  In that case, use a
  // local version.
  int local_exit_status;
  int* exit_status_pointer;
  if (exit_status) {
    exit_status_pointer = exit_status;
  } else {
    exit_status_pointer = &local_exit_status;
  }

  if (pid != -1) {
    pid_t wait_result;
    HANDLE_EINTR(wait_result = waitpid(pid, exit_status_pointer, 0));
    if (wait_result != pid) {
      PLOG(ERROR) << "waitpid";
      *exit_status_pointer = -1;
    }
  } else {
    *exit_status_pointer = -1;
  }

  return status;
}

}  // namespace authorization_util
