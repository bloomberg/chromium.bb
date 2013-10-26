// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/chrome_extensions_client.h"

#include "chrome/common/extensions/chrome_manifest_handlers.h"
#include "chrome/common/extensions/extension_constants.h"
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

  // Set up the scripting whitelist.
  // Whitelist ChromeVox, an accessibility extension from Google that needs
  // the ability to script webui pages. This is temporary and is not
  // meant to be a general solution.
  // TODO(dmazzoni): remove this once we have an extension API that
  // allows any extension to request read-only access to webui pages.
  scripting_whitelist_.push_back(extension_misc::kChromeVoxExtensionId);

  // Whitelist "Discover DevTools Companion" extension from Google that
  // needs the ability to script DevTools pages. Companion will assist
  // online courses and will be needed while the online educational programs
  // are in place.
  scripting_whitelist_.push_back("angkfkebojeancgemegoedelbnjgcgme");
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

void ChromeExtensionsClient::SetScriptingWhitelist(
    const ExtensionsClient::ScriptingWhitelist& whitelist) {
  scripting_whitelist_ = whitelist;
}

const ExtensionsClient::ScriptingWhitelist&
ChromeExtensionsClient::GetScriptingWhitelist() const {
  return scripting_whitelist_;
}

// static
ChromeExtensionsClient* ChromeExtensionsClient::GetInstance() {
  return g_client.Pointer();
}

}  // namespace extensions
