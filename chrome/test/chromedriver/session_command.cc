// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/session_command.h"

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome.h"
#include "chrome/test/chromedriver/session.h"
#include "chrome/test/chromedriver/session_map.h"
#include "chrome/test/chromedriver/status.h"

namespace {

Status WaitForPendingNavigations(const Session& session) {
  if (!session.chrome)
    return Status(kOk);
  std::string frame = session.frame;
  if (frame == "") {
    Status status = session.chrome->GetMainFrame(&frame);
    if (status.IsError())
      return status;
  }
  return session.chrome->WaitForPendingNavigations(frame);
}

} // namespace

Status ExecuteSessionCommand(
    SessionMap* session_map,
    const SessionCommand& command,
    const base::DictionaryValue& params,
    const std::string& session_id,
    scoped_ptr<base::Value>* out_value,
    std::string* out_session_id) {
  *out_session_id = session_id;
  scoped_refptr<SessionAccessor> session_accessor;
  if (!session_map->Get(session_id, &session_accessor))
    return Status(kNoSuchSession, session_id);
  scoped_ptr<base::AutoLock> session_lock;
  Session* session = session_accessor->Access(&session_lock);
  if (!session)
    return Status(kNoSuchSession, session_id);

  Status nav_status = WaitForPendingNavigations(*session);
  if (nav_status.IsError())
    return nav_status;
  Status status = command.Run(session, params, out_value);
  nav_status = WaitForPendingNavigations(*session);
  if (status.IsOk() && nav_status.IsError() &&
      nav_status.code() != kDisconnected)
    return nav_status;
  return status;
}
