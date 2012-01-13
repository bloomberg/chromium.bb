// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

  // TODO(dpranke): Doing real work (calling webkit_glue::SetUserAgent)
  // inside what looks like a function that initializes a global is a
  // bit odd, but we need to make sure this is done before
  // webkit_glue::GetUserAgent() is called (so that the UA doesn't change).
  //
  // It would be cleaner if we could rename this to something like a
  // content::Initialize(). Maybe we can merge this into ContentMain() somehow?
  if (client) {
    bool custom = false;
    std::string ua = client->GetUserAgent(&custom);
    webkit_glue::SetUserAgent(ua, custom);
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
