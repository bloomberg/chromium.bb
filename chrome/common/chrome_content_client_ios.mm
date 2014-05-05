// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_content_client.h"

#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/url_constants.h"
#include "content/public/common/user_agent.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

// TODO(ios): Investigate merging with chrome_content_client.cc; this would
// requiring either a lot of ifdefing, or spliting the file into parts.

void ChromeContentClient::SetActiveURL(const GURL& url) {
  NOTIMPLEMENTED();
}

void ChromeContentClient::SetGpuInfo(const gpu::GPUInfo& gpu_info) {
  NOTIMPLEMENTED();
}

void ChromeContentClient::AddPepperPlugins(
    std::vector<content::PepperPluginInfo>* plugins) {
  NOTREACHED();
}

void ChromeContentClient::AddAdditionalSchemes(
    std::vector<std::string>* standard_schemes,
    std::vector<std::string>* saveable_shemes) {
  // No additional schemes for iOS.
}

std::string ChromeContentClient::GetProduct() const {
  chrome::VersionInfo version_info;
  std::string product("CriOS/");
  product += version_info.is_valid() ? version_info.Version() : "0.0.0.0";
  return product;
}

std::string ChromeContentClient::GetUserAgent() const {
  std::string product = GetProduct();
  return content::BuildUserAgentFromProduct(product);
}

base::string16 ChromeContentClient::GetLocalizedString(int message_id) const {
  return l10n_util::GetStringUTF16(message_id);
}

base::StringPiece ChromeContentClient::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) const {
  return ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedStaticMemory* ChromeContentClient::GetDataResourceBytes(
    int resource_id) const {
  return ResourceBundle::GetSharedInstance().LoadDataResourceBytes(resource_id);
}

gfx::Image& ChromeContentClient::GetNativeImageNamed(int resource_id) const {
  return ResourceBundle::GetSharedInstance().GetNativeImageNamed(resource_id);
}

std::string ChromeContentClient::GetProcessTypeNameInEnglish(int type) {
  DCHECK(false) << "Unknown child process type!";
  return "Unknown"; 
}
