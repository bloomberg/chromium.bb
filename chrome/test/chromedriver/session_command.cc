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

  Status nav_status = session->WaitForPendingNavigations();
  if (nav_status.IsError())
    return nav_status;
  Status status = command.Run(session, params, out_value);
  // Switch to main frame and retry command if subframe no longer exists.
  if (status.code() == kNoSuchFrame) {
    session->frame = "";
    nav_status = session->WaitForPendingNavigations();
    if (nav_status.IsError())
      return nav_status;
    status = command.Run(session, params, out_value);
  }
  nav_status = session->WaitForPendingNavigations();
  if (status.IsOk() && nav_status.IsError() &&
      nav_status.code() != kDisconnected)
    return nav_status;
  return status;
}
