// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/chrome_extensions_client.h"

#include <memory>
#include <set>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/api_features.h"
#include "chrome/common/extensions/api/behavior_features.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/common/extensions/api/generated_schemas.h"
#include "chrome/common/extensions/api/manifest_features.h"
#include "chrome/common/extensions/api/permission_features.h"
#include "chrome/common/extensions/chrome_aliases.h"
#include "chrome/common/extensions/chrome_manifest_handlers.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest_handlers/theme_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/common_resources.h"
#include "components/version_info/version_info.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/api/generated_schemas.h"
#include "extensions/common/common_manifest_handlers.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/extensions_aliases.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/features/json_feature_provider_source.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/grit/extensions_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace extensions {

namespace {

// TODO(battre): Delete the HTTP URL once the blacklist is downloaded via HTTPS.
const char kExtensionBlocklistUrlPrefix[] =
    "http://www.gstatic.com/chrome/extensions/blacklist";
const char kExtensionBlocklistHttpsUrlPrefix[] =
    "https://www.gstatic.com/chrome/extensions/blacklist";

const char kThumbsWhiteListedExtension[] = "khopmbdjffemhegeeobelklnbglcdgfh";

// Mirrors version_info::Channel for histograms.
enum ChromeChannelForHistogram {
  CHANNEL_UNKNOWN,
  CHANNEL_CANARY,
  CHANNEL_DEV,
  CHANNEL_BETA,
  CHANNEL_STABLE,
  NUM_CHANNELS_FOR_HISTOGRAM
};

ChromeChannelForHistogram GetChromeChannelForHistogram(
    version_info::Channel channel) {
  switch (channel) {
    case version_info::Channel::UNKNOWN:
      return CHANNEL_UNKNOWN;
    case version_info::Channel::CANARY:
      return CHANNEL_CANARY;
    case version_info::Channel::DEV:
      return CHANNEL_DEV;
    case version_info::Channel::BETA:
      return CHANNEL_BETA;
    case version_info::Channel::STABLE:
      return CHANNEL_STABLE;
  }
  NOTREACHED() << static_cast<int>(channel);
  return CHANNEL_UNKNOWN;
}

}  // namespace

static base::LazyInstance<ChromeExtensionsClient>::Leaky g_client =
    LAZY_INSTANCE_INITIALIZER;

ChromeExtensionsClient::ChromeExtensionsClient() {}

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
  PermissionsInfo::GetInstance()->AddProvider(chrome_api_permissions_,
                                              GetChromePermissionAliases());
  PermissionsInfo::GetInstance()->AddProvider(extensions_api_permissions_,
                                              GetExtensionsPermissionAliases());

  // Set up the scripting whitelist.
  // Whitelist ChromeVox, an accessibility extension from Google that needs
  // the ability to script webui pages. This is temporary and is not
  // meant to be a general solution.
  // TODO(dmazzoni): remove this once we have an extension API that
  // allows any extension to request read-only access to webui pages.
  scripting_whitelist_.push_back(extension_misc::kChromeVoxExtensionId);

  webstore_base_url_ = GURL(extension_urls::kChromeWebstoreBaseURL);
  webstore_update_url_ = GURL(extension_urls::GetDefaultWebstoreUpdateUrl());
}

const PermissionMessageProvider&
ChromeExtensionsClient::GetPermissionMessageProvider() const {
  return permission_message_provider_;
}

const std::string ChromeExtensionsClient::GetProductName() {
  return l10n_util::GetStringUTF8(IDS_PRODUCT_NAME);
}

std::unique_ptr<FeatureProvider> ChromeExtensionsClient::CreateFeatureProvider(
    const std::string& name) const {
  std::unique_ptr<FeatureProvider> provider;
  if (name == "api") {
    provider.reset(new APIFeatureProvider());
  } else if (name == "manifest") {
    provider.reset(new ManifestFeatureProvider());
  } else if (name == "permission") {
    provider.reset(new PermissionFeatureProvider());
  } else if (name == "behavior") {
    provider.reset(new BehaviorFeatureProvider());
  } else {
    NOTREACHED();
  }
  return provider;
}

std::unique_ptr<JSONFeatureProviderSource>
ChromeExtensionsClient::CreateAPIFeatureSource() const {
  std::unique_ptr<JSONFeatureProviderSource> source(
      new JSONFeatureProviderSource("api"));
  source->LoadJSON(IDR_EXTENSION_API_FEATURES);
  source->LoadJSON(IDR_CHROME_EXTENSION_API_FEATURES);
  return source;
}

void ChromeExtensionsClient::FilterHostPermissions(
    const URLPatternSet& hosts,
    URLPatternSet* new_hosts,
    PermissionIDSet* permissions) const {
  // When editing this function, be sure to add the same functionality to
  // FilterHostPermissions() above.
  for (URLPatternSet::const_iterator i = hosts.begin(); i != hosts.end(); ++i) {
    // Filters out every URL pattern that matches chrome:// scheme.
    if (i->scheme() == content::kChromeUIScheme) {
      // chrome://favicon is the only URL for chrome:// scheme that we
      // want to support. We want to deprecate the "chrome" scheme.
      // We should not add any additional "host" here.
      if (GURL(chrome::kChromeUIFaviconURL).host() != i->host())
        continue;
      permissions->insert(APIPermission::kFavicon);
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
  if (url.DomainIs(store_url.host())) {
    if (error)
      *error = manifest_errors::kCannotScriptGallery;
    return false;
  }
  return true;
}

bool ChromeExtensionsClient::IsAPISchemaGenerated(
    const std::string& name) const {
  // Test from most common to least common.
  return api::ChromeGeneratedSchemas::IsGenerated(name) ||
         api::GeneratedSchemas::IsGenerated(name);
}

base::StringPiece ChromeExtensionsClient::GetAPISchema(
    const std::string& name) const {
  // Test from most common to least common.
  if (api::ChromeGeneratedSchemas::IsGenerated(name))
    return api::ChromeGeneratedSchemas::Get(name);

  return api::GeneratedSchemas::Get(name);
}

bool ChromeExtensionsClient::ShouldSuppressFatalErrors() const {
  // Suppress fatal everywhere until the cause of bugs like http://crbug/471599
  // are fixed. This would typically be:
  // return GetCurrentChannel() > version_info::Channel::DEV;
  return true;
}

void ChromeExtensionsClient::RecordDidSuppressFatalError() {
  UMA_HISTOGRAM_ENUMERATION("Extensions.DidSuppressJavaScriptException",
                            GetChromeChannelForHistogram(GetCurrentChannel()),
                            NUM_CHANNELS_FOR_HISTOGRAM);
}

const GURL& ChromeExtensionsClient::GetWebstoreBaseURL() const {
  // Browser tests like to alter the command line at runtime with new update
  // URLs. Just update the cached value of the base url (to avoid reparsing
  // it) if the value has changed.
  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  if (cmdline->HasSwitch(switches::kAppsGalleryURL)) {
    std::string url = cmdline->GetSwitchValueASCII(switches::kAppsGalleryURL);
    if (webstore_base_url_.possibly_invalid_spec() != url)
      webstore_base_url_ = GURL(url);
  }
  return webstore_base_url_;
}

const GURL& ChromeExtensionsClient::GetWebstoreUpdateURL() const {
  // Browser tests like to alter the command line at runtime with new update
  // URLs. Just update the cached value of the update url (to avoid reparsing
  // it) if the value has changed.
  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  if (cmdline->HasSwitch(switches::kAppsGalleryUpdateURL)) {
    std::string url =
        cmdline->GetSwitchValueASCII(switches::kAppsGalleryUpdateURL);
    if (webstore_update_url_.possibly_invalid_spec() != url)
      webstore_update_url_ = GURL(url);
  }
  return webstore_update_url_;
}

bool ChromeExtensionsClient::IsBlacklistUpdateURL(const GURL& url) const {
  // The extension blacklist URL is returned from the update service and
  // therefore not determined by Chromium. If the location of the blacklist file
  // ever changes, we need to update this function. A DCHECK in the
  // ExtensionUpdater ensures that we notice a change. This is the full URL
  // of a blacklist:
  // http://www.gstatic.com/chrome/extensions/blacklist/l_0_0_0_7.txt
  return base::StartsWith(url.spec(), kExtensionBlocklistUrlPrefix,
                          base::CompareCase::SENSITIVE) ||
         base::StartsWith(url.spec(), kExtensionBlocklistHttpsUrlPrefix,
                          base::CompareCase::SENSITIVE);
}

std::set<base::FilePath> ChromeExtensionsClient::GetBrowserImagePaths(
    const Extension* extension) {
  std::set<base::FilePath> image_paths =
      ExtensionsClient::GetBrowserImagePaths(extension);

  // Theme images
  const base::DictionaryValue* theme_images = ThemeInfo::GetImages(extension);
  if (theme_images) {
    for (base::DictionaryValue::Iterator it(*theme_images); !it.IsAtEnd();
         it.Advance()) {
      base::FilePath::StringType path;
      if (it.value().GetAsString(&path))
        image_paths.insert(base::FilePath(path));
    }
  }

  const ActionInfo* page_action = ActionInfo::GetPageActionInfo(extension);
  if (page_action && !page_action->default_icon.empty())
    page_action->default_icon.GetPaths(&image_paths);

  const ActionInfo* browser_action =
      ActionInfo::GetBrowserActionInfo(extension);
  if (browser_action && !browser_action->default_icon.empty())
    browser_action->default_icon.GetPaths(&image_paths);

  return image_paths;
}

bool ChromeExtensionsClient::ExtensionAPIEnabledInExtensionServiceWorkers()
    const {
  return GetCurrentChannel() == version_info::Channel::UNKNOWN;
}

std::string ChromeExtensionsClient::GetUserAgent() const {
  return ::GetUserAgent();
}

// static
ChromeExtensionsClient* ChromeExtensionsClient::GetInstance() {
  return g_client.Pointer();
}

}  // namespace extensions
