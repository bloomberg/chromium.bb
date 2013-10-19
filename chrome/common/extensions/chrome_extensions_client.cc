// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/chrome_extensions_client.h"

#include "chrome/common/extensions/chrome_manifest_handlers.h"
#include "chrome/common/extensions/features/base_feature_provider.h"
#include "chrome/common/url_constants.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/permissions/permission_message.h"
#include "extensions/common/url_pattern_set.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace extensions {

static base::LazyInstance<ChromeExtensionsClient> g_client =
    LAZY_INSTANCE_INITIALIZER;

ChromeExtensionsClient::ChromeExtensionsClient()
    :  chrome_api_permissions_(ChromeAPIPermissions()) {
}

ChromeExtensionsClient::~ChromeExtensionsClient() {
}

void ChromeExtensionsClient::Initialize() {
  RegisterChromeManifestHandlers();
}

const PermissionsProvider&
ChromeExtensionsClient::GetPermissionsProvider() const {
  return chrome_api_permissions_;
}

const PermissionMessageProvider&
ChromeExtensionsClient::GetPermissionMessageProvider() const {
  return permission_message_provider_;
}

FeatureProvider* ChromeExtensionsClient::GetFeatureProviderByName(
    const std::string& name) const {
  return BaseFeatureProvider::GetByName(name);
}

void ChromeExtensionsClient::FilterHostPermissions(
    const URLPatternSet& hosts,
    URLPatternSet* new_hosts,
    std::set<PermissionMessage>* messages) const {
  for (URLPatternSet::const_iterator i = hosts.begin();
       i != hosts.end(); ++i) {
    // Filters out every URL pattern that matches chrome:// scheme.
    if (i->scheme() == chrome::kChromeUIScheme) {
      // chrome://favicon is the only URL for chrome:// scheme that we
      // want to support. We want to deprecate the "chrome" scheme.
      // We should not add any additional "host" here.
      if (GURL(chrome::kChromeUIFaviconURL).host() != i->host())
        continue;
      messages->insert(PermissionMessage(
          PermissionMessage::kFavicon,
          l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_FAVICON)));
    } else {
      new_hosts->AddPattern(*i);
    }
  }
}

// static
ChromeExtensionsClient* ChromeExtensionsClient::GetInstance() {
  return g_client.Pointer();
}

}  // namespace extensions
