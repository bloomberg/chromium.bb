// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/web_database_observer_impl.h"

#include "base/auto_reset.h"
#include "base/message_loop.h"
#include "base/string16.h"
#include "chrome/common/render_messages.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDatabase.h"

WebDatabaseObserverImpl::WebDatabaseObserverImpl(
    IPC::Message::Sender* sender)
    : sender_(sender),
      waiting_for_dbs_to_close_(false) {
}

void WebDatabaseObserverImpl::databaseOpened(
    const WebKit::WebDatabase& database) {
  string16 origin_identifier = database.securityOrigin().databaseIdentifier();
  string16 database_name = database.name();
  sender_->Send(new ViewHostMsg_DatabaseOpened(
      origin_identifier, database_name,
      database.displayName(), database.estimatedSize()));
  database_connections_.AddConnection(origin_identifier, database_name);
}

void WebDatabaseObserverImpl::databaseModified(
    const WebKit::WebDatabase& database) {
  sender_->Send(new ViewHostMsg_DatabaseModified(
      database.securityOrigin().databaseIdentifier(), database.name()));
}

void WebDatabaseObserverImpl::databaseClosed(
    const WebKit::WebDatabase& database) {
  string16 origin_identifier = database.securityOrigin().databaseIdentifier();
  string16 database_name = database.name();
  sender_->Send(new ViewHostMsg_DatabaseClosed(
      origin_identifier, database_name));
  database_connections_.RemoveConnection(origin_identifier, database_name);
  if (waiting_for_dbs_to_close_ && database_connections_.IsEmpty())
    MessageLoop::current()->Quit();
}

void WebDatabaseObserverImpl::WaitForAllDatabasesToClose() {
  if (!database_connections_.IsEmpty()) {
    AutoReset<bool> waiting_for_dbs_auto_reset(&waiting_for_dbs_to_close_, true);
    MessageLoop::ScopedNestableTaskAllower nestable(MessageLoop::current());
    MessageLoop::current()->Run();
  }
}
