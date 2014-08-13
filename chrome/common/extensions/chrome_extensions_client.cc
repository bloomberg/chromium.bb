// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/chrome_extensions_client.h"

#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/api/generated_schemas.h"
#include "chrome/common/extensions/chrome_manifest_handlers.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/features/chrome_channel_feature_filter.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/common_resources.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/api/generated_schemas.h"
#include "extensions/common/common_manifest_handlers.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/features/api_feature.h"
#include "extensions/common/features/base_feature_provider.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/features/json_feature_provider_source.h"
#include "extensions/common/features/manifest_feature.h"
#include "extensions/common/features/permission_feature.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/permission_message.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/grit/extensions_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/grit/extensions_api_resources.h"
#endif

namespace extensions {

namespace {

const char kThumbsWhiteListedExtension[] = "khopmbdjffemhegeeobelklnbglcdgfh";

template <class FeatureClass>
SimpleFeature* CreateFeature() {
  SimpleFeature* feature = new FeatureClass;
  feature->AddFilter(
      scoped_ptr<SimpleFeatureFilter>(new ChromeChannelFeatureFilter(feature)));
  return feature;
}

}  // namespace

static base::LazyInstance<ChromeExtensionsClient> g_client =
    LAZY_INSTANCE_INITIALIZER;

ChromeExtensionsClient::ChromeExtensionsClient()
    : chrome_api_permissions_(ChromeAPIPermissions()),
      extensions_api_permissions_(ExtensionsAPIPermissions()) {
}

ChromeExtensionsClient::~ChromeExtensionsClient() {
}

void ChromeExtensionsClient::Initialize() {
  // Registration could already be finalized in unit tests, where the utility
  // thread runs in-process.
  if (!ManifestHandler::IsRegistrationFinalized()) {
    RegisterCommonManifestHandlers();
    RegisterChromeManifestHandlers();
    ManifestHandler::FinalizeRegistration();
  }

  // Set up permissions.
  PermissionsInfo::GetInstance()->AddProvider(chrome_api_permissions_);
  PermissionsInfo::GetInstance()->AddProvider(extensions_api_permissions_);

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

const PermissionMessageProvider&
ChromeExtensionsClient::GetPermissionMessageProvider() const {
  return permission_message_provider_;
}

scoped_ptr<FeatureProvider> ChromeExtensionsClient::CreateFeatureProvider(
    const std::string& name) const {
  scoped_ptr<FeatureProvider> provider;
  scoped_ptr<JSONFeatureProviderSource> source(
      CreateFeatureProviderSource(name));
  if (name == "api") {
    provider.reset(new BaseFeatureProvider(source->dictionary(),
                                           CreateFeature<APIFeature>));
  } else if (name == "manifest") {
    provider.reset(new BaseFeatureProvider(source->dictionary(),
                                           CreateFeature<ManifestFeature>));
  } else if (name == "permission") {
    provider.reset(new BaseFeatureProvider(source->dictionary(),
                                           CreateFeature<PermissionFeature>));
  } else {
    NOTREACHED();
  }
  return provider.Pass();
}

scoped_ptr<JSONFeatureProviderSource>
ChromeExtensionsClient::CreateFeatureProviderSource(
    const std::string& name) const {
  scoped_ptr<JSONFeatureProviderSource> source(
      new JSONFeatureProviderSource(name));
  if (name == "api") {
    source->LoadJSON(IDR_EXTENSION_API_FEATURES);
    source->LoadJSON(IDR_CHROME_EXTENSION_API_FEATURES);
  } else if (name == "manifest") {
    source->LoadJSON(IDR_EXTENSION_MANIFEST_FEATURES);
    source->LoadJSON(IDR_CHROME_EXTENSION_MANIFEST_FEATURES);
  } else if (name == "permission") {
    source->LoadJSON(IDR_EXTENSION_PERMISSION_FEATURES);
    source->LoadJSON(IDR_CHROME_EXTENSION_PERMISSION_FEATURES);
  } else {
    NOTREACHED();
    source.reset();
  }
  return source.Pass();
}

void ChromeExtensionsClient::FilterHostPermissions(
    const URLPatternSet& hosts,
    URLPatternSet* new_hosts,
    std::set<PermissionMessage>* messages) const {
  for (URLPatternSet::const_iterator i = hosts.begin();
       i != hosts.end(); ++i) {
    // Filters out every URL pattern that matches chrome:// scheme.
    if (i->scheme() == content::kChromeUIScheme) {
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

URLPatternSet ChromeExtensionsClient::GetPermittedChromeSchemeHosts(
      const Extension* extension,
      const APIPermissionSet& api_permissions) const {
  URLPatternSet hosts;
  // Regular extensions are only allowed access to chrome://favicon.
  hosts.AddPattern(URLPattern(URLPattern::SCHEME_CHROMEUI,
                              chrome::kChromeUIFaviconURL));

  // Experimental extensions are also allowed chrome://thumb.
  //
  // TODO: A public API should be created for retrieving thumbnails.
  // See http://crbug.com/222856. A temporary hack is implemented here to
  // make chrome://thumbs available to NTP Russia extension as
  // non-experimental.
  if ((api_permissions.find(APIPermission::kExperimental) !=
       api_permissions.end()) ||
      (extension->id() == kThumbsWhiteListedExtension &&
       extension->from_webstore())) {
    hosts.AddPattern(URLPattern(URLPattern::SCHEME_CHROMEUI,
                                chrome::kChromeUIThumbnailURL));
  }
  return hosts;
}

bool ChromeExtensionsClient::IsScriptableURL(
    const GURL& url, std::string* error) const {
  // The gallery is special-cased as a restricted URL for scripting to prevent
  // access to special JS bindings we expose to the gallery (and avoid things
  // like extensions removing the "report abuse" link).
  // TODO(erikkay): This seems like the wrong test.  Shouldn't we we testing
  // against the store app extent?
  GURL store_url(extension_urls::GetWebstoreLaunchURL());
  if (url.host() == store_url.host()) {
    if (error)
      *error = manifest_errors::kCannotScriptGallery;
    return false;
  }
  return true;
}

bool ChromeExtensionsClient::IsAPISchemaGenerated(
    const std::string& name) const {
  // Test from most common to least common.
  return api::GeneratedSchemas::IsGenerated(name) ||
         core_api::GeneratedSchemas::IsGenerated(name);
}

base::StringPiece ChromeExtensionsClient::GetAPISchema(
    const std::string& name) const {
  // Test from most common to least common.
  if (api::GeneratedSchemas::IsGenerated(name))
    return api::GeneratedSchemas::Get(name);

  return core_api::GeneratedSchemas::Get(name);
}

void ChromeExtensionsClient::RegisterAPISchemaResources(
    ExtensionAPI* api) const {
#if defined(ENABLE_EXTENSIONS)
  api->RegisterSchemaResource("accessibilityPrivate",
                              IDR_EXTENSION_API_JSON_ACCESSIBILITYPRIVATE);
  api->RegisterSchemaResource("app", IDR_EXTENSION_API_JSON_APP);
  api->RegisterSchemaResource("browserAction",
                              IDR_EXTENSION_API_JSON_BROWSERACTION);
  api->RegisterSchemaResource("commands", IDR_EXTENSION_API_JSON_COMMANDS);
  api->RegisterSchemaResource("declarativeContent",
                              IDR_EXTENSION_API_JSON_DECLARATIVE_CONTENT);
  api->RegisterSchemaResource("declarativeWebRequest",
                              IDR_EXTENSION_API_JSON_DECLARATIVE_WEBREQUEST);
  api->RegisterSchemaResource("fileBrowserHandler",
                              IDR_EXTENSION_API_JSON_FILEBROWSERHANDLER);
  api->RegisterSchemaResource("inputMethodPrivate",
                              IDR_EXTENSION_API_JSON_INPUTMETHODPRIVATE);
  api->RegisterSchemaResource("pageAction", IDR_EXTENSION_API_JSON_PAGEACTION);
  api->RegisterSchemaResource("pageActions",
                              IDR_EXTENSION_API_JSON_PAGEACTIONS);
  api->RegisterSchemaResource("privacy", IDR_EXTENSION_API_JSON_PRIVACY);
  api->RegisterSchemaResource("processes", IDR_EXTENSION_API_JSON_PROCESSES);
  api->RegisterSchemaResource("proxy", IDR_EXTENSION_API_JSON_PROXY);
  api->RegisterSchemaResource("scriptBadge",
                              IDR_EXTENSION_API_JSON_SCRIPTBADGE);
  api->RegisterSchemaResource("ttsEngine", IDR_EXTENSION_API_JSON_TTSENGINE);
  api->RegisterSchemaResource("tts", IDR_EXTENSION_API_JSON_TTS);
  api->RegisterSchemaResource("types", IDR_EXTENSION_API_JSON_TYPES);
  api->RegisterSchemaResource("types.private",
                              IDR_EXTENSION_API_JSON_TYPES_PRIVATE);
  api->RegisterSchemaResource("webstore", IDR_EXTENSION_API_JSON_WEBSTORE);
  api->RegisterSchemaResource("webViewRequest",
                              IDR_EXTENSION_API_JSON_WEB_VIEW_REQUEST);
#endif  // defined(ENABLE_EXTENSIONS)
}

bool ChromeExtensionsClient::ShouldSuppressFatalErrors() const {
  // Suppress fatal errors only on beta and stable channels.
  return GetCurrentChannel() > chrome::VersionInfo::CHANNEL_DEV;
}

// static
ChromeExtensionsClient* ChromeExtensionsClient::GetInstance() {
  return g_client.Pointer();
}

}  // namespace extensions
