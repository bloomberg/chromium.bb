// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/renderer_web_database_observer.h"

#include "chrome/common/render_messages.h"
#include "webkit/api/public/WebDatabase.h"

RendererWebDatabaseObserver::RendererWebDatabaseObserver(
    IPC::Message::Sender* sender)
    : sender_(sender) {
}

void RendererWebDatabaseObserver::databaseOpened(
    const WebKit::WebDatabase& database) {
  sender_->Send(new ViewHostMsg_DatabaseOpened(
      database.securityOrigin().databaseIdentifier(), database.name(),
      database.displayName(), database.estimatedSize()));
}

void RendererWebDatabaseObserver::databaseModified(
    const WebKit::WebDatabase& database) {
  sender_->Send(new ViewHostMsg_DatabaseModified(
      database.securityOrigin().databaseIdentifier(), database.name()));
}

void RendererWebDatabaseObserver::databaseClosed(
    const WebKit::WebDatabase& database) {
  sender_->Send(new ViewHostMsg_DatabaseClosed(
      database.securityOrigin().databaseIdentifier(), database.name()));
}
