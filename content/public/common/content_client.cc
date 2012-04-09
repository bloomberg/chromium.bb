// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/content_client.h"

#include "base/logging.h"
#include "base/string_piece.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/plugins/ppapi/host_globals.h"

namespace content {

static ContentClient* g_client;

void SetContentClient(ContentClient* client) {
  g_client = client;

  // Set the default user agent as provided by the client. We need to make
  // sure this is done before webkit_glue::GetUserAgent() is called (so that
  // the UA doesn't change).
  if (client) {
    webkit_glue::SetUserAgent(client->GetUserAgent(), false);
  }
}

ContentClient* GetContentClient() {
  return g_client;
}

const std::string& GetUserAgent(const GURL& url) {
  DCHECK(g_client);
  return webkit_glue::GetUserAgent(url);
}

webkit::ppapi::HostGlobals* GetHostGlobals() {
  return webkit::ppapi::HostGlobals::Get();
}

ContentClient::ContentClient()
    : browser_(NULL), plugin_(NULL), renderer_(NULL), utility_(NULL) {
}

ContentClient::~ContentClient() {
}

}  // namespace content
