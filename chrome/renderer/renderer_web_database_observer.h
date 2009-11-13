// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_RENDERER_RENDERER_WEB_DATABASE_OBSERVER_H_
#define CHROME_RENDERER_RENDERER_WEB_DATABASE_OBSERVER_H_

#include "ipc/ipc_message.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDatabaseObserver.h"

class RendererWebDatabaseObserver : public WebKit::WebDatabaseObserver {
 public:
  explicit RendererWebDatabaseObserver(IPC::Message::Sender* sender);
  virtual void databaseOpened(const WebKit::WebDatabase& database);
  virtual void databaseModified(const WebKit::WebDatabase& database);
  virtual void databaseClosed(const WebKit::WebDatabase& database);

 private:
  IPC::Message::Sender* sender_;
};

#endif  // CHROME_RENDERER_RENDERER_WEB_DATABASE_OBSERVER_H_
