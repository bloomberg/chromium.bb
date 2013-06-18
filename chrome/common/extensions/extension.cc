// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension.h"

#include "base/base64.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest.h"
#include "chrome/common/extensions/manifest_handler.h"
#include "chrome/common/extensions/permissions/api_permission_set.h"
#include "chrome/common/extensions/permissions/permission_set.h"
#include "chrome/common/extensions/permissions/permissions_data.h"
#include "chrome/common/extensions/permissions/permissions_info.h"
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
                                           const base::DictionaryValue& value,
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
                                           const base::DictionaryValue& value,
                                           int flags,
                                           const std::string& explicit_id,
                                           std::string* utf8_error) {
  DCHECK(utf8_error);
  string16 error;
  scoped_ptr<extensions::Manifest> manifest(
      new extensions::Manifest(
          location, scoped_ptr<base::DictionaryValue>(value.DeepCopy())));

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
  base::FilePath relative_file_path = base::FilePath::FromUTF8Unsafe(new_path);
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

bool Extension::HasAPIPermission(APIPermission::ID permission) const {
  return PermissionsData::HasAPIPermission(this, permission);
}

bool Extension::HasAPIPermission(const std::string& function_name) const {
  return PermissionsData::HasAPIPermission(this, function_name);
}

scoped_refptr<const PermissionSet> Extension::GetActivePermissions() const {
  return PermissionsData::GetActivePermissions(this);
}

bool Extension::ShowConfigureContextMenus() const {
  // Don't show context menu for component extensions. We might want to show
  // options for component extension button but now there is no component
  // extension with options. All other menu items like uninstall have
  // no sense for component extensions.
  return location() != Manifest::COMPONENT;
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

  // Don't show apps that aren't visible in either launcher or ntp.
  if (is_app() && !ShouldDisplayInAppLauncher() && !ShouldDisplayInNewTabPage())
    return false;

  // Unless they are unpacked, never show hosted apps. Note: We intentionally
  // show packaged apps and platform apps because there are some pieces of
  // functionality that are only available in chrome://extensions/ but which
  // are needed for packaged and platform apps. For example, inspecting
  // background pages. See http://crbug.com/116134.
  if (is_hosted_app())
    return false;

  return true;
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
  // Only component platform apps are supported in incognito.
  return !is_platform_app() || location() == Manifest::COMPONENT;
}

void Extension::AddWebExtentPattern(const URLPattern& pattern) {
  extent_.AddPattern(pattern);
}

bool Extension::is_theme() const {
  return manifest()->is_theme();
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

Extension::Extension(const base::FilePath& path,
                     scoped_ptr<extensions::Manifest> manifest)
    : manifest_version_(0),
      converted_from_user_script_(false),
      manifest_(manifest.release()),
      finished_parsing_manifest_(false),
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

  permissions_data_.reset(new PermissionsData);
  if (!permissions_data_->ParsePermissions(this, error))
    return false;

  if (manifest_->HasKey(keys::kConvertedFromUserScript)) {
    manifest_->GetBoolean(keys::kConvertedFromUserScript,
                          &converted_from_user_script_);
  }

  if (!LoadSharedFeatures(error))
    return false;

  finished_parsing_manifest_ = true;

  permissions_data_->FinalizePermissions(this);

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
                  errors::kInvalidWebURLs, errors::kInvalidWebURL, error)) {
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
  const base::Value* temp_pattern_value = NULL;
  if (!manifest_->Get(key, &temp_pattern_value))
    return true;

  const base::ListValue* pattern_list = NULL;
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

bool Extension::LoadSharedFeatures(string16* error) {
  if (!LoadDescription(error) ||
      !ManifestHandler::ParseExtension(this, error))
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

ExtensionInfo::ExtensionInfo(const base::DictionaryValue* manifest,
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

InstalledExtensionInfo::InstalledExtensionInfo(
    const Extension* extension,
    bool is_update,
    const std::string& old_name)
    : extension(extension),
      is_update(is_update),
      old_name(old_name) {}

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
