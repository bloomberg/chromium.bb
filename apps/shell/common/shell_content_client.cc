// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/common/shell_content_client.h"

#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/common/user_agent.h"
#include "extensions/common/constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace apps {

ShellContentClient::ShellContentClient() {}

ShellContentClient::~ShellContentClient() {}

void ShellContentClient::AddAdditionalSchemes(
    std::vector<std::string>* standard_schemes,
    std::vector<std::string>* savable_schemes) {
  standard_schemes->push_back(extensions::kExtensionScheme);
  savable_schemes->push_back(extensions::kExtensionScheme);
  standard_schemes->push_back(extensions::kExtensionResourceScheme);
  savable_schemes->push_back(extensions::kExtensionResourceScheme);
}

std::string ShellContentClient::GetUserAgent() const {
  // TODO(derat): Figure out what this should be for app_shell and determine
  // whether we need to include a version number to placate browser sniffing.
  return content::BuildUserAgentFromProduct("Chrome");
}

base::string16 ShellContentClient::GetLocalizedString(int message_id) const {
  return l10n_util::GetStringUTF16(message_id);
}

base::StringPiece ShellContentClient::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) const {
  return ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedStaticMemory* ShellContentClient::GetDataResourceBytes(
    int resource_id) const {
  return ResourceBundle::GetSharedInstance().LoadDataResourceBytes(resource_id);
}

gfx::Image& ShellContentClient::GetNativeImageNamed(int resource_id) const {
  return ResourceBundle::GetSharedInstance().GetNativeImageNamed(resource_id);
}

}  // namespace apps
