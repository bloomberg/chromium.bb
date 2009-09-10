// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_COMMUNICATOR_TALK_AUTH_TASK_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_COMMUNICATOR_TALK_AUTH_TASK_H_

#include "talk/base/scoped_ptr.h"
#include "talk/base/sigslot.h"
#include "talk/base/task.h"

namespace buzz {
class GaiaAuth;
}

namespace notifier {
class Login;

// Create an authenticated talk url from an unauthenticated url
class TalkAuthTask : public talk_base::Task, public sigslot::has_slots<> {
 public:
  TalkAuthTask(talk_base::Task* parent,
               Login* login,
               const char* url);

  // An abort method which doesn't take any parameters.
  // (talk_base::Task::Abort() has a default parameter.)
  //
  // The primary purpose of this method is to allow a
  // signal to be hooked up to abort this task.
  void Abort() {
    talk_base::Task::Abort();
  }

  const std::string& url() {
    return url_;
  }

  std::string GetAuthenticatedUrl(const char* talk_base_url) const;
  std::string GetSID() const;

  sigslot::signal1<const TalkAuthTask&> SignalAuthDone;

  bool HadError() const;

  // TODO(sync): add captcha support

 protected:
  virtual int ProcessStart();
  virtual int ProcessResponse();

 private:
  void OnAuthDone();

  scoped_ptr<buzz::GaiaAuth> auth_;
  Login* login_;
  std::string url_;
  DISALLOW_COPY_AND_ASSIGN(TalkAuthTask);
};
}  // namespace notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_COMMUNICATOR_TALK_AUTH_TASK_H_
