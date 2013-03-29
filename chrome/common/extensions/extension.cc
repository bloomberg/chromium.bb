// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension.h"

#include "base/base64.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/stl_util.h"
#include "base/string16.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
// TODO(rdevlin.cronin): Remove these once all references have been removed as
// part of crbug.com/159265.
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/common/extensions/api/extension_action/page_action_handler.h"
#include "chrome/common/extensions/api/icons/icons_handler.h"
#include "chrome/common/extensions/api/plugins/plugins_handler.h"
#include "chrome/common/extensions/api/themes/theme_handler.h"
#include "chrome/common/extensions/background_info.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/feature_switch.h"
#include "chrome/common/extensions/features/base_feature_provider.h"
#include "chrome/common/extensions/features/feature.h"
#include "chrome/common/extensions/incognito_handler.h"
#include "chrome/common/extensions/manifest.h"
#include "chrome/common/extensions/manifest_handler.h"
#include "chrome/common/extensions/manifest_handler_helpers.h"
#include "chrome/common/extensions/manifest_handlers/kiosk_enabled_info.h"
#include "chrome/common/extensions/manifest_handlers/offline_enabled_info.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "chrome/common/extensions/permissions/api_permission_set.h"
#include "chrome/common/extensions/permissions/permission_set.h"
#include "chrome/common/extensions/permissions/permissions_info.h"
#include "chrome/common/extensions/user_script.h"
#include "chrome/common/url_constants.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/id_util.h"
#include "extensions/common/url_pattern_set.h"
#include "googleurl/src/url_util.h"
#include "grit/chromium_strings.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN)
#include "grit/generated_resources.h"
#endif

namespace keys = extension_manifest_keys;
namespace values = extension_manifest_values;
namespace errors = extension_manifest_errors;
namespace info_keys = extension_info_keys;

namespace extensions {

namespace {

const int kModernManifestVersion = 2;
const int kPEMOutputColumns = 65;

// KEY MARKERS
const char kKeyBeginHeaderMarker[] = "-----BEGIN";
const char kKeyBeginFooterMarker[] = "-----END";
const char kKeyInfoEndMarker[] = "KEY-----";
const char kPublic[] = "PUBLIC";
const char kPrivate[] = "PRIVATE";

const int kRSAKeySize = 1024;

const char kThumbsWhiteListedExtension[] = "khopmbdjffemhegeeobelklnbglcdgfh";

// A singleton object containing global data needed by the extension objects.
class ExtensionConfig {
 public:
  static ExtensionConfig* GetInstance() {
    return Singleton<ExtensionConfig>::get();
  }

  Extension::ScriptingWhitelist* whitelist() { return &scripting_whitelist_; }

 private:
  friend struct DefaultSingletonTraits<ExtensionConfig>;

  ExtensionConfig() {
    // Whitelist ChromeVox, an accessibility extension from Google that needs
    // the ability to script webui pages. This is temporary and is not
    // meant to be a general solution.
    // TODO(dmazzoni): remove this once we have an extension API that
    // allows any extension to request read-only access to webui pages.
    scripting_whitelist_.push_back("kgejglhpjiefppelpmljglcjbhoiplfn");

    // Whitelist "Discover DevTools Companion" extension from Google that
    // needs the ability to script DevTools pages. Companion will assist
    // online courses and will be needed while the online educational programs
    // are in place.
    scripting_whitelist_.push_back("angkfkebojeancgemegoedelbnjgcgme");
  }
  ~ExtensionConfig() { }

  // A whitelist of extensions that can script anywhere. Do not add to this
  // list (except in tests) without consulting the Extensions team first.
  // Note: Component extensions have this right implicitly and do not need to be
  // added to this list.
  Extension::ScriptingWhitelist scripting_whitelist_;
};

bool ReadLaunchDimension(const extensions::Manifest* manifest,
                         const char* key,
                         int* target,
                         bool is_valid_container,
                         string16* error) {
  const Value* temp = NULL;
  if (manifest->Get(key, &temp)) {
    if (!is_valid_container) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidLaunchValueContainer,
          key);
      return false;
    }
    if (!temp->GetAsInteger(target) || *target < 0) {
      *target = 0;
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidLaunchValue,
          key);
      return false;
    }
  }
  return true;
}

bool ContainsManifestForbiddenPermission(const APIPermissionSet& apis,
                                         string16* error) {
  CHECK(error);
  for (APIPermissionSet::const_iterator i = apis.begin();
      i != apis.end(); ++i) {
    if ((*i)->ManifestEntryForbidden()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kPermissionNotAllowedInManifest,
          (*i)->info()->name());
      return true;
    }
  }
  return false;
}

}  // namespace

#if defined(OS_WIN)
const char Extension::kExtensionRegistryPath[] =
    "Software\\Google\\Chrome\\Extensions";
#endif

const char Extension::kMimeType[] = "application/x-chrome-extension";

const int Extension::kValidWebExtentSchemes =
    URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS;

const int Extension::kValidHostPermissionSchemes = URLPattern::SCHEME_CHROMEUI |
                                                   URLPattern::SCHEME_HTTP |
                                                   URLPattern::SCHEME_HTTPS |
                                                   URLPattern::SCHEME_FILE |
                                                   URLPattern::SCHEME_FTP;

//
// Extension
//

// static
scoped_refptr<Extension> Extension::Create(const base::FilePath& path,
                                           Manifest::Location location,
                                           const DictionaryValue& value,
                                           int flags,
                                           std::string* utf8_error) {
  return Extension::Create(path,
                           location,
                           value,
                           flags,
                           std::string(),  // ID is ignored if empty.
                           utf8_error);
}

// TODO(sungguk): Continue removing std::string errors and replacing
// with string16. See http://crbug.com/71980.
scoped_refptr<Extension> Extension::Create(const base::FilePath& path,
                                           Manifest::Location location,
                                           const DictionaryValue& value,
                                           int flags,
                                           const std::string& explicit_id,
                                           std::string* utf8_error) {
  DCHECK(utf8_error);
  string16 error;
  scoped_ptr<extensions::Manifest> manifest(
      new extensions::Manifest(location,
                               scoped_ptr<DictionaryValue>(value.DeepCopy())));

  if (!InitExtensionID(manifest.get(), path, explicit_id, flags, &error)) {
    *utf8_error = UTF16ToUTF8(error);
    return NULL;
  }

  std::vector<InstallWarning> install_warnings;
  if (!manifest->ValidateManifest(utf8_error, &install_warnings)) {
    return NULL;
  }

  scoped_refptr<Extension> extension = new Extension(path, manifest.Pass());
  extension->install_warnings_.swap(install_warnings);

  if (!extension->InitFromValue(flags, &error)) {
    *utf8_error = UTF16ToUTF8(error);
    return NULL;
  }

  if (!extension->CheckPlatformAppFeatures(&error) ||
      !extension->CheckConflictingFeatures(&error)) {
    *utf8_error = UTF16ToUTF8(error);
    return NULL;
  }

  return extension;
}

// static
bool Extension::IdIsValid(const std::string& id) {
  // Verify that the id is legal.
  if (id.size() != (id_util::kIdSize * 2))
    return false;

  // We only support lowercase IDs, because IDs can be used as URL components
  // (where GURL will lowercase it).
  std::string temp = StringToLowerASCII(id);
  for (size_t i = 0; i < temp.size(); i++)
    if (temp[i] < 'a' || temp[i] > 'p')
      return false;

  return true;
}

// static
bool Extension::IsExtension(const base::FilePath& file_name) {
  return file_name.MatchesExtension(chrome::kExtensionFileExtension);
}

void Extension::GetBasicInfo(bool enabled,
                             DictionaryValue* info) const {
  info->SetString(info_keys::kIdKey, id());
  info->SetString(info_keys::kNameKey, name());
  info->SetBoolean(info_keys::kEnabledKey, enabled);
  info->SetBoolean(info_keys::kKioskEnabledKey,
                   KioskEnabledInfo::IsKioskEnabled(this));
  info->SetBoolean(info_keys::kOfflineEnabledKey,
                   OfflineEnabledInfo::IsOfflineEnabled(this));
  info->SetString(info_keys::kVersionKey, VersionString());
  info->SetString(info_keys::kDescriptionKey, description());
  info->SetString(info_keys::kOptionsUrlKey,
                  ManifestURL::GetOptionsPage(this).possibly_invalid_spec());
  info->SetString(info_keys::kHomepageUrlKey,
                  ManifestURL::GetHomepageURL(this).possibly_invalid_spec());
  info->SetString(info_keys::kDetailsUrlKey,
                  ManifestURL::GetDetailsURL(this).possibly_invalid_spec());
  info->SetBoolean(info_keys::kPackagedAppKey, is_platform_app());
}

Manifest::Type Extension::GetType() const {
  return converted_from_user_script() ?
      Manifest::TYPE_USER_SCRIPT : manifest_->type();
}

// static
GURL Extension::GetResourceURL(const GURL& extension_url,
                               const std::string& relative_path) {
  DCHECK(extension_url.SchemeIs(extensions::kExtensionScheme));
  DCHECK_EQ("/", extension_url.path());

  std::string path = relative_path;

  // If the relative path starts with "/", it is "absolute" relative to the
  // extension base directory, but extension_url is already specified to refer
  // to that base directory, so strip the leading "/" if present.
  if (relative_path.size() > 0 && relative_path[0] == '/')
    path = relative_path.substr(1);

  GURL ret_val = GURL(extension_url.spec() + path);
  DCHECK(StartsWithASCII(ret_val.spec(), extension_url.spec(), false));

  return ret_val;
}

bool Extension::ResourceMatches(const URLPatternSet& pattern_set,
                                const std::string& resource) const {
  return pattern_set.MatchesURL(extension_url_.Resolve(resource));
}

ExtensionResource Extension::GetResource(
    const std::string& relative_path) const {
  std::string new_path = relative_path;
  // We have some legacy data where resources have leading slashes.
  // See: http://crbug.com/121164
  if (!new_path.empty() && new_path.at(0) == '/')
    new_path.erase(0, 1);
#if defined(OS_POSIX)
  base::FilePath relative_file_path(new_path);
#elif defined(OS_WIN)
  base::FilePath relative_file_path(UTF8ToWide(new_path));
#endif
  ExtensionResource r(id(), path(), relative_file_path);
  if ((creation_flags() & Extension::FOLLOW_SYMLINKS_ANYWHERE)) {
    r.set_follow_symlinks_anywhere();
  }
  return r;
}

ExtensionResource Extension::GetResource(
    const base::FilePath& relative_file_path) const {
  ExtensionResource r(id(), path(), relative_file_path);
  if ((creation_flags() & Extension::FOLLOW_SYMLINKS_ANYWHERE)) {
    r.set_follow_symlinks_anywhere();
  }
  return r;
}

// TODO(rafaelw): Move ParsePEMKeyBytes, ProducePEM & FormatPEMForOutput to a
// util class in base:
// http://code.google.com/p/chromium/issues/detail?id=13572
// static
bool Extension::ParsePEMKeyBytes(const std::string& input,
                                 std::string* output) {
  DCHECK(output);
  if (!output)
    return false;
  if (input.length() == 0)
    return false;

  std::string working = input;
  if (StartsWithASCII(working, kKeyBeginHeaderMarker, true)) {
    working = CollapseWhitespaceASCII(working, true);
    size_t header_pos = working.find(kKeyInfoEndMarker,
      sizeof(kKeyBeginHeaderMarker) - 1);
    if (header_pos == std::string::npos)
      return false;
    size_t start_pos = header_pos + sizeof(kKeyInfoEndMarker) - 1;
    size_t end_pos = working.rfind(kKeyBeginFooterMarker);
    if (end_pos == std::string::npos)
      return false;
    if (start_pos >= end_pos)
      return false;

    working = working.substr(start_pos, end_pos - start_pos);
    if (working.length() == 0)
      return false;
  }

  return base::Base64Decode(working, output);
}

// static
bool Extension::ProducePEM(const std::string& input, std::string* output) {
  DCHECK(output);
  return (input.length() == 0) ? false : base::Base64Encode(input, output);
}

// static
bool Extension::FormatPEMForFileOutput(const std::string& input,
                                       std::string* output,
                                       bool is_public) {
  DCHECK(output);
  if (input.length() == 0)
    return false;
  *output = "";
  output->append(kKeyBeginHeaderMarker);
  output->append(" ");
  output->append(is_public ? kPublic : kPrivate);
  output->append(" ");
  output->append(kKeyInfoEndMarker);
  output->append("\n");
  for (size_t i = 0; i < input.length(); ) {
    int slice = std::min<int>(input.length() - i, kPEMOutputColumns);
    output->append(input.substr(i, slice));
    output->append("\n");
    i += slice;
  }
  output->append(kKeyBeginFooterMarker);
  output->append(" ");
  output->append(is_public ? kPublic : kPrivate);
  output->append(" ");
  output->append(kKeyInfoEndMarker);
  output->append("\n");

  return true;
}

// static
GURL Extension::GetBaseURLFromExtensionId(const std::string& extension_id) {
  return GURL(std::string(extensions::kExtensionScheme) +
              content::kStandardSchemeSeparator + extension_id + "/");
}

// static
void Extension::SetScriptingWhitelist(
    const Extension::ScriptingWhitelist& whitelist) {
  ScriptingWhitelist* current_whitelist =
      ExtensionConfig::GetInstance()->whitelist();
  current_whitelist->clear();
  for (ScriptingWhitelist::const_iterator it = whitelist.begin();
       it != whitelist.end(); ++it) {
    current_whitelist->push_back(*it);
  }
}

// static
const Extension::ScriptingWhitelist* Extension::GetScriptingWhitelist() {
  return ExtensionConfig::GetInstance()->whitelist();
}

bool Extension::ParsePermissions(const char* key,
                                 string16* error,
                                 APIPermissionSet* api_permissions,
                                 URLPatternSet* host_permissions) {
  if (manifest_->HasKey(key)) {
    const ListValue* permissions = NULL;
    if (!manifest_->GetList(key, &permissions)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidPermissions, "");
      return false;
    }

    // NOTE: We need to get the APIPermission before we check if features
    // associated with them are available because the feature system does not
    // know about aliases.

    std::vector<std::string> host_data;
    if (!APIPermissionSet::ParseFromJSON(permissions, api_permissions,
                                         error, &host_data))
      return false;

    // Verify feature availability of permissions.
    std::vector<APIPermission::ID> to_remove;
    FeatureProvider* permission_features =
        BaseFeatureProvider::GetPermissionFeatures();
    for (APIPermissionSet::const_iterator it = api_permissions->begin();
         it != api_permissions->end(); ++it) {
      extensions::Feature* feature =
          permission_features->GetFeature(it->name());

      // The feature should exist since we just got an APIPermission
      // for it. The two systems should be updated together whenever a
      // permission is added.
      DCHECK(feature);
      // http://crbug.com/176381
      if (!feature) {
        to_remove.push_back(it->id());
        continue;
      }

      Feature::Availability availability =
          feature->IsAvailableToManifest(
              id(),
              GetType(),
              Feature::ConvertLocation(location()),
              manifest_version());
      if (!availability.is_available()) {
        // Don't fail, but warn the developer that the manifest contains
        // unrecognized permissions. This may happen legitimately if the
        // extensions requests platform- or channel-specific permissions.
        install_warnings_.push_back(InstallWarning(InstallWarning::FORMAT_TEXT,
                                                   availability.message()));
        to_remove.push_back(it->id());
        continue;
      }

      if (it->id() == APIPermission::kExperimental) {
        if (!CanSpecifyExperimentalPermission()) {
          *error = ASCIIToUTF16(errors::kExperimentalFlagRequired);
          return false;
        }
      }
    }

    // Remove permissions that are not available to this extension.
    for (std::vector<APIPermission::ID>::const_iterator it = to_remove.begin();
         it != to_remove.end(); ++it) {
      api_permissions->erase(*it);
    }

    // Parse host pattern permissions.
    const int kAllowedSchemes = CanExecuteScriptEverywhere() ?
        URLPattern::SCHEME_ALL : kValidHostPermissionSchemes;

    for (std::vector<std::string>::const_iterator it = host_data.begin();
         it != host_data.end(); ++it) {
      const std::string& permission_str = *it;

      // Check if it's a host pattern permission.
      URLPattern pattern = URLPattern(kAllowedSchemes);
      URLPattern::ParseResult parse_result = pattern.Parse(permission_str);
      if (parse_result == URLPattern::PARSE_SUCCESS) {
        // The path component is not used for host permissions, so we force it
        // to match all paths.
        pattern.SetPath("/*");
        int valid_schemes = pattern.valid_schemes();
        if (pattern.MatchesScheme(chrome::kFileScheme) &&
            !CanExecuteScriptEverywhere()) {
          wants_file_access_ = true;
          if (!(creation_flags_ & ALLOW_FILE_ACCESS))
            valid_schemes &= ~URLPattern::SCHEME_FILE;
        }

        if (pattern.scheme() != chrome::kChromeUIScheme &&
            !CanExecuteScriptEverywhere()) {
          // Keep chrome:// in allowed schemes only if it's explicitly requested
          // or CanExecuteScriptEverywhere is true. If the
          // extensions_on_chrome_urls flag is not set, CanSpecifyHostPermission
          // will fail, so don't check the flag here.
          valid_schemes &= ~URLPattern::SCHEME_CHROMEUI;
        }
        pattern.SetValidSchemes(valid_schemes);

        if (!CanSpecifyHostPermission(pattern, *api_permissions)) {
          // TODO(aboxhall): make a warning (see line 633)
          *error = ErrorUtils::FormatErrorMessageUTF16(
              errors::kInvalidPermissionScheme, permission_str);
          return false;
        }

        host_permissions->AddPattern(pattern);

        // We need to make sure all_urls matches chrome://favicon and
        // (maybe) chrome://thumbnail, so add them back in to host_permissions
        // separately.
        if (pattern.match_all_urls()) {
          host_permissions->AddPattern(
              URLPattern(URLPattern::SCHEME_CHROMEUI,
                         chrome::kChromeUIFaviconURL));
          if (api_permissions->find(APIPermission::kExperimental) !=
              api_permissions->end()) {
            host_permissions->AddPattern(
                URLPattern(URLPattern::SCHEME_CHROMEUI,
                           chrome::kChromeUIThumbnailURL));
          }
        }
        continue;
      }

      // It's probably an unknown API permission. Do not throw an error so
      // extensions can retain backwards compatability (http://crbug.com/42742).
      install_warnings_.push_back(InstallWarning(
          InstallWarning::FORMAT_TEXT,
          base::StringPrintf(
              "Permission '%s' is unknown or URL pattern is malformed.",
              permission_str.c_str())));
    }
  }
  return true;
}

bool Extension::HasAPIPermission(APIPermission::ID permission) const {
  base::AutoLock auto_lock(runtime_data_lock_);
  return runtime_data_.GetActivePermissions()->HasAPIPermission(permission);
}

bool Extension::HasAPIPermission(const std::string& function_name) const {
  base::AutoLock auto_lock(runtime_data_lock_);
  return runtime_data_.GetActivePermissions()->
      HasAccessToFunction(function_name, true);
}

bool Extension::HasAPIPermissionForTab(int tab_id,
                                       APIPermission::ID permission) const {
  base::AutoLock auto_lock(runtime_data_lock_);
  if (runtime_data_.GetActivePermissions()->HasAPIPermission(permission))
    return true;
  scoped_refptr<const PermissionSet> tab_specific_permissions =
      runtime_data_.GetTabSpecificPermissions(tab_id);
  return tab_specific_permissions.get() &&
         tab_specific_permissions->HasAPIPermission(permission);
}

bool Extension::CheckAPIPermissionWithParam(APIPermission::ID permission,
    const APIPermission::CheckParam* param) const {
  base::AutoLock auto_lock(runtime_data_lock_);
  return runtime_data_.GetActivePermissions()->
      CheckAPIPermissionWithParam(permission, param);
}

const URLPatternSet& Extension::GetEffectiveHostPermissions() const {
  base::AutoLock auto_lock(runtime_data_lock_);
  return runtime_data_.GetActivePermissions()->effective_hosts();
}

bool Extension::CanSilentlyIncreasePermissions() const {
  return location() != Manifest::INTERNAL;
}

bool Extension::HasHostPermission(const GURL& url) const {
  base::AutoLock auto_lock(runtime_data_lock_);
  return runtime_data_.GetActivePermissions()->
      HasExplicitAccessToOrigin(url);
}

bool Extension::HasEffectiveAccessToAllHosts() const {
  base::AutoLock auto_lock(runtime_data_lock_);
  return runtime_data_.GetActivePermissions()->HasEffectiveAccessToAllHosts();
}

bool Extension::HasFullPermissions() const {
  base::AutoLock auto_lock(runtime_data_lock_);
  return runtime_data_.GetActivePermissions()->HasEffectiveFullAccess();
}

PermissionMessages Extension::GetPermissionMessages() const {
  base::AutoLock auto_lock(runtime_data_lock_);
  if (IsTrustedId(id())) {
    return PermissionMessages();
  } else {
    return runtime_data_.GetActivePermissions()->GetPermissionMessages(
        GetType());
  }
}

std::vector<string16> Extension::GetPermissionMessageStrings() const {
  base::AutoLock auto_lock(runtime_data_lock_);
  if (IsTrustedId(id()))
    return std::vector<string16>();
  else
    return runtime_data_.GetActivePermissions()->GetWarningMessages(GetType());
}

bool Extension::ShouldSkipPermissionWarnings() const {
  return IsTrustedId(id());
}

void Extension::SetActivePermissions(
    const PermissionSet* permissions) const {
  base::AutoLock auto_lock(runtime_data_lock_);
  runtime_data_.SetActivePermissions(permissions);
}

scoped_refptr<const PermissionSet>
    Extension::GetActivePermissions() const {
  base::AutoLock auto_lock(runtime_data_lock_);
  return runtime_data_.GetActivePermissions();
}

bool Extension::ShowConfigureContextMenus() const {
  // Don't show context menu for component extensions. We might want to show
  // options for component extension button but now there is no component
  // extension with options. All other menu items like uninstall have
  // no sense for component extensions.
  return location() != Manifest::COMPONENT;
}

std::set<base::FilePath> Extension::GetBrowserImages() const {
  std::set<base::FilePath> image_paths;
  // TODO(viettrungluu): These |FilePath::FromWStringHack(UTF8ToWide())|
  // indicate that we're doing something wrong.

  // Extension icons.
  for (ExtensionIconSet::IconMap::const_iterator iter =
           IconsInfo::GetIcons(this).map().begin();
       iter != IconsInfo::GetIcons(this).map().end(); ++iter) {
    image_paths.insert(
        base::FilePath::FromWStringHack(UTF8ToWide(iter->second)));
  }

  // Theme images.
  DictionaryValue* theme_images = ThemeInfo::GetThemeImages(this);
  if (theme_images) {
    for (DictionaryValue::Iterator it(*theme_images); !it.IsAtEnd();
         it.Advance()) {
      std::string val;
      if (it.value().GetAsString(&val))
        image_paths.insert(base::FilePath::FromWStringHack(UTF8ToWide(val)));
    }
  }

  const ActionInfo* page_action_info = ActionInfo::GetPageActionInfo(this);
  if (page_action_info && !page_action_info->default_icon.empty()) {
    for (ExtensionIconSet::IconMap::const_iterator iter =
             page_action_info->default_icon.map().begin();
         iter != page_action_info->default_icon.map().end();
         ++iter) {
      image_paths.insert(
          base::FilePath::FromWStringHack(UTF8ToWide(iter->second)));
    }
  }

  const ActionInfo* browser_action = ActionInfo::GetBrowserActionInfo(this);
  if (browser_action && !browser_action->default_icon.empty()) {
    for (ExtensionIconSet::IconMap::const_iterator iter =
             browser_action->default_icon.map().begin();
         iter != browser_action->default_icon.map().end();
         ++iter) {
      image_paths.insert(
          base::FilePath::FromWStringHack(UTF8ToWide(iter->second)));
    }
  }

  return image_paths;
}

GURL Extension::GetFullLaunchURL() const {
  return launch_local_path().empty() ? GURL(launch_web_url()) :
                                       url().Resolve(launch_local_path());
}

bool Extension::CanExecuteScriptOnPage(const GURL& document_url,
                                       const GURL& top_frame_url,
                                       int tab_id,
                                       const UserScript* script,
                                       std::string* error) const {
  base::AutoLock auto_lock(runtime_data_lock_);
  // The gallery is special-cased as a restricted URL for scripting to prevent
  // access to special JS bindings we expose to the gallery (and avoid things
  // like extensions removing the "report abuse" link).
  // TODO(erikkay): This seems like the wrong test.  Shouldn't we we testing
  // against the store app extent?
  GURL store_url(extension_urls::GetWebstoreLaunchURL());
  if ((document_url.host() == store_url.host()) &&
      !CanExecuteScriptEverywhere() &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAllowScriptingGallery)) {
    if (error)
      *error = errors::kCannotScriptGallery;
    return false;
  }

  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kExtensionsOnChromeURLs)) {
    if (document_url.SchemeIs(chrome::kChromeUIScheme) &&
        !CanExecuteScriptEverywhere()) {
      return false;
    }
  }

  if (top_frame_url.SchemeIs(extensions::kExtensionScheme) &&
      top_frame_url.GetOrigin() !=
          GetBaseURLFromExtensionId(id()).GetOrigin() &&
      !CanExecuteScriptEverywhere()) {
    return false;
  }

  // If a tab ID is specified, try the tab-specific permissions.
  if (tab_id >= 0) {
    scoped_refptr<const PermissionSet> tab_permissions =
        runtime_data_.GetTabSpecificPermissions(tab_id);
    if (tab_permissions.get() &&
        tab_permissions->explicit_hosts().MatchesSecurityOrigin(document_url)) {
      return true;
    }
  }

  // If a script is specified, use its matches.
  if (script)
    return script->MatchesURL(document_url);

  // Otherwise, see if this extension has permission to execute script
  // programmatically on pages.
  if (runtime_data_.GetActivePermissions()->HasExplicitAccessToOrigin(
          document_url)) {
    return true;
  }

  if (error) {
    *error = ErrorUtils::FormatErrorMessage(errors::kCannotAccessPage,
                                            document_url.spec());
  }

  return false;
}

bool Extension::CanExecuteScriptEverywhere() const {
  if (location() == Manifest::COMPONENT)
    return true;

  ScriptingWhitelist* whitelist = ExtensionConfig::GetInstance()->whitelist();

  for (ScriptingWhitelist::const_iterator it = whitelist->begin();
       it != whitelist->end(); ++it) {
    if (id() == *it) {
      return true;
    }
  }

  return false;
}

bool Extension::CanCaptureVisiblePage(const GURL& page_url,
                                      int tab_id,
                                      std::string* error) const {
  if (tab_id >= 0) {
    scoped_refptr<const PermissionSet> tab_permissions =
        GetTabSpecificPermissions(tab_id);
    if (tab_permissions.get() &&
        tab_permissions->explicit_hosts().MatchesSecurityOrigin(page_url)) {
      return true;
    }
  }

  if (HasHostPermission(page_url) || page_url.GetOrigin() == url())
    return true;

  if (error) {
    *error = ErrorUtils::FormatErrorMessage(errors::kCannotAccessPage,
                                            page_url.spec());
  }
  return false;
}

bool Extension::UpdatesFromGallery() const {
  return extension_urls::IsWebstoreUpdateUrl(ManifestURL::GetUpdateURL(this));
}

bool Extension::OverlapsWithOrigin(const GURL& origin) const {
  if (url() == origin)
    return true;

  if (web_extent().is_empty())
    return false;

  // Note: patterns and extents ignore port numbers.
  URLPattern origin_only_pattern(kValidWebExtentSchemes);
  if (!origin_only_pattern.SetScheme(origin.scheme()))
    return false;
  origin_only_pattern.SetHost(origin.host());
  origin_only_pattern.SetPath("/*");

  URLPatternSet origin_only_pattern_list;
  origin_only_pattern_list.AddPattern(origin_only_pattern);

  return web_extent().OverlapsWith(origin_only_pattern_list);
}

Extension::SyncType Extension::GetSyncType() const {
  if (!IsSyncable()) {
    // We have a non-standard location.
    return SYNC_TYPE_NONE;
  }

  // Disallow extensions with non-gallery auto-update URLs for now.
  //
  // TODO(akalin): Relax this restriction once we've put in UI to
  // approve synced extensions.
  if (!ManifestURL::GetUpdateURL(this).is_empty() && !UpdatesFromGallery())
    return SYNC_TYPE_NONE;

  // Disallow extensions with native code plugins.
  //
  // TODO(akalin): Relax this restriction once we've put in UI to
  // approve synced extensions.
  if (PluginInfo::HasPlugins(this))
    return SYNC_TYPE_NONE;

  switch (GetType()) {
    case Manifest::TYPE_EXTENSION:
      return SYNC_TYPE_EXTENSION;

    case Manifest::TYPE_USER_SCRIPT:
      // We only want to sync user scripts with gallery update URLs.
      if (UpdatesFromGallery())
        return SYNC_TYPE_EXTENSION;
      else
        return SYNC_TYPE_NONE;

    case Manifest::TYPE_HOSTED_APP:
    case Manifest::TYPE_LEGACY_PACKAGED_APP:
    case Manifest::TYPE_PLATFORM_APP:
        return SYNC_TYPE_APP;

    default:
      return SYNC_TYPE_NONE;
  }
}

bool Extension::IsSyncable() const {
  // TODO(akalin): Figure out if we need to allow some other types.

  // Default apps are not synced because otherwise they will pollute profiles
  // that don't already have them. Specially, if a user doesn't have default
  // apps, creates a new profile (which get default apps) and then enables sync
  // for it, then their profile everywhere gets the default apps.
  bool is_syncable = (location() == Manifest::INTERNAL &&
      !was_installed_by_default());
  // Sync the chrome web store to maintain its position on the new tab page.
  is_syncable |= (id() ==  extension_misc::kWebStoreAppId);
  return is_syncable;
}

bool Extension::RequiresSortOrdinal() const {
  return is_app() && (display_in_launcher_ || display_in_new_tab_page_);
}

bool Extension::ShouldDisplayInAppLauncher() const {
  // Only apps should be displayed in the launcher.
  return is_app() && display_in_launcher_;
}

bool Extension::ShouldDisplayInNewTabPage() const {
  // Only apps should be displayed on the NTP.
  return is_app() && display_in_new_tab_page_;
}

bool Extension::ShouldDisplayInExtensionSettings() const {
  // Don't show for themes since the settings UI isn't really useful for them.
  if (is_theme())
    return false;

  // Don't show component extensions because they are only extensions as an
  // implementation detail of Chrome.
  if (location() == Manifest::COMPONENT &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kShowComponentExtensionOptions)) {
    return false;
  }

  // Always show unpacked extensions and apps.
  if (Manifest::IsUnpackedLocation(location()))
    return true;

  // Unless they are unpacked, never show hosted apps. Note: We intentionally
  // show packaged apps and platform apps because there are some pieces of
  // functionality that are only available in chrome://extensions/ but which
  // are needed for packaged and platform apps. For example, inspecting
  // background pages. See http://crbug.com/116134.
  if (is_hosted_app())
    return false;

  return true;
}

scoped_refptr<const PermissionSet> Extension::GetTabSpecificPermissions(
    int tab_id) const {
  base::AutoLock auto_lock(runtime_data_lock_);
  return runtime_data_.GetTabSpecificPermissions(tab_id);
}

void Extension::UpdateTabSpecificPermissions(
    int tab_id,
    scoped_refptr<const PermissionSet> permissions) const {
  base::AutoLock auto_lock(runtime_data_lock_);
  runtime_data_.UpdateTabSpecificPermissions(tab_id, permissions);
}

void Extension::ClearTabSpecificPermissions(int tab_id) const {
  base::AutoLock auto_lock(runtime_data_lock_);
  runtime_data_.ClearTabSpecificPermissions(tab_id);
}

Extension::ManifestData* Extension::GetManifestData(const std::string& key)
    const {
  DCHECK(finished_parsing_manifest_ || thread_checker_.CalledOnValidThread());
  ManifestDataMap::const_iterator iter = manifest_data_.find(key);
  if (iter != manifest_data_.end())
    return iter->second.get();
  return NULL;
}

void Extension::SetManifestData(const std::string& key,
                                Extension::ManifestData* data) {
  DCHECK(!finished_parsing_manifest_ && thread_checker_.CalledOnValidThread());
  manifest_data_[key] = linked_ptr<ManifestData>(data);
}

Manifest::Location Extension::location() const {
  return manifest_->location();
}

const std::string& Extension::id() const {
  return manifest_->extension_id();
}

const std::string Extension::VersionString() const {
  return version()->GetString();
}

void Extension::AddInstallWarning(const InstallWarning& new_warning) {
  install_warnings_.push_back(new_warning);
}

void Extension::AddInstallWarnings(
    const std::vector<InstallWarning>& new_warnings) {
  install_warnings_.insert(install_warnings_.end(),
                           new_warnings.begin(), new_warnings.end());
}

bool Extension::is_app() const {
  return manifest_->is_app();
}

bool Extension::is_platform_app() const {
  return manifest_->is_platform_app();
}

bool Extension::is_hosted_app() const {
  return manifest()->is_hosted_app();
}

bool Extension::is_legacy_packaged_app() const {
  return manifest()->is_legacy_packaged_app();
}

bool Extension::is_extension() const {
  return manifest()->is_extension();
}

bool Extension::can_be_incognito_enabled() const {
  return !is_platform_app();
}

void Extension::AddWebExtentPattern(const URLPattern& pattern) {
  extent_.AddPattern(pattern);
}

bool Extension::is_theme() const {
  return manifest()->is_theme();
}

Extension::RuntimeData::RuntimeData() {}
Extension::RuntimeData::RuntimeData(const PermissionSet* active)
    : active_permissions_(active) {}
Extension::RuntimeData::~RuntimeData() {}

void Extension::RuntimeData::SetActivePermissions(
    const PermissionSet* active) {
  active_permissions_ = active;
}

scoped_refptr<const PermissionSet>
    Extension::RuntimeData::GetActivePermissions() const {
  return active_permissions_;
}

scoped_refptr<const PermissionSet>
    Extension::RuntimeData::GetTabSpecificPermissions(int tab_id) const {
  CHECK_GE(tab_id, 0);
  TabPermissionsMap::const_iterator it = tab_specific_permissions_.find(tab_id);
  return (it != tab_specific_permissions_.end()) ? it->second : NULL;
}

void Extension::RuntimeData::UpdateTabSpecificPermissions(
    int tab_id,
    scoped_refptr<const PermissionSet> permissions) {
  CHECK_GE(tab_id, 0);
  if (tab_specific_permissions_.count(tab_id)) {
    tab_specific_permissions_[tab_id] = PermissionSet::CreateUnion(
        tab_specific_permissions_[tab_id],
        permissions.get());
  } else {
    tab_specific_permissions_[tab_id] = permissions;
  }
}

void Extension::RuntimeData::ClearTabSpecificPermissions(int tab_id) {
  CHECK_GE(tab_id, 0);
  tab_specific_permissions_.erase(tab_id);
}

// static
bool Extension::InitExtensionID(extensions::Manifest* manifest,
                                const base::FilePath& path,
                                const std::string& explicit_id,
                                int creation_flags,
                                string16* error) {
  if (!explicit_id.empty()) {
    manifest->set_extension_id(explicit_id);
    return true;
  }

  if (manifest->HasKey(keys::kPublicKey)) {
    std::string public_key;
    std::string public_key_bytes;
    if (!manifest->GetString(keys::kPublicKey, &public_key) ||
        !ParsePEMKeyBytes(public_key, &public_key_bytes)) {
      *error = ASCIIToUTF16(errors::kInvalidKey);
      return false;
    }
    std::string extension_id = id_util::GenerateId(public_key_bytes);
    manifest->set_extension_id(extension_id);
    return true;
  }

  if (creation_flags & REQUIRE_KEY) {
    *error = ASCIIToUTF16(errors::kInvalidKey);
    return false;
  } else {
    // If there is a path, we generate the ID from it. This is useful for
    // development mode, because it keeps the ID stable across restarts and
    // reloading the extension.
    std::string extension_id = id_util::GenerateIdForPath(path);
    if (extension_id.empty()) {
      NOTREACHED() << "Could not create ID from path.";
      return false;
    }
    manifest->set_extension_id(extension_id);
    return true;
  }
}

// static
bool Extension::IsTrustedId(const std::string& id) {
  // See http://b/4946060 for more details.
  return id == std::string("nckgahadagoaajjgafhacjanaoiihapd");
}

Extension::Extension(const base::FilePath& path,
                     scoped_ptr<extensions::Manifest> manifest)
    : manifest_version_(0),
      converted_from_user_script_(false),
      manifest_(manifest.release()),
      finished_parsing_manifest_(false),
      is_storage_isolated_(false),
      launch_container_(extension_misc::LAUNCH_TAB),
      launch_width_(0),
      launch_height_(0),
      display_in_launcher_(true),
      display_in_new_tab_page_(true),
      wants_file_access_(false),
      creation_flags_(0) {
  DCHECK(path.empty() || path.IsAbsolute());
  path_ = id_util::MaybeNormalizePath(path);
}

Extension::~Extension() {
}

bool Extension::InitFromValue(int flags, string16* error) {
  DCHECK(error);

  base::AutoLock auto_lock(runtime_data_lock_);

  // Initialize permissions with an empty, default permission set.
  runtime_data_.SetActivePermissions(new PermissionSet());
  optional_permission_set_ = new PermissionSet();
  required_permission_set_ = new PermissionSet();

  creation_flags_ = flags;

  // Important to load manifest version first because many other features
  // depend on its value.
  if (!LoadManifestVersion(error))
    return false;

  // Validate minimum Chrome version. We don't need to store this, since the
  // extension is not valid if it is incorrect
  if (!CheckMinimumChromeVersion(error))
    return false;

  if (!LoadRequiredFeatures(error))
    return false;

  // We don't need to validate because InitExtensionID already did that.
  manifest_->GetString(keys::kPublicKey, &public_key_);

  extension_url_ = Extension::GetBaseURLFromExtensionId(id());

  // Load App settings. LoadExtent at least has to be done before
  // ParsePermissions(), because the valid permissions depend on what type of
  // package this is.
  if (is_app() && !LoadAppFeatures(error))
    return false;

  initial_api_permissions_.reset(new APIPermissionSet);
  URLPatternSet host_permissions;
  if (!ParsePermissions(keys::kPermissions,
                        error,
                        initial_api_permissions_.get(),
                        &host_permissions)) {
    return false;
  }

  // Check for any permissions that are optional only.
  for (APIPermissionSet::const_iterator i = initial_api_permissions_->begin();
       i != initial_api_permissions_->end(); ++i) {
    if ((*i)->info()->must_be_optional()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kPermissionMustBeOptional, (*i)->info()->name());
      return false;
    }
  }

  // TODO(jeremya/kalman) do this via the features system by exposing the
  // app.window API to platform apps, with no dependency on any permissions.
  // See http://crbug.com/120069.
  if (is_platform_app()) {
    initial_api_permissions_->insert(APIPermission::kAppCurrentWindowInternal);
    initial_api_permissions_->insert(APIPermission::kAppRuntime);
    initial_api_permissions_->insert(APIPermission::kAppWindow);
  }

  APIPermissionSet optional_api_permissions;
  URLPatternSet optional_host_permissions;
  if (!ParsePermissions(keys::kOptionalPermissions,
                        error,
                        &optional_api_permissions,
                        &optional_host_permissions)) {
    return false;
  }

  if (ContainsManifestForbiddenPermission(*initial_api_permissions_, error) ||
      ContainsManifestForbiddenPermission(optional_api_permissions, error)) {
    return false;
  }

  if (!LoadAppIsolation(error))
    return false;

  if (!LoadSharedFeatures(error))
    return false;

  if (manifest_->HasKey(keys::kConvertedFromUserScript))
    manifest_->GetBoolean(keys::kConvertedFromUserScript,
                          &converted_from_user_script_);

  if (HasMultipleUISurfaces()) {
    *error = ASCIIToUTF16(errors::kOneUISurfaceOnly);
    return false;
  }

  finished_parsing_manifest_ = true;

  runtime_data_.SetActivePermissions(new PermissionSet(
      this, *initial_api_permissions_, host_permissions));
  required_permission_set_ = new PermissionSet(
      this, *initial_api_permissions_, host_permissions);
  optional_permission_set_ = new PermissionSet(
      optional_api_permissions, optional_host_permissions, URLPatternSet());
  initial_api_permissions_.reset();

  return true;
}

bool Extension::LoadAppIsolation(string16* error) {
  // Platform apps always get isolated storage.
  if (is_platform_app()) {
    is_storage_isolated_ = true;
    return true;
  }

  // Other apps only get it if it is requested _and_ experimental APIs are
  // enabled.
  if (!initial_api_permissions()->count(APIPermission::kExperimental)
      || !is_app())
    return true;

  const Value* tmp_isolation = NULL;
  if (!manifest_->Get(keys::kIsolation, &tmp_isolation))
    return true;

  const ListValue* isolation_list = NULL;
  if (!tmp_isolation->GetAsList(&isolation_list)) {
    *error = ASCIIToUTF16(errors::kInvalidIsolation);
    return false;
  }

  for (size_t i = 0; i < isolation_list->GetSize(); ++i) {
    std::string isolation_string;
    if (!isolation_list->GetString(i, &isolation_string)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidIsolationValue,
          base::UintToString(i));
      return false;
    }

    // Check for isolated storage.
    if (isolation_string == values::kIsolatedStorage) {
      is_storage_isolated_ = true;
    } else {
      DLOG(WARNING) << "Did not recognize isolation type: " << isolation_string;
    }
  }
  return true;
}

bool Extension::LoadRequiredFeatures(string16* error) {
  if (!LoadName(error) ||
      !LoadVersion(error))
    return false;
  return true;
}

bool Extension::LoadName(string16* error) {
  string16 localized_name;
  if (!manifest_->GetString(keys::kName, &localized_name)) {
    *error = ASCIIToUTF16(errors::kInvalidName);
    return false;
  }
  non_localized_name_ = UTF16ToUTF8(localized_name);
  base::i18n::AdjustStringForLocaleDirection(&localized_name);
  name_ = UTF16ToUTF8(localized_name);
  return true;
}

bool Extension::LoadVersion(string16* error) {
  std::string version_str;
  if (!manifest_->GetString(keys::kVersion, &version_str)) {
    *error = ASCIIToUTF16(errors::kInvalidVersion);
    return false;
  }
  version_.reset(new Version(version_str));
  if (!version_->IsValid() || version_->components().size() > 4) {
    *error = ASCIIToUTF16(errors::kInvalidVersion);
    return false;
  }
  return true;
}

bool Extension::LoadAppFeatures(string16* error) {
  if (!LoadExtent(keys::kWebURLs, &extent_,
                  errors::kInvalidWebURLs, errors::kInvalidWebURL, error) ||
      !LoadLaunchURL(error) ||
      !LoadLaunchContainer(error)) {
    return false;
  }
  if (manifest_->HasKey(keys::kDisplayInLauncher) &&
      !manifest_->GetBoolean(keys::kDisplayInLauncher, &display_in_launcher_)) {
    *error = ASCIIToUTF16(errors::kInvalidDisplayInLauncher);
    return false;
  }
  if (manifest_->HasKey(keys::kDisplayInNewTabPage)) {
    if (!manifest_->GetBoolean(keys::kDisplayInNewTabPage,
                               &display_in_new_tab_page_)) {
      *error = ASCIIToUTF16(errors::kInvalidDisplayInNewTabPage);
      return false;
    }
  } else {
    // Inherit default from display_in_launcher property.
    display_in_new_tab_page_ = display_in_launcher_;
  }
  return true;
}

bool Extension::LoadExtent(const char* key,
                           URLPatternSet* extent,
                           const char* list_error,
                           const char* value_error,
                           string16* error) {
  const Value* temp_pattern_value = NULL;
  if (!manifest_->Get(key, &temp_pattern_value))
    return true;

  const ListValue* pattern_list = NULL;
  if (!temp_pattern_value->GetAsList(&pattern_list)) {
    *error = ASCIIToUTF16(list_error);
    return false;
  }

  for (size_t i = 0; i < pattern_list->GetSize(); ++i) {
    std::string pattern_string;
    if (!pattern_list->GetString(i, &pattern_string)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(value_error,
                                                       base::UintToString(i),
                                                       errors::kExpectString);
      return false;
    }

    URLPattern pattern(kValidWebExtentSchemes);
    URLPattern::ParseResult parse_result = pattern.Parse(pattern_string);
    if (parse_result == URLPattern::PARSE_ERROR_EMPTY_PATH) {
      pattern_string += "/";
      parse_result = pattern.Parse(pattern_string);
    }

    if (parse_result != URLPattern::PARSE_SUCCESS) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          value_error,
          base::UintToString(i),
          URLPattern::GetParseResultString(parse_result));
      return false;
    }

    // Do not allow authors to claim "<all_urls>".
    if (pattern.match_all_urls()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          value_error,
          base::UintToString(i),
          errors::kCannotClaimAllURLsInExtent);
      return false;
    }

    // Do not allow authors to claim "*" for host.
    if (pattern.host().empty()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          value_error,
          base::UintToString(i),
          errors::kCannotClaimAllHostsInExtent);
      return false;
    }

    // We do not allow authors to put wildcards in their paths. Instead, we
    // imply one at the end.
    if (pattern.path().find('*') != std::string::npos) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          value_error,
          base::UintToString(i),
          errors::kNoWildCardsInPaths);
      return false;
    }
    pattern.SetPath(pattern.path() + '*');

    extent->AddPattern(pattern);
  }

  return true;
}

bool Extension::LoadLaunchContainer(string16* error) {
  const Value* tmp_launcher_container = NULL;
  if (!manifest_->Get(keys::kLaunchContainer, &tmp_launcher_container))
    return true;

  std::string launch_container_string;
  if (!tmp_launcher_container->GetAsString(&launch_container_string)) {
    *error = ASCIIToUTF16(errors::kInvalidLaunchContainer);
    return false;
  }

  if (launch_container_string == values::kLaunchContainerPanel) {
    launch_container_ = extension_misc::LAUNCH_PANEL;
  } else if (launch_container_string == values::kLaunchContainerTab) {
    launch_container_ = extension_misc::LAUNCH_TAB;
  } else {
    *error = ASCIIToUTF16(errors::kInvalidLaunchContainer);
    return false;
  }

  bool can_specify_initial_size =
      launch_container_ == extension_misc::LAUNCH_PANEL ||
      launch_container_ == extension_misc::LAUNCH_WINDOW;

  // Validate the container width if present.
  if (!ReadLaunchDimension(manifest_.get(),
                           keys::kLaunchWidth,
                           &launch_width_,
                           can_specify_initial_size,
                           error)) {
      return false;
  }

  // Validate container height if present.
  if (!ReadLaunchDimension(manifest_.get(),
                           keys::kLaunchHeight,
                           &launch_height_,
                           can_specify_initial_size,
                           error)) {
      return false;
  }

  return true;
}

bool Extension::LoadLaunchURL(string16* error) {
  const Value* temp = NULL;

  // launch URL can be either local (to chrome-extension:// root) or an absolute
  // web URL.
  if (manifest_->Get(keys::kLaunchLocalPath, &temp)) {
    if (manifest_->Get(keys::kLaunchWebURL, NULL)) {
      *error = ASCIIToUTF16(errors::kLaunchPathAndURLAreExclusive);
      return false;
    }

    if (manifest_->Get(keys::kWebURLs, NULL)) {
      *error = ASCIIToUTF16(errors::kLaunchPathAndExtentAreExclusive);
      return false;
    }

    std::string launch_path;
    if (!temp->GetAsString(&launch_path)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidLaunchValue,
          keys::kLaunchLocalPath);
      return false;
    }

    // Ensure the launch path is a valid relative URL.
    GURL resolved = url().Resolve(launch_path);
    if (!resolved.is_valid() || resolved.GetOrigin() != url()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidLaunchValue,
          keys::kLaunchLocalPath);
      return false;
    }

    launch_local_path_ = launch_path;
  } else if (manifest_->Get(keys::kLaunchWebURL, &temp)) {
    std::string launch_url;
    if (!temp->GetAsString(&launch_url)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidLaunchValue,
          keys::kLaunchWebURL);
      return false;
    }

    // Ensure the launch URL is a valid absolute URL and web extent scheme.
    GURL url(launch_url);
    URLPattern pattern(kValidWebExtentSchemes);
    if (!url.is_valid() || !pattern.SetScheme(url.scheme())) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidLaunchValue,
          keys::kLaunchWebURL);
      return false;
    }

    launch_web_url_ = launch_url;
  } else if (is_legacy_packaged_app() || is_hosted_app()) {
    *error = ASCIIToUTF16(errors::kLaunchURLRequired);
    return false;
  }

  // If there is no extent, we default the extent based on the launch URL.
  if (web_extent().is_empty() && !launch_web_url().empty()) {
    GURL launch_url(launch_web_url());
    URLPattern pattern(kValidWebExtentSchemes);
    if (!pattern.SetScheme("*")) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidLaunchValue,
          keys::kLaunchWebURL);
      return false;
    }
    pattern.SetHost(launch_url.host());
    pattern.SetPath("/*");
    extent_.AddPattern(pattern);
  }

  // In order for the --apps-gallery-url switch to work with the gallery
  // process isolation, we must insert any provided value into the component
  // app's launch url and web extent.
  if (id() == extension_misc::kWebStoreAppId) {
    std::string gallery_url_str = CommandLine::ForCurrentProcess()->
        GetSwitchValueASCII(switches::kAppsGalleryURL);

    // Empty string means option was not used.
    if (!gallery_url_str.empty()) {
      GURL gallery_url(gallery_url_str);
      OverrideLaunchUrl(gallery_url);
    }
  } else if (id() == extension_misc::kCloudPrintAppId) {
    // In order for the --cloud-print-service switch to work, we must update
    // the launch URL and web extent.
    // TODO(sanjeevr): Ideally we want to use CloudPrintURL here but that is
    // currently under chrome/browser.
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    GURL cloud_print_service_url = GURL(command_line.GetSwitchValueASCII(
        switches::kCloudPrintServiceURL));
    if (!cloud_print_service_url.is_empty()) {
      std::string path(
          cloud_print_service_url.path() + "/enable_chrome_connector");
      GURL::Replacements replacements;
      replacements.SetPathStr(path);
      GURL cloud_print_enable_connector_url =
          cloud_print_service_url.ReplaceComponents(replacements);
      OverrideLaunchUrl(cloud_print_enable_connector_url);
    }
  } else if (id() == extension_misc::kChromeAppId) {
    // Override launch url to new tab.
    launch_web_url_ = chrome::kChromeUINewTabURL;
    extent_.ClearPatterns();
  }

  return true;
}

bool Extension::LoadSharedFeatures(string16* error) {
  if (!LoadDescription(error) ||
      !ManifestHandler::ParseExtension(this, error) ||
      !LoadNaClModules(error))
    return false;

  return true;
}

bool Extension::LoadDescription(string16* error) {
  if (manifest_->HasKey(keys::kDescription) &&
      !manifest_->GetString(keys::kDescription, &description_)) {
    *error = ASCIIToUTF16(errors::kInvalidDescription);
    return false;
  }
  return true;
}

bool Extension::LoadManifestVersion(string16* error) {
  // Get the original value out of the dictionary so that we can validate it
  // more strictly.
  if (manifest_->value()->HasKey(keys::kManifestVersion)) {
    int manifest_version = 1;
    if (!manifest_->GetInteger(keys::kManifestVersion, &manifest_version) ||
        manifest_version < 1) {
      *error = ASCIIToUTF16(errors::kInvalidManifestVersion);
      return false;
    }
  }

  manifest_version_ = manifest_->GetManifestVersion();
  if (creation_flags_ & REQUIRE_MODERN_MANIFEST_VERSION &&
      manifest_version_ < kModernManifestVersion &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAllowLegacyExtensionManifests)) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidManifestVersionOld,
        base::IntToString(kModernManifestVersion));
    return false;
  }

  return true;
}

bool Extension::LoadNaClModules(string16* error) {
  if (!manifest_->HasKey(keys::kNaClModules))
    return true;
  const ListValue* list_value = NULL;
  if (!manifest_->GetList(keys::kNaClModules, &list_value)) {
    *error = ASCIIToUTF16(errors::kInvalidNaClModules);
    return false;
  }

  for (size_t i = 0; i < list_value->GetSize(); ++i) {
    const DictionaryValue* module_value = NULL;
    if (!list_value->GetDictionary(i, &module_value)) {
      *error = ASCIIToUTF16(errors::kInvalidNaClModules);
      return false;
    }

    // Get nacl_modules[i].path.
    std::string path_str;
    if (!module_value->GetString(keys::kNaClModulesPath, &path_str)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidNaClModulesPath, base::IntToString(i));
      return false;
    }

    // Get nacl_modules[i].mime_type.
    std::string mime_type;
    if (!module_value->GetString(keys::kNaClModulesMIMEType, &mime_type)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidNaClModulesMIMEType, base::IntToString(i));
      return false;
    }

    nacl_modules_.push_back(NaClModuleInfo());
    nacl_modules_.back().url = GetResourceURL(path_str);
    nacl_modules_.back().mime_type = mime_type;
  }

  return true;
}

bool Extension::HasMultipleUISurfaces() const {
  int num_surfaces = 0;

  if (ActionInfo::GetPageActionInfo(this))
    ++num_surfaces;

  if (ActionInfo::GetBrowserActionInfo(this))
    ++num_surfaces;

  if (is_app())
    ++num_surfaces;

  return num_surfaces > 1;
}

void Extension::OverrideLaunchUrl(const GURL& override_url) {
  GURL new_url(override_url);
  if (!new_url.is_valid()) {
    DLOG(WARNING) << "Invalid override url given for " << name();
  } else {
    if (new_url.has_port()) {
      DLOG(WARNING) << "Override URL passed for " << name()
                    << " should not contain a port.  Removing it.";

      GURL::Replacements remove_port;
      remove_port.ClearPort();
      new_url = new_url.ReplaceComponents(remove_port);
    }

    launch_web_url_ = new_url.spec();

    URLPattern pattern(kValidWebExtentSchemes);
    URLPattern::ParseResult result = pattern.Parse(new_url.spec());
    DCHECK_EQ(result, URLPattern::PARSE_SUCCESS);
    pattern.SetPath(pattern.path() + '*');
    extent_.AddPattern(pattern);
  }
}

bool Extension::CanSpecifyExperimentalPermission() const {
  if (location() == Manifest::COMPONENT)
    return true;

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExperimentalExtensionApis)) {
    return true;
  }

  // We rely on the webstore to check access to experimental. This way we can
  // whitelist extensions to have access to experimental in just the store, and
  // not have to push a new version of the client.
  if (from_webstore())
    return true;

  return false;
}

bool Extension::CanSpecifyHostPermission(const URLPattern& pattern,
    const APIPermissionSet& permissions) const {
  if (!pattern.match_all_urls() &&
      pattern.MatchesScheme(chrome::kChromeUIScheme)) {
    // Regular extensions are only allowed access to chrome://favicon.
    if (pattern.host() == chrome::kChromeUIFaviconHost)
      return true;

    // Experimental extensions are also allowed chrome://thumb.
    //
    // TODO: A public API should be created for retrieving thumbnails.
    // See http://crbug.com/222856. A temporary hack is implemented here to
    // make chrome://thumbs available to NTP Russia extension as
    // non-experimental.
    if (pattern.host() == chrome::kChromeUIThumbnailHost) {
      return
          permissions.find(APIPermission::kExperimental) != permissions.end() ||
          (id() == kThumbsWhiteListedExtension && from_webstore());
    }

    // Component extensions can have access to all of chrome://*.
    if (CanExecuteScriptEverywhere())
      return true;

    if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kExtensionsOnChromeURLs))
      return true;

    // TODO(aboxhall): return from_webstore() when webstore handles blocking
    // extensions which request chrome:// urls
    return false;
  }

  // Otherwise, the valid schemes were handled by URLPattern.
  return true;
}

bool Extension::CheckMinimumChromeVersion(string16* error) const {
  if (!manifest_->HasKey(keys::kMinimumChromeVersion))
    return true;
  std::string minimum_version_string;
  if (!manifest_->GetString(keys::kMinimumChromeVersion,
                            &minimum_version_string)) {
    *error = ASCIIToUTF16(errors::kInvalidMinimumChromeVersion);
    return false;
  }

  Version minimum_version(minimum_version_string);
  if (!minimum_version.IsValid()) {
    *error = ASCIIToUTF16(errors::kInvalidMinimumChromeVersion);
    return false;
  }

  chrome::VersionInfo current_version_info;
  if (!current_version_info.is_valid()) {
    NOTREACHED();
    return false;
  }

  Version current_version(current_version_info.Version());
  if (!current_version.IsValid()) {
    DCHECK(false);
    return false;
  }

  if (current_version.CompareTo(minimum_version) < 0) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kChromeVersionTooLow,
        l10n_util::GetStringUTF8(IDS_PRODUCT_NAME),
        minimum_version_string);
    return false;
  }
  return true;
}

bool Extension::CheckPlatformAppFeatures(string16* error) const {
  if (!is_platform_app())
    return true;

  if (!BackgroundInfo::HasBackgroundPage(this)) {
    *error = ASCIIToUTF16(errors::kBackgroundRequiredForPlatformApps);
    return false;
  }

  if (!IncognitoInfo::IsSplitMode(this)) {
    *error = ASCIIToUTF16(errors::kInvalidIncognitoModeForPlatformApp);
    return false;
  }

  return true;
}

bool Extension::CheckConflictingFeatures(string16* error) const {
  if (BackgroundInfo::HasLazyBackgroundPage(this) &&
      HasAPIPermission(APIPermission::kWebRequest)) {
    *error = ASCIIToUTF16(errors::kWebRequestConflictsWithLazyBackground);
    return false;
  }

  return true;
}

ExtensionInfo::ExtensionInfo(const DictionaryValue* manifest,
                             const std::string& id,
                             const base::FilePath& path,
                             Manifest::Location location)
    : extension_id(id),
      extension_path(path),
      extension_location(location) {
  if (manifest)
    extension_manifest.reset(manifest->DeepCopy());
}

ExtensionInfo::~ExtensionInfo() {}

UnloadedExtensionInfo::UnloadedExtensionInfo(
    const Extension* extension,
    extension_misc::UnloadedExtensionReason reason)
  : reason(reason),
    already_disabled(false),
    extension(extension) {}

UpdatedExtensionPermissionsInfo::UpdatedExtensionPermissionsInfo(
    const Extension* extension,
    const PermissionSet* permissions,
    Reason reason)
    : reason(reason),
      extension(extension),
      permissions(permissions) {}

}   // namespace extensions
