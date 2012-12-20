// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/content_client.h"

#include "base/logging.h"
#include "base/string_piece.h"
#include "ui/gfx/image/image.h"
#include "webkit/user_agent/user_agent.h"

#if defined(ENABLE_PLUGINS)
#include "webkit/plugins/ppapi/host_globals.h"
#endif

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
#if defined(ENABLE_PLUGINS)
  return webkit::ppapi::HostGlobals::Get();
#else
  return NULL;
#endif
}

ContentClient::ContentClient()
    : browser_(NULL), plugin_(NULL), renderer_(NULL), utility_(NULL) {
}

ContentClient::~ContentClient() {
}

bool ContentClient::HasWebUIScheme(const GURL& url) const {
  return false;
}

bool ContentClient::CanHandleWhileSwappedOut(const IPC::Message& message) {
  return false;
}

std::string ContentClient::GetProduct() const {
  return std::string();
}

std::string ContentClient::GetUserAgent() const {
  return std::string();
}

string16 ContentClient::GetLocalizedString(int message_id) const {
  return string16();
}

base::StringPiece ContentClient::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) const {
  return base::StringPiece();
}

gfx::Image& ContentClient::GetNativeImageNamed(int resource_id) const {
  CR_DEFINE_STATIC_LOCAL(gfx::Image, kEmptyImage, ());
  return kEmptyImage;
}

#if defined(OS_MACOSX) && !defined(OS_IOS)
bool ContentClient::GetSandboxProfileForSandboxType(
    int sandbox_type,
    int* sandbox_profile_resource_id) const {
  return false;
}

std::string ContentClient::GetCarbonInterposePath() const {
  return std::string();
}
#endif

}  // namespace content
