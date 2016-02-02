// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_SESSION_ASSIGNMENT_SOURCE_H_
#define BLIMP_CLIENT_SESSION_ASSIGNMENT_SOURCE_H_

#include <string>

#include "base/callback.h"
#include "blimp/client/blimp_client_export.h"
#include "net/base/ip_endpoint.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blimp {
namespace client {

// An Assignment contains the configuration data needed for a client
// to connect to the engine.
struct BLIMP_CLIENT_EXPORT Assignment {
  net::IPEndPoint ip_endpoint;
  std::string client_token;
};

// AssignmentSource provides functionality to find out how a client should
// connect to an engine.
class BLIMP_CLIENT_EXPORT AssignmentSource {
 public:
  typedef const base::Callback<void(const Assignment&)> AssignmentCallback;

  // The |main_task_runner| should be the task runner for the UI thread because
  // this will in some cases be used to trigger user interaction on the UI
  // thread.
  AssignmentSource(
      const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner);
  virtual ~AssignmentSource();

  // Retrieves a valid assignment for the client and posts the result to the
  // given callback.
  void GetAssignment(const AssignmentCallback& callback);

 private:
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(AssignmentSource);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_SESSION_ASSIGNMENT_SOURCE_H_
