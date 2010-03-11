// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/logging.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "chrome/browser/chromeos/login/pam_google_authenticator.h"
#include "chrome/browser/chromeos/login/login_status_consumer.h"

bool PamGoogleAuthenticator::Authenticate(const std::string& username,
                                          const std::string& password) {
  base::ProcessHandle handle;
  std::vector<std::string> argv;
  // TODO(cmasone): we'll want this to be configurable.
  argv.push_back("/opt/google/chrome/session");
  argv.push_back(username);
  argv.push_back(password);

  base::environment_vector no_env;
  base::file_handle_mapping_vector no_files;
  base::LaunchApp(argv, no_env, no_files, false, &handle);
  int child_exit_code;
  bool ret = (base::WaitForExitCode(handle, &child_exit_code) &&
              child_exit_code == 0);

  if (ret)
    consumer_->OnLoginSuccess(username);
  else
    consumer_->OnLoginFailure("");
  return ret;
}
