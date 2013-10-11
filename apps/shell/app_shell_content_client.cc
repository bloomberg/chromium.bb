// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/app_shell_content_client.h"

#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/common/user_agent/user_agent_util.h"

namespace apps {

AppShellContentClient::AppShellContentClient() {}

AppShellContentClient::~AppShellContentClient() {}

std::string AppShellContentClient::GetUserAgent() const {
  // TODO(derat): Figure out what this should be for app_shell and determine
  // whether we need to include a version number to placate browser sniffing.
  return webkit_glue::BuildUserAgentFromProduct("Chrome");
}

string16 AppShellContentClient::GetLocalizedString(int message_id) const {
  return l10n_util::GetStringUTF16(message_id);
}

base::StringPiece AppShellContentClient::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) const {
  return ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedStaticMemory* AppShellContentClient::GetDataResourceBytes(
    int resource_id) const {
  return ResourceBundle::GetSharedInstance().LoadDataResourceBytes(resource_id);
}

gfx::Image& AppShellContentClient::GetNativeImageNamed(int resource_id) const {
  return ResourceBundle::GetSharedInstance().GetNativeImageNamed(resource_id);
}

}  // namespace apps
