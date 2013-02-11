// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/session_commands.h"

#include <list>

#include "base/callback.h"
#include "base/logging.h"  // For CHECK macros.
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome.h"
#include "chrome/test/chromedriver/session.h"
#include "chrome/test/chromedriver/session_map.h"
#include "chrome/test/chromedriver/status.h"
#include "chrome/test/chromedriver/web_view.h"

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

  return command.Run(session, params, out_value);
}

Status ExecuteQuit(
    SessionMap* session_map,
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  CHECK(session_map->Remove(session->id));
  return session->chrome->Quit();
}

Status ExecuteGetCurrentWindowHandle(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  if (session->window.empty())
    return Status(kNoSuchWindow);
  value->reset(new StringValue(session->window));
  return Status(kOk);
}

Status ExecuteGetWindowHandles(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  std::list<WebView*> web_views;
  Status status = session->chrome->GetWebViews(&web_views);
  if (status.IsError())
    return status;
  base::ListValue window_ids;
  for (std::list<WebView*>::const_iterator it = web_views.begin();
       it != web_views.end(); ++it) {
    window_ids.AppendString((*it)->GetId());
  }
  value->reset(window_ids.DeepCopy());
  return Status(kOk);
}

Status ExecuteSetTimeout(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  int ms;
  if (!params.GetInteger("ms", &ms) || ms < 0)
    return Status(kUnknownError, "'ms' must be a non-negative integer");
  std::string type;
  if (!params.GetString("type", &type))
    return Status(kUnknownError, "'type' must be a string");
  if (type == "implicit")
    session->implicit_wait = ms;
  else if (type == "script")
    session->script_timeout = ms;
  else if (type == "page load")
    session->page_load_timeout = ms;
  else
    return Status(kUnknownError, "unknown type of timeout:" + type);
  return Status(kOk);
}
