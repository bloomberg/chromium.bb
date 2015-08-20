// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/chrome_extensions_client.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/common/extensions/api/generated_schemas.h"
#include "chrome/common/extensions/chrome_manifest_handlers.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/features/chrome_channel_feature_filter.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "chrome/common/extensions/manifest_handlers/theme_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/common_resources.h"
#include "chrome/grit/extensions_api_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/version_info/version_info.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/api/generated_schemas.h"
#include "extensions/common/common_manifest_handlers.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/features/api_feature.h"
#include "extensions/common/features/base_feature_provider.h"
#include "extensions/common/features/behavior_feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/features/json_feature_provider_source.h"
#include "extensions/common/features/manifest_feature.h"
#include "extensions/common/features/permission_feature.h"
#include "extensions/common/features/simple_feature.h"
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

template <class FeatureClass>
SimpleFeature* CreateFeature() {
  SimpleFeature* feature = new FeatureClass;
  feature->AddFilter(
      scoped_ptr<SimpleFeatureFilter>(new ChromeChannelFeatureFilter(feature)));
  return feature;
}

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
}

const PermissionMessageProvider&
ChromeExtensionsClient::GetPermissionMessageProvider() const {
  return permission_message_provider_;
}

const std::string ChromeExtensionsClient::GetProductName() {
  return l10n_util::GetStringUTF8(IDS_PRODUCT_NAME);
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
  } else if (name == "behavior") {
    provider.reset(new BaseFeatureProvider(source->dictionary(),
                                           CreateFeature<BehaviorFeature>));
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
  } else if (name == "behavior") {
    source->LoadJSON(IDR_EXTENSION_BEHAVIOR_FEATURES);
  } else {
    NOTREACHED();
    source.reset();
  }
  return source.Pass();
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

void ChromeExtensionsClient::RegisterAPISchemaResources(
    ExtensionAPI* api) const {
  api->RegisterSchemaResource("accessibilityPrivate",
                              IDR_EXTENSION_API_JSON_ACCESSIBILITYPRIVATE);
  api->RegisterSchemaResource("app", IDR_EXTENSION_API_JSON_APP);
  api->RegisterSchemaResource("browserAction",
                              IDR_EXTENSION_API_JSON_BROWSERACTION);
  api->RegisterSchemaResource("commands", IDR_EXTENSION_API_JSON_COMMANDS);
  api->RegisterSchemaResource("declarativeContent",
                              IDR_EXTENSION_API_JSON_DECLARATIVE_CONTENT);
  api->RegisterSchemaResource("fileBrowserHandler",
                              IDR_EXTENSION_API_JSON_FILEBROWSERHANDLER);
  api->RegisterSchemaResource("inputMethodPrivate",
                              IDR_EXTENSION_API_JSON_INPUTMETHODPRIVATE);
  api->RegisterSchemaResource("pageAction", IDR_EXTENSION_API_JSON_PAGEACTION);
  api->RegisterSchemaResource("privacy", IDR_EXTENSION_API_JSON_PRIVACY);
  api->RegisterSchemaResource("processes", IDR_EXTENSION_API_JSON_PROCESSES);
  api->RegisterSchemaResource("proxy", IDR_EXTENSION_API_JSON_PROXY);
  api->RegisterSchemaResource("ttsEngine", IDR_EXTENSION_API_JSON_TTSENGINE);
  api->RegisterSchemaResource("tts", IDR_EXTENSION_API_JSON_TTS);
  api->RegisterSchemaResource("types", IDR_EXTENSION_API_JSON_TYPES);
  api->RegisterSchemaResource("types.private",
                              IDR_EXTENSION_API_JSON_TYPES_PRIVATE);
  api->RegisterSchemaResource("webstore", IDR_EXTENSION_API_JSON_WEBSTORE);
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

std::string ChromeExtensionsClient::GetWebstoreBaseURL() const {
  std::string gallery_prefix = extension_urls::kChromeWebstoreBaseURL;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAppsGalleryURL))
    gallery_prefix =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kAppsGalleryURL);
  if (base::EndsWith(gallery_prefix, "/", base::CompareCase::SENSITIVE))
    gallery_prefix = gallery_prefix.substr(0, gallery_prefix.length() - 1);
  return gallery_prefix;
}

std::string ChromeExtensionsClient::GetWebstoreUpdateURL() const {
  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  if (cmdline->HasSwitch(switches::kAppsGalleryUpdateURL))
    return cmdline->GetSwitchValueASCII(switches::kAppsGalleryUpdateURL);
  else
    return extension_urls::GetDefaultWebstoreUpdateUrl().spec();
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
  const base::DictionaryValue* theme_images =
      extensions::ThemeInfo::GetImages(extension);
  if (theme_images) {
    for (base::DictionaryValue::Iterator it(*theme_images); !it.IsAtEnd();
         it.Advance()) {
      base::FilePath::StringType path;
      if (it.value().GetAsString(&path))
        image_paths.insert(base::FilePath(path));
    }
  }

  const extensions::ActionInfo* page_action =
      extensions::ActionInfo::GetPageActionInfo(extension);
  if (page_action && !page_action->default_icon.empty())
    page_action->default_icon.GetPaths(&image_paths);

  const extensions::ActionInfo* browser_action =
      extensions::ActionInfo::GetBrowserActionInfo(extension);
  if (browser_action && !browser_action->default_icon.empty())
    browser_action->default_icon.GetPaths(&image_paths);

  return image_paths;
}

// static
ChromeExtensionsClient* ChromeExtensionsClient::GetInstance() {
  return g_client.Pointer();
}

}  // namespace extensions
