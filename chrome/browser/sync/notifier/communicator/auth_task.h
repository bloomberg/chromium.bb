// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_COMMUNICATOR_AUTH_TASK_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_COMMUNICATOR_AUTH_TASK_H_

#include <string>
#include "talk/base/scoped_ptr.h"
#include "talk/base/sigslot.h"
#include "talk/base/task.h"

namespace buzz {
class GaiaAuth;
}

namespace notifier {

class Login;

// Create an authenticated talk url from an unauthenticated url.
class AuthTask : public talk_base::Task, public sigslot::has_slots<> {
 public:
  AuthTask(talk_base::Task* parent, Login* login, const char* url);

  // An abort method which doesn't take any parameters.
  // (talk_base::Task::Abort() has a default parameter).
  //
  // The primary purpose of this method is to allow a signal to be hooked up to
  // abort this task.
  void Abort() {
    talk_base::Task::Abort();
  }

  void set_service(const char* service) {
    service_ = service;
  }

  void set_use_gaia_redirect(bool use_gaia_redirect) {
    use_gaia_redirect_ = use_gaia_redirect;
  }

  void set_redir_auth_prefix(const char* redir_auth_prefix) {
    redir_auth_prefix_ = redir_auth_prefix;
  }

  void set_redir_continue(const char* redir_continue) {
    redir_continue_ = redir_continue;
  }

  sigslot::signal1<const std::string&> SignalAuthDone;
  sigslot::signal1<bool> SignalAuthError;

 protected:
  virtual int ProcessStart();
  virtual int ProcessResponse();

 private:
  void OnAuthDone();

  scoped_ptr<buzz::GaiaAuth> auth_;
  Login* login_;
  std::string service_;
  std::string url_;

  // The following members are used for cases where we don't want to redirect
  // through gaia, but rather via the end-site's mechanism.
  bool use_gaia_redirect_;
  std::string redir_auth_prefix_;
  std::string redir_continue_;

  DISALLOW_COPY_AND_ASSIGN(AuthTask);
};

}  // namespace notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_COMMUNICATOR_AUTH_TASK_H_
