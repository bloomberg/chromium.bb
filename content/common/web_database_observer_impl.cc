// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/web_database_observer_impl.h"

#include "base/string16.h"
#include "content/common/database_messages.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDatabase.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"

WebDatabaseObserverImpl::WebDatabaseObserverImpl(
    IPC::Message::Sender* sender)
    : sender_(sender),
      open_connections_(new webkit_database::DatabaseConnectionsWrapper) {
}

WebDatabaseObserverImpl::~WebDatabaseObserverImpl() {
}

void WebDatabaseObserverImpl::databaseOpened(
    const WebKit::WebDatabase& database) {
  string16 origin_identifier = database.securityOrigin().databaseIdentifier();
  string16 database_name = database.name();
  open_connections_->AddOpenConnection(origin_identifier, database_name);
  sender_->Send(new DatabaseHostMsg_Opened(
      origin_identifier, database_name,
      database.displayName(), database.estimatedSize()));
}

void WebDatabaseObserverImpl::databaseModified(
    const WebKit::WebDatabase& database) {
  sender_->Send(new DatabaseHostMsg_Modified(
      database.securityOrigin().databaseIdentifier(), database.name()));
}

void WebDatabaseObserverImpl::databaseClosed(
    const WebKit::WebDatabase& database) {
  string16 origin_identifier = database.securityOrigin().databaseIdentifier();
  string16 database_name = database.name();
  sender_->Send(new DatabaseHostMsg_Closed(
      origin_identifier, database_name));
  open_connections_->RemoveOpenConnection(origin_identifier, database_name);
}

void WebDatabaseObserverImpl::WaitForAllDatabasesToClose() {
  open_connections_->WaitForAllDatabasesToClose();
}
