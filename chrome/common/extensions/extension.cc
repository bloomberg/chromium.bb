// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension.h"

#include <ostream>

#include "base/base64.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/stl_util.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/csp_validator.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/extensions/feature_switch.h"
#include "chrome/common/extensions/features/base_feature_provider.h"
#include "chrome/common/extensions/features/feature.h"
#include "chrome/common/extensions/manifest.h"
#include "chrome/common/extensions/manifest_handler.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "chrome/common/extensions/permissions/permission_set.h"
#include "chrome/common/extensions/permissions/permissions_info.h"
#include "chrome/common/extensions/user_script.h"
#include "chrome/common/url_constants.h"
#include "crypto/sha2.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/url_pattern_set.h"
#include "googleurl/src/url_util.h"
#include "grit/chromium_strings.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/glue/image_decoder.h"

#if defined(OS_WIN)
#include "base/win/metro.h"
#include "grit/generated_resources.h"
#endif

namespace keys = extension_manifest_keys;
namespace values = extension_manifest_values;
namespace errors = extension_manifest_errors;
namespace info_keys = extension_info_keys;

using extensions::csp_validator::ContentSecurityPolicyIsLegal;
using extensions::csp_validator::ContentSecurityPolicyIsSandboxed;
using extensions::csp_validator::ContentSecurityPolicyIsSecure;

namespace extensions {

namespace {

const int kModernManifestVersion = 2;
const int kPEMOutputColumns = 65;

// The maximum number of commands (including page action/browser actions) an
// extension can have.
const size_t kMaxCommandsPerExtension = 4;

// KEY MARKERS
const char kKeyBeginHeaderMarker[] = "-----BEGIN";
const char kKeyBeginFooterMarker[] = "-----END";
const char kKeyInfoEndMarker[] = "KEY-----";
const char kPublic[] = "PUBLIC";
const char kPrivate[] = "PRIVATE";

const int kRSAKeySize = 1024;

const char kDefaultContentSecurityPolicy[] =
    "script-src 'self' chrome-extension-resource:; object-src 'self'";

#define PLATFORM_APP_LOCAL_CSP_SOURCES \
    "'self' data: chrome-extension-resource:"
const char kDefaultPlatformAppContentSecurityPolicy[] =
    // Platform apps can only use local resources by default.
    "default-src 'self' chrome-extension-resource:;"
    // For remote resources, they can fetch them via XMLHttpRequest.
    "connect-src *;"
    // And serve them via data: or same-origin (blob:, filesystem:) URLs
    "style-src " PLATFORM_APP_LOCAL_CSP_SOURCES " 'unsafe-inline';"
    "img-src " PLATFORM_APP_LOCAL_CSP_SOURCES ";"
    "frame-src " PLATFORM_APP_LOCAL_CSP_SOURCES ";"
    "font-src " PLATFORM_APP_LOCAL_CSP_SOURCES ";"
    // Media can be loaded from remote resources since:
    // 1. <video> and <audio> have good fallback behavior when offline or under
    //    spotty connectivity.
    // 2. Fetching via XHR and serving via blob: URLs currently does not allow
    //    streaming or partial buffering.
    "media-src *;";

const char kDefaultSandboxedPageContentSecurityPolicy[] =
    "sandbox allow-scripts allow-forms allow-popups";

// Converts a normal hexadecimal string into the alphabet used by extensions.
// We use the characters 'a'-'p' instead of '0'-'f' to avoid ever having a
// completely numeric host, since some software interprets that as an IP
// address.
static void ConvertHexadecimalToIDAlphabet(std::string* id) {
  for (size_t i = 0; i < id->size(); ++i) {
    int val;
    if (base::HexStringToInt(base::StringPiece(id->begin() + i,
                                               id->begin() + i + 1),
                             &val)) {
      (*id)[i] = val + 'a';
    } else {
      (*id)[i] = 'a';
    }
  }
}

// Strips leading slashes from the file path. Returns true iff the final path is
// non empty.
bool NormalizeAndValidatePath(std::string* path) {
  size_t first_non_slash = path->find_first_not_of('/');
  if (first_non_slash == std::string::npos) {
    *path = "";
    return false;
  }

  *path = path->substr(first_non_slash);
  return true;
}

// Loads icon paths defined in dictionary |icons_value| into ExtensionIconSet
// |icons|. |icons_value| is a dictionary value {icon size -> icon path}. Icons
// in |icons_value| whose size is not in |icon_sizes| will be ignored.
// Returns success. If load fails, |error| will be set.
bool LoadIconsFromDictionary(const DictionaryValue* icons_value,
                             const int* icon_sizes,
                             size_t num_icon_sizes,
                             ExtensionIconSet* icons,
                             string16* error) {
  DCHECK(icons);
  for (size_t i = 0; i < num_icon_sizes; ++i) {
    std::string key = base::IntToString(icon_sizes[i]);
    if (icons_value->HasKey(key)) {
      std::string icon_path;
      if (!icons_value->GetString(key, &icon_path)) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidIconPath, key);
        return false;
      }

      if (!NormalizeAndValidatePath(&icon_path)) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidIconPath, key);
        return false;
      }

      icons->Add(icon_sizes[i], icon_path);
    }
  }
  return true;
}

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

// Rank extension locations in a way that allows
// Extension::GetHigherPriorityLocation() to compare locations.
// An extension installed from two locations will have the location
// with the higher rank, as returned by this function. The actual
// integer values may change, and should never be persisted.
int GetLocationRank(Extension::Location location) {
  const int kInvalidRank = -1;
  int rank = kInvalidRank;  // Will CHECK that rank is not kInvalidRank.

  switch (location) {
    // Component extensions can not be overriden by any other type.
    case Extension::COMPONENT:
      rank = 6;
      break;

    // Policy controlled extensions may not be overridden by any type
    // that is not part of chrome.
    case Extension::EXTERNAL_POLICY_DOWNLOAD:
      rank = 5;
      break;

    // A developer-loaded extension should override any installed type
    // that a user can disable.
    case Extension::LOAD:
      rank = 4;
      break;

    // The relative priority of various external sources is not important,
    // but having some order ensures deterministic behavior.
    case Extension::EXTERNAL_REGISTRY:
      rank = 3;
      break;

    case Extension::EXTERNAL_PREF:
      rank = 2;
      break;

    case Extension::EXTERNAL_PREF_DOWNLOAD:
      rank = 1;
      break;

    // User installed extensions are overridden by any external type.
    case Extension::INTERNAL:
      rank = 0;
      break;

    default:
      NOTREACHED() << "Need to add new extension locaton " << location;
  }

  CHECK(rank != kInvalidRank);
  return rank;
}

bool ReadLaunchDimension(const extensions::Manifest* manifest,
                         const char* key,
                         int* target,
                         bool is_valid_container,
                         string16* error) {
  Value* temp = NULL;
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

std::string SizeToString(const gfx::Size& max_size) {
  return base::IntToString(max_size.width()) + "x" +
         base::IntToString(max_size.height());
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

const FilePath::CharType Extension::kManifestFilename[] =
    FILE_PATH_LITERAL("manifest.json");
const FilePath::CharType Extension::kLocaleFolder[] =
    FILE_PATH_LITERAL("_locales");
const FilePath::CharType Extension::kMessagesFilename[] =
    FILE_PATH_LITERAL("messages.json");

#if defined(OS_WIN)
const char Extension::kExtensionRegistryPath[] =
    "Software\\Google\\Chrome\\Extensions";
#endif

// first 16 bytes of SHA256 hashed public key.
const size_t Extension::kIdSize = 16;

const char Extension::kMimeType[] = "application/x-chrome-extension";

const int Extension::kPageActionIconMaxSize = 19;
const int Extension::kBrowserActionIconMaxSize = 19;

const int Extension::kValidWebExtentSchemes =
    URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS;

const int Extension::kValidHostPermissionSchemes =
    UserScript::kValidUserScriptSchemes | URLPattern::SCHEME_CHROMEUI;

Extension::Requirements::Requirements()
    : webgl(false),
      css3d(false),
      npapi(false) {
}

Extension::Requirements::~Requirements() {}

Extension::OAuth2Info::OAuth2Info() {}
Extension::OAuth2Info::~OAuth2Info() {}

Extension::ActionInfo::ActionInfo() {}
Extension::ActionInfo::~ActionInfo() {}

//
// Extension
//

bool Extension::InstallWarning::operator==(const InstallWarning& other) const {
  return format == other.format && message == other.message;
}

// static
scoped_refptr<Extension> Extension::Create(const FilePath& path,
                                           Location location,
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

scoped_refptr<Extension> Extension::Create(const FilePath& path,
                                           Location location,
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

  InstallWarningVector install_warnings;
  manifest->ValidateManifest(utf8_error, &install_warnings);
  if (!utf8_error->empty())
    return NULL;

  scoped_refptr<Extension> extension = new Extension(path, manifest.Pass());
  extension->install_warnings_.swap(install_warnings);

  if (!extension->InitFromValue(flags, &error)) {
    *utf8_error = UTF16ToUTF8(error);
    return NULL;
  }

  if (!extension->CheckPlatformAppFeatures(utf8_error) ||
      !extension->CheckConflictingFeatures(utf8_error)) {
    return NULL;
  }

  return extension;
}

// static
Extension::Location Extension::GetHigherPriorityLocation(
    Extension::Location loc1, Extension::Location loc2) {
  if (loc1 == loc2)
    return loc1;

  int loc1_rank = GetLocationRank(loc1);
  int loc2_rank = GetLocationRank(loc2);

  // If two different locations have the same rank, then we can not
  // deterministicly choose a location.
  CHECK(loc1_rank != loc2_rank);

  // Highest rank has highest priority.
  return (loc1_rank > loc2_rank ? loc1 : loc2 );
}

// static
bool Extension::IdIsValid(const std::string& id) {
  // Verify that the id is legal.
  if (id.size() != (kIdSize * 2))
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
std::string Extension::GenerateIdForPath(const FilePath& path) {
  FilePath new_path = Extension::MaybeNormalizePath(path);
  std::string path_bytes =
      std::string(reinterpret_cast<const char*>(new_path.value().data()),
                  new_path.value().size() * sizeof(FilePath::CharType));
  std::string id;
  return GenerateId(path_bytes, &id) ? id : "";
}

// static
bool Extension::IsExtension(const FilePath& file_name) {
  return file_name.MatchesExtension(chrome::kExtensionFileExtension);
}

void Extension::GetBasicInfo(bool enabled,
                             DictionaryValue* info) const {
  info->SetString(info_keys::kIdKey, id());
  info->SetString(info_keys::kNameKey, name());
  info->SetBoolean(info_keys::kEnabledKey, enabled);
  info->SetBoolean(info_keys::kOfflineEnabledKey, offline_enabled());
  info->SetString(info_keys::kVersionKey, VersionString());
  info->SetString(info_keys::kDescriptionKey, description());
  info->SetString(info_keys::kOptionsUrlKey,
                  ManifestURL::GetOptionsPage(this).possibly_invalid_spec());
  info->SetString(info_keys::kHomepageUrlKey,
                  ManifestURL::GetHomepageURL(this).possibly_invalid_spec());
  info->SetString(info_keys::kDetailsUrlKey,
                  details_url().possibly_invalid_spec());
  info->SetBoolean(info_keys::kPackagedAppKey, is_platform_app());
}

Extension::Type Extension::GetType() const {
  return converted_from_user_script() ? TYPE_USER_SCRIPT : manifest_->type();
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

bool Extension::IsSandboxedPage(const std::string& relative_path) const {
  return ResourceMatches(sandboxed_pages_, relative_path);
}

std::string Extension::GetResourceContentSecurityPolicy(
    const std::string& relative_path) const {
  return IsSandboxedPage(relative_path) ?
      sandboxed_pages_content_security_policy_ : content_security_policy();
}

ExtensionResource Extension::GetResource(
    const std::string& relative_path) const {
  std::string new_path = relative_path;
  // We have some legacy data where resources have leading slashes.
  // See: http://crbug.com/121164
  if (!new_path.empty() && new_path.at(0) == '/')
    new_path.erase(0, 1);
#if defined(OS_POSIX)
  FilePath relative_file_path(new_path);
#elif defined(OS_WIN)
  FilePath relative_file_path(UTF8ToWide(new_path));
#endif
  ExtensionResource r(id(), path(), relative_file_path);
  if ((creation_flags() & Extension::FOLLOW_SYMLINKS_ANYWHERE)) {
    r.set_follow_symlinks_anywhere();
  }
  return r;
}

ExtensionResource Extension::GetResource(
    const FilePath& relative_file_path) const {
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
bool Extension::GenerateId(const std::string& input, std::string* output) {
  DCHECK(output);
  uint8 hash[Extension::kIdSize];
  crypto::SHA256HashString(input, hash, sizeof(hash));
  *output = StringToLowerASCII(base::HexEncode(hash, sizeof(hash)));
  ConvertHexadecimalToIDAlphabet(output);

  return true;
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
void Extension::DecodeIcon(const Extension* extension,
                           int preferred_icon_size,
                           ExtensionIconSet::MatchType match_type,
                           scoped_ptr<SkBitmap>* result) {
  std::string path = extension->icons().Get(preferred_icon_size, match_type);
  int size = extension->icons().GetIconSizeFromPath(path);
  ExtensionResource icon_resource = extension->GetResource(path);
  DecodeIconFromPath(icon_resource.GetFilePath(), size, result);
}

// static
void Extension::DecodeIcon(const Extension* extension,
                           int icon_size,
                           scoped_ptr<SkBitmap>* result) {
  DecodeIcon(extension, icon_size, ExtensionIconSet::MATCH_EXACTLY, result);
}

// static
void Extension::DecodeIconFromPath(const FilePath& icon_path,
                                   int icon_size,
                                   scoped_ptr<SkBitmap>* result) {
  if (icon_path.empty())
    return;

  std::string file_contents;
  if (!file_util::ReadFileToString(icon_path, &file_contents)) {
    DLOG(ERROR) << "Could not read icon file: " << icon_path.LossyDisplayName();
    return;
  }

  // Decode the image using WebKit's image decoder.
  const unsigned char* data =
    reinterpret_cast<const unsigned char*>(file_contents.data());
  webkit_glue::ImageDecoder decoder;
  scoped_ptr<SkBitmap> decoded(new SkBitmap());
  *decoded = decoder.Decode(data, file_contents.length());
  if (decoded->empty()) {
    DLOG(ERROR) << "Could not decode icon file: "
                << icon_path.LossyDisplayName();
    return;
  }

  if (decoded->width() != icon_size || decoded->height() != icon_size) {
    DLOG(ERROR) << "Icon file has unexpected size: "
                << base::IntToString(decoded->width()) << "x"
                << base::IntToString(decoded->height());
    return;
  }

  result->swap(decoded);
}

// static
const gfx::ImageSkia& Extension::GetDefaultIcon(bool is_app) {
  int id = is_app ? IDR_APP_DEFAULT_ICON : IDR_EXTENSION_DEFAULT_ICON;
  return *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(id);
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
    ListValue* permissions = NULL;
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
      CHECK(feature);

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
        if (!CanSpecifyHostPermission(pattern, *api_permissions)) {
          *error = ErrorUtils::FormatErrorMessageUTF16(
              errors::kInvalidPermissionScheme, permission_str);
          return false;
        }

        // The path component is not used for host permissions, so we force it
        // to match all paths.
        pattern.SetPath("/*");

        if (pattern.MatchesScheme(chrome::kFileScheme) &&
            !CanExecuteScriptEverywhere()) {
          wants_file_access_ = true;
          if (!(creation_flags_ & ALLOW_FILE_ACCESS)) {
            pattern.SetValidSchemes(
                pattern.valid_schemes() & ~URLPattern::SCHEME_FILE);
          }
        }

        host_permissions->AddPattern(pattern);
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
      HasAccessToFunction(function_name);
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
  return location() != INTERNAL;
}

bool Extension::HasHostPermission(const GURL& url) const {
  if (url.SchemeIs(chrome::kChromeUIScheme) &&
      url.host() != chrome::kChromeUIFaviconHost &&
      url.host() != chrome::kChromeUIThumbnailHost &&
      location() != Extension::COMPONENT) {
    return false;
  }

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
  return location() != Extension::COMPONENT;
}

std::set<FilePath> Extension::GetBrowserImages() const {
  std::set<FilePath> image_paths;
  // TODO(viettrungluu): These |FilePath::FromWStringHack(UTF8ToWide())|
  // indicate that we're doing something wrong.

  // Extension icons.
  for (ExtensionIconSet::IconMap::const_iterator iter = icons().map().begin();
       iter != icons().map().end(); ++iter) {
    image_paths.insert(FilePath::FromWStringHack(UTF8ToWide(iter->second)));
  }

  // Theme images.
  DictionaryValue* theme_images = GetThemeImages();
  if (theme_images) {
    for (DictionaryValue::key_iterator it = theme_images->begin_keys();
         it != theme_images->end_keys(); ++it) {
      std::string val;
      if (theme_images->GetStringWithoutPathExpansion(*it, &val))
        image_paths.insert(FilePath::FromWStringHack(UTF8ToWide(val)));
    }
  }

  if (page_action_info() && !page_action_info()->default_icon.empty()) {
    for (ExtensionIconSet::IconMap::const_iterator iter =
             page_action_info()->default_icon.map().begin();
         iter != page_action_info()->default_icon.map().end();
         ++iter) {
       image_paths.insert(FilePath::FromWStringHack(UTF8ToWide(iter->second)));
    }
  }

  if (browser_action_info() && !browser_action_info()->default_icon.empty()) {
    for (ExtensionIconSet::IconMap::const_iterator iter =
             browser_action_info()->default_icon.map().begin();
         iter != browser_action_info()->default_icon.map().end();
         ++iter) {
       image_paths.insert(FilePath::FromWStringHack(UTF8ToWide(iter->second)));
    }
  }

  return image_paths;
}

ExtensionResource Extension::GetIconResource(
    int size, ExtensionIconSet::MatchType match_type) const {
  std::string path = icons().Get(size, match_type);
  return path.empty() ? ExtensionResource() : GetResource(path);
}

GURL Extension::GetIconURL(int size,
                           ExtensionIconSet::MatchType match_type) const {
  std::string path = icons().Get(size, match_type);
  return path.empty() ? GURL() : GetResourceURL(path);
}

GURL Extension::GetFullLaunchURL() const {
  return launch_local_path().empty() ? GURL(launch_web_url()) :
                                       url().Resolve(launch_local_path());
}

void Extension::SetCachedImage(const ExtensionResource& source,
                               const SkBitmap& image,
                               const gfx::Size& original_size) const {
  DCHECK(source.extension_root() == path());  // The resource must come from
                                              // this extension.
  const FilePath& path = source.relative_path();
  gfx::Size actual_size(image.width(), image.height());
  std::string location;
  if (actual_size != original_size)
    location = SizeToString(actual_size);
  image_cache_[ImageCacheKey(path, location)] = image;
}

bool Extension::HasCachedImage(const ExtensionResource& source,
                               const gfx::Size& max_size) const {
  DCHECK(source.extension_root() == path());  // The resource must come from
                                              // this extension.
  return GetCachedImageImpl(source, max_size) != NULL;
}

SkBitmap Extension::GetCachedImage(const ExtensionResource& source,
                                   const gfx::Size& max_size) const {
  DCHECK(source.extension_root() == path());  // The resource must come from
                                              // this extension.
  SkBitmap* image = GetCachedImageImpl(source, max_size);
  return image ? *image : SkBitmap();
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

  if (document_url.SchemeIs(chrome::kChromeUIScheme) &&
      !CanExecuteScriptEverywhere()) {
    return false;
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
  if (location() == Extension::COMPONENT)
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
  if (!plugins().empty()) {
    return SYNC_TYPE_NONE;
  }

  switch (GetType()) {
    case Extension::TYPE_EXTENSION:
      return SYNC_TYPE_EXTENSION;

    case Extension::TYPE_USER_SCRIPT:
      // We only want to sync user scripts with gallery update URLs.
      if (UpdatesFromGallery())
        return SYNC_TYPE_EXTENSION;
      else
        return SYNC_TYPE_NONE;

    case Extension::TYPE_HOSTED_APP:
    case Extension::TYPE_LEGACY_PACKAGED_APP:
    case Extension::TYPE_PLATFORM_APP:
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
  bool is_syncable = (location() == Extension::INTERNAL &&
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
  if (location() == Extension::COMPONENT &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kShowComponentExtensionOptions)) {
    return false;
  }

  // Always show unpacked extensions and apps.
  if (location() == Extension::LOAD)
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

bool Extension::HasContentScriptAtURL(const GURL& url) const {
  for (UserScriptList::const_iterator it = content_scripts_.begin();
      it != content_scripts_.end(); ++it) {
    if (it->MatchesURL(url))
      return true;
  }
  return false;
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

Extension::Location Extension::location() const {
  return manifest_->location();
}

const std::string& Extension::id() const {
  return manifest_->extension_id();
}

const std::string Extension::VersionString() const {
  return version()->GetString();
}

void Extension::AddInstallWarnings(
    const InstallWarningVector& new_warnings) {
  install_warnings_.insert(install_warnings_.end(),
                           new_warnings.begin(), new_warnings.end());
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

ExtensionResource Extension::GetContentPackSiteList() const {
  if (content_pack_site_list_.empty())
    return ExtensionResource();

  return GetResource(content_pack_site_list_);
}

GURL Extension::GetBackgroundURL() const {
  if (background_scripts_.empty())
    return background_url_;
  return GetResourceURL(extension_filenames::kGeneratedBackgroundPageFilename);
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
                                const FilePath& path,
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
    std::string extension_id;
    if (!manifest->GetString(keys::kPublicKey, &public_key) ||
        !ParsePEMKeyBytes(public_key, &public_key_bytes) ||
        !GenerateId(public_key_bytes, &extension_id)) {
      *error = ASCIIToUTF16(errors::kInvalidKey);
      return false;
    }
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
    std::string extension_id = GenerateIdForPath(path);
    if (extension_id.empty()) {
      NOTREACHED() << "Could not create ID from path.";
      return false;
    }
    manifest->set_extension_id(extension_id);
    return true;
  }
}

// static
FilePath Extension::MaybeNormalizePath(const FilePath& path) {
#if defined(OS_WIN)
  // Normalize any drive letter to upper-case. We do this for consistency with
  // net_utils::FilePathToFileURL(), which does the same thing, to make string
  // comparisons simpler.
  std::wstring path_str = path.value();
  if (path_str.size() >= 2 && path_str[0] >= L'a' && path_str[0] <= L'z' &&
      path_str[1] == ':')
    path_str[0] += ('A' - 'a');

  return FilePath(path_str);
#else
  return path;
#endif
}

bool Extension::LoadManagedModeFeatures(string16* error) {
  if (!manifest_->HasKey(keys::kContentPack))
    return true;
  DictionaryValue* content_pack_value = NULL;
  if (!manifest_->GetDictionary(keys::kContentPack, &content_pack_value)) {
    *error = ASCIIToUTF16(errors::kInvalidContentPack);
    return false;
  }

  if (!LoadManagedModeSites(content_pack_value, error))
    return false;
  if (!LoadManagedModeConfigurations(content_pack_value, error))
    return false;

  return true;
}

bool Extension::LoadManagedModeSites(
    const DictionaryValue* content_pack_value,
    string16* error) {
  if (!content_pack_value->HasKey(keys::kContentPackSites))
    return true;

  FilePath::StringType site_list_str;
  if (!content_pack_value->GetString(keys::kContentPackSites, &site_list_str)) {
    *error = ASCIIToUTF16(errors::kInvalidContentPackSites);
    return false;
  }

  content_pack_site_list_ = FilePath(site_list_str);

  return true;
}

bool Extension::LoadManagedModeConfigurations(
    const DictionaryValue* content_pack_value,
    string16* error) {
  NOTIMPLEMENTED();
  return true;
}

// static
bool Extension::IsTrustedId(const std::string& id) {
  // See http://b/4946060 for more details.
  return id == std::string("nckgahadagoaajjgafhacjanaoiihapd");
}

Extension::Extension(const FilePath& path,
                     scoped_ptr<extensions::Manifest> manifest)
    : manifest_version_(0),
      incognito_split_mode_(false),
      offline_enabled_(false),
      converted_from_user_script_(false),
      background_page_is_persistent_(true),
      allow_background_js_access_(true),
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
  path_ = MaybeNormalizePath(path);
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

  APIPermissionSet api_permissions;
  URLPatternSet host_permissions;
  if (!ParsePermissions(keys::kPermissions,
                        error,
                        &api_permissions,
                        &host_permissions)) {
    return false;
  }

  // Check for any permissions that are optional only.
  for (APIPermissionSet::const_iterator i = api_permissions.begin();
      i != api_permissions.end(); ++i) {
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
    api_permissions.insert(APIPermission::kAppCurrentWindowInternal);
    api_permissions.insert(APIPermission::kAppRuntime);
    api_permissions.insert(APIPermission::kAppWindow);
  }

  if (from_webstore()) {
    details_url_ =
        GURL(extension_urls::GetWebstoreItemDetailURLPrefix() + id());
  }

  APIPermissionSet optional_api_permissions;
  URLPatternSet optional_host_permissions;
  if (!ParsePermissions(keys::kOptionalPermissions,
                        error,
                        &optional_api_permissions,
                        &optional_host_permissions)) {
    return false;
  }

  if (ContainsManifestForbiddenPermission(api_permissions, error) ||
      ContainsManifestForbiddenPermission(optional_api_permissions, error)) {
    return false;
  }

  if (!LoadAppIsolation(api_permissions, error))
    return false;

  if (!LoadSharedFeatures(api_permissions, error))
    return false;

  if (!LoadExtensionFeatures(&api_permissions, error))
    return false;

  if (!LoadThemeFeatures(error))
    return false;

  if (!LoadManagedModeFeatures(error))
    return false;

  if (HasMultipleUISurfaces()) {
    *error = ASCIIToUTF16(errors::kOneUISurfaceOnly);
    return false;
  }

  finished_parsing_manifest_ = true;

  runtime_data_.SetActivePermissions(new PermissionSet(
      this, api_permissions, host_permissions));
  required_permission_set_ = new PermissionSet(
      this, api_permissions, host_permissions);
  optional_permission_set_ = new PermissionSet(
      optional_api_permissions, optional_host_permissions, URLPatternSet());

  return true;
}

bool Extension::LoadAppIsolation(const APIPermissionSet& api_permissions,
                                 string16* error) {
  // Platform apps always get isolated storage.
  if (is_platform_app()) {
    is_storage_isolated_ = true;
    return true;
  }

  // Other apps only get it if it is requested _and_ experimental APIs are
  // enabled.
  if (!api_permissions.count(APIPermission::kExperimental) || !is_app())
    return true;

  Value* tmp_isolation = NULL;
  if (!manifest_->Get(keys::kIsolation, &tmp_isolation))
    return true;

  if (tmp_isolation->GetType() != Value::TYPE_LIST) {
    *error = ASCIIToUTF16(errors::kInvalidIsolation);
    return false;
  }

  ListValue* isolation_list = static_cast<ListValue*>(tmp_isolation);
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
  Value* temp_pattern_value = NULL;
  if (!manifest_->Get(key, &temp_pattern_value))
    return true;

  if (temp_pattern_value->GetType() != Value::TYPE_LIST) {
    *error = ASCIIToUTF16(list_error);
    return false;
  }

  ListValue* pattern_list = static_cast<ListValue*>(temp_pattern_value);
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
  Value* tmp_launcher_container = NULL;
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
  Value* temp = NULL;

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

bool Extension::LoadSharedFeatures(
    const APIPermissionSet& api_permissions,
    string16* error) {
  if (!LoadDescription(error) ||
      !LoadIcons(error) ||
      !LoadCommands(error) ||
      !LoadPlugins(error) ||
      !LoadNaClModules(error) ||
      !LoadSandboxedPages(error) ||
      !LoadRequirements(error) ||
      !LoadDefaultLocale(error) ||
      !LoadOfflineEnabled(error) ||
      // LoadBackgroundScripts() must be called before LoadBackgroundPage().
      !LoadBackgroundScripts(error) ||
      !LoadBackgroundPage(api_permissions, error) ||
      !LoadBackgroundPersistent(api_permissions, error) ||
      !LoadBackgroundAllowJSAccess(api_permissions, error) ||
      !LoadOAuth2Info(error))
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

bool Extension::LoadIcons(string16* error) {
  if (!manifest_->HasKey(keys::kIcons))
    return true;
  DictionaryValue* icons_value = NULL;
  if (!manifest_->GetDictionary(keys::kIcons, &icons_value)) {
    *error = ASCIIToUTF16(errors::kInvalidIcons);
    return false;
  }

  return LoadIconsFromDictionary(icons_value,
                                 extension_misc::kExtensionIconSizes,
                                 extension_misc::kNumExtensionIconSizes,
                                 &icons_,
                                 error);
}

bool Extension::LoadCommands(string16* error) {
  if (manifest_->HasKey(keys::kCommands)) {
    DictionaryValue* commands = NULL;
    if (!manifest_->GetDictionary(keys::kCommands, &commands)) {
      *error = ASCIIToUTF16(errors::kInvalidCommandsKey);
      return false;
    }

    if (commands->size() > kMaxCommandsPerExtension) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidKeyBindingTooMany,
          base::IntToString(kMaxCommandsPerExtension));
      return false;
    }

    int command_index = 0;
    for (DictionaryValue::key_iterator iter = commands->begin_keys();
         iter != commands->end_keys(); ++iter) {
      ++command_index;

      DictionaryValue* command = NULL;
      if (!commands->GetDictionary(*iter, &command)) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidKeyBindingDictionary,
            base::IntToString(command_index));
        return false;
      }

      scoped_ptr<extensions::Command> binding(new extensions::Command());
      if (!binding->Parse(command, *iter, command_index, error))
        return false;  // |error| already set.

      std::string command_name = binding->command_name();
      if (command_name == values::kPageActionCommandEvent) {
        page_action_command_.reset(binding.release());
      } else if (command_name == values::kBrowserActionCommandEvent) {
        browser_action_command_.reset(binding.release());
      } else if (command_name == values::kScriptBadgeCommandEvent) {
        script_badge_command_.reset(binding.release());
      } else {
        if (command_name[0] != '_')  // All commands w/underscore are reserved.
          named_commands_[command_name] = *binding.get();
      }
    }
  }

  if (manifest_->HasKey(keys::kBrowserAction) &&
      !browser_action_command_.get()) {
    // If the extension defines a browser action, but no command for it, then
    // we synthesize a generic one, so the user can configure a shortcut for it.
    // No keyboard shortcut will be assigned to it, until the user selects one.
    browser_action_command_.reset(
        new extensions::Command(
            values::kBrowserActionCommandEvent, string16(), ""));
  }

  return true;
}

bool Extension::LoadPlugins(string16* error) {
  if (!manifest_->HasKey(keys::kPlugins))
    return true;

  ListValue* list_value = NULL;
  if (!manifest_->GetList(keys::kPlugins, &list_value)) {
    *error = ASCIIToUTF16(errors::kInvalidPlugins);
    return false;
  }

  for (size_t i = 0; i < list_value->GetSize(); ++i) {
    DictionaryValue* plugin_value = NULL;
    if (!list_value->GetDictionary(i, &plugin_value)) {
      *error = ASCIIToUTF16(errors::kInvalidPlugins);
      return false;
    }
    // Get plugins[i].path.
    std::string path_str;
    if (!plugin_value->GetString(keys::kPluginsPath, &path_str)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidPluginsPath, base::IntToString(i));
      return false;
    }

    // Get plugins[i].content (optional).
    bool is_public = false;
    if (plugin_value->HasKey(keys::kPluginsPublic)) {
      if (!plugin_value->GetBoolean(keys::kPluginsPublic, &is_public)) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidPluginsPublic, base::IntToString(i));
        return false;
      }
    }

    // We don't allow extensions to load NPAPI plugins on Chrome OS, or under
    // Windows 8 Metro mode, but still parse the entries to display consistent
    // error messages. If the extension actually requires the plugins then
    // LoadRequirements will prevent it loading.
#if defined(OS_CHROMEOS)
    continue;
#elif defined(OS_WIN)
    if (base::win::IsMetroProcess()) {
      continue;
    }
#endif  // defined(OS_WIN).
    plugins_.push_back(PluginInfo());
    plugins_.back().path = path().Append(FilePath::FromUTF8Unsafe(path_str));
    plugins_.back().is_public = is_public;
  }
  return true;
}

bool Extension::LoadNaClModules(string16* error) {
  if (!manifest_->HasKey(keys::kNaClModules))
    return true;
  ListValue* list_value = NULL;
  if (!manifest_->GetList(keys::kNaClModules, &list_value)) {
    *error = ASCIIToUTF16(errors::kInvalidNaClModules);
    return false;
  }

  for (size_t i = 0; i < list_value->GetSize(); ++i) {
    DictionaryValue* module_value = NULL;
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

bool Extension::LoadSandboxedPages(string16* error) {
  if (!manifest_->HasPath(keys::kSandboxedPages))
    return true;

  ListValue* list_value = NULL;
  if (!manifest_->GetList(keys::kSandboxedPages, &list_value)) {
    *error = ASCIIToUTF16(errors::kInvalidSandboxedPagesList);
    return false;
  }
  for (size_t i = 0; i < list_value->GetSize(); ++i) {
    std::string relative_path;
    if (!list_value->GetString(i, &relative_path)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidSandboxedPage, base::IntToString(i));
      return false;
    }
    URLPattern pattern(URLPattern::SCHEME_EXTENSION);
    if (pattern.Parse(extension_url_.spec()) != URLPattern::PARSE_SUCCESS) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidURLPatternError, extension_url_.spec());
      return false;
    }
    while (relative_path[0] == '/')
      relative_path = relative_path.substr(1, relative_path.length() - 1);
    pattern.SetPath(pattern.path() + relative_path);
    sandboxed_pages_.AddPattern(pattern);
  }

  if (manifest_->HasPath(keys::kSandboxedPagesCSP)) {
    if (!manifest_->GetString(
        keys::kSandboxedPagesCSP, &sandboxed_pages_content_security_policy_)) {
      *error = ASCIIToUTF16(errors::kInvalidSandboxedPagesCSP);
      return false;
    }

    if (!ContentSecurityPolicyIsLegal(
            sandboxed_pages_content_security_policy_) ||
        !ContentSecurityPolicyIsSandboxed(
            sandboxed_pages_content_security_policy_, GetType())) {
      *error = ASCIIToUTF16(errors::kInvalidSandboxedPagesCSP);
      return false;
    }
  } else {
    sandboxed_pages_content_security_policy_ =
        kDefaultSandboxedPageContentSecurityPolicy;
    CHECK(ContentSecurityPolicyIsSandboxed(
        sandboxed_pages_content_security_policy_, GetType()));
  }

  return true;
}

bool Extension::LoadRequirements(string16* error) {
  // Before parsing requirements from the manifest, automatically default the
  // NPAPI plugin requirement based on whether it includes NPAPI plugins.
  ListValue* list_value = NULL;
  requirements_.npapi =
    manifest_->GetList(keys::kPlugins, &list_value) && !list_value->empty();

  if (!manifest_->HasKey(keys::kRequirements))
    return true;

  DictionaryValue* requirements_value = NULL;
  if (!manifest_->GetDictionary(keys::kRequirements, &requirements_value)) {
    *error = ASCIIToUTF16(errors::kInvalidRequirements);
    return false;
  }

  for (DictionaryValue::key_iterator it = requirements_value->begin_keys();
       it != requirements_value->end_keys(); ++it) {
    DictionaryValue* requirement_value;
    if (!requirements_value->GetDictionaryWithoutPathExpansion(
        *it, &requirement_value)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidRequirement, *it);
      return false;
    }

    if (*it == "plugins") {
      for (DictionaryValue::key_iterator plugin_it =
              requirement_value->begin_keys();
           plugin_it != requirement_value->end_keys(); ++plugin_it) {
        bool plugin_required = false;
        if (!requirement_value->GetBoolean(*plugin_it, &plugin_required)) {
          *error = ErrorUtils::FormatErrorMessageUTF16(
              errors::kInvalidRequirement, *it);
          return false;
        }
        if (*plugin_it == "npapi") {
          requirements_.npapi = plugin_required;
        } else {
          *error = ErrorUtils::FormatErrorMessageUTF16(
              errors::kInvalidRequirement, *it);
          return false;
        }
      }
    } else if (*it == "3D") {
      ListValue* features = NULL;
      if (!requirement_value->GetListWithoutPathExpansion("features",
                                                          &features) ||
          !features) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidRequirement, *it);
        return false;
      }

      for (base::ListValue::iterator feature_it = features->begin();
           feature_it != features->end();
           ++feature_it) {
        std::string feature;
        if ((*feature_it)->GetAsString(&feature)) {
          if (feature == "webgl") {
            requirements_.webgl = true;
          } else if (feature == "css3d") {
            requirements_.css3d = true;
          } else {
            *error = ErrorUtils::FormatErrorMessageUTF16(
                errors::kInvalidRequirement, *it);
            return false;
          }
        }
      }
    } else {
      *error = ASCIIToUTF16(errors::kInvalidRequirements);
      return false;
    }
  }
  return true;
}

bool Extension::LoadDefaultLocale(string16* error) {
  if (!manifest_->HasKey(keys::kDefaultLocale))
    return true;
  if (!manifest_->GetString(keys::kDefaultLocale, &default_locale_) ||
      !l10n_util::IsValidLocaleSyntax(default_locale_)) {
    *error = ASCIIToUTF16(errors::kInvalidDefaultLocale);
    return false;
  }
  return true;
}

bool Extension::LoadOfflineEnabled(string16* error) {
  // Defaults to false, except for platform apps which are offline by default.
  if (!manifest_->HasKey(keys::kOfflineEnabled)) {
    offline_enabled_ = is_platform_app();
    return true;
  }
  if (!manifest_->GetBoolean(keys::kOfflineEnabled, &offline_enabled_)) {
    *error = ASCIIToUTF16(errors::kInvalidOfflineEnabled);
    return false;
  }
  return true;
}

bool Extension::LoadBackgroundScripts(string16* error) {
  const std::string& key = is_platform_app() ?
      keys::kPlatformAppBackgroundScripts : keys::kBackgroundScripts;
  return LoadBackgroundScripts(key, error);
}

bool Extension::LoadBackgroundScripts(const std::string& key, string16* error) {
  Value* background_scripts_value = NULL;
  if (!manifest_->Get(key, &background_scripts_value))
    return true;

  CHECK(background_scripts_value);
  if (background_scripts_value->GetType() != Value::TYPE_LIST) {
    *error = ASCIIToUTF16(errors::kInvalidBackgroundScripts);
    return false;
  }

  ListValue* background_scripts =
      static_cast<ListValue*>(background_scripts_value);
  for (size_t i = 0; i < background_scripts->GetSize(); ++i) {
    std::string script;
    if (!background_scripts->GetString(i, &script)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidBackgroundScript, base::IntToString(i));
      return false;
    }
    background_scripts_.push_back(script);
  }

  return true;
}

bool Extension::LoadBackgroundPage(
    const APIPermissionSet& api_permissions,
    string16* error) {
  if (is_platform_app()) {
    return LoadBackgroundPage(
        keys::kPlatformAppBackgroundPage, api_permissions, error);
  }

  if (!LoadBackgroundPage(keys::kBackgroundPage, api_permissions, error))
    return false;
  if (background_url_.is_empty()) {
    return LoadBackgroundPage(
        keys::kBackgroundPageLegacy, api_permissions, error);
  }
  return true;
}

bool Extension::LoadBackgroundPage(
    const std::string& key,
    const APIPermissionSet& api_permissions,
    string16* error) {
  base::Value* background_page_value = NULL;
  if (!manifest_->Get(key, &background_page_value))
    return true;

  if (!background_scripts_.empty()) {
    *error = ASCIIToUTF16(errors::kInvalidBackgroundCombination);
    return false;
  }


  std::string background_str;
  if (!background_page_value->GetAsString(&background_str)) {
    *error = ASCIIToUTF16(errors::kInvalidBackground);
    return false;
  }

  if (is_hosted_app()) {
    background_url_ = GURL(background_str);

    // Make sure "background" permission is set.
    if (!api_permissions.count(APIPermission::kBackground)) {
      *error = ASCIIToUTF16(errors::kBackgroundPermissionNeeded);
      return false;
    }
    // Hosted apps require an absolute URL.
    if (!background_url_.is_valid()) {
      *error = ASCIIToUTF16(errors::kInvalidBackgroundInHostedApp);
      return false;
    }

    if (!(background_url_.SchemeIs("https") ||
          (CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kAllowHTTPBackgroundPage) &&
           background_url_.SchemeIs("http")))) {
      *error = ASCIIToUTF16(errors::kInvalidBackgroundInHostedApp);
      return false;
    }
  } else {
    background_url_ = GetResourceURL(background_str);
  }

  return true;
}

bool Extension::LoadBackgroundPersistent(
    const APIPermissionSet& api_permissions,
    string16* error) {
  if (is_platform_app()) {
    background_page_is_persistent_ = false;
    return true;
  }

  Value* background_persistent = NULL;
  if (!manifest_->Get(keys::kBackgroundPersistent, &background_persistent))
    return true;

  if (!background_persistent->GetAsBoolean(&background_page_is_persistent_)) {
    *error = ASCIIToUTF16(errors::kInvalidBackgroundPersistent);
    return false;
  }

  if (!has_background_page()) {
    *error = ASCIIToUTF16(errors::kInvalidBackgroundPersistentNoPage);
    return false;
  }

  return true;
}

bool Extension::LoadBackgroundAllowJSAccess(
    const APIPermissionSet& api_permissions,
    string16* error) {
  Value* allow_js_access = NULL;
  if (!manifest_->Get(keys::kBackgroundAllowJsAccess, &allow_js_access))
    return true;

  if (!allow_js_access->IsType(Value::TYPE_BOOLEAN) ||
      !allow_js_access->GetAsBoolean(&allow_background_js_access_)) {
    *error = ASCIIToUTF16(errors::kInvalidBackgroundAllowJsAccess);
    return false;
  }

  return true;
}

bool Extension::LoadExtensionFeatures(APIPermissionSet* api_permissions,
                                      string16* error) {
  if (manifest_->HasKey(keys::kConvertedFromUserScript))
    manifest_->GetBoolean(keys::kConvertedFromUserScript,
                          &converted_from_user_script_);

  if (!LoadManifestHandlerFeatures(error) ||
      !LoadContentScripts(error) ||
      !LoadPageAction(error) ||
      !LoadBrowserAction(error) ||
      !LoadSystemIndicator(api_permissions, error) ||
      !LoadScriptBadge(error) ||
      !LoadIncognitoMode(error) ||
      !LoadContentSecurityPolicy(error))
    return false;

  return true;
}

bool Extension::LoadManifestHandlerFeatures(string16* error) {
  std::vector<std::string> keys = ManifestHandler::GetKeys();
  for (size_t i = 0; i < keys.size(); ++i) {
    Value* value = NULL;
    if (!manifest_->Get(keys[i], &value)) {
      if (!ManifestHandler::Get(keys[i])->HasNoKey(this, error))
        return false;
    } else if (!ManifestHandler::Get(keys[i])->Parse(value, this, error)) {
      return false;
    }
  }
  return true;
}

bool Extension::LoadContentScripts(string16* error) {
  if (!manifest_->HasKey(keys::kContentScripts))
    return true;
  ListValue* list_value;
  if (!manifest_->GetList(keys::kContentScripts, &list_value)) {
    *error = ASCIIToUTF16(errors::kInvalidContentScriptsList);
    return false;
  }

  for (size_t i = 0; i < list_value->GetSize(); ++i) {
    DictionaryValue* content_script = NULL;
    if (!list_value->GetDictionary(i, &content_script)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidContentScript, base::IntToString(i));
      return false;
    }

    UserScript script;
    if (!LoadUserScriptHelper(content_script, i, error, &script))
      return false;  // Failed to parse script context definition.
    script.set_extension_id(id());
    if (converted_from_user_script_) {
      script.set_emulate_greasemonkey(true);
      script.set_match_all_frames(true);  // Greasemonkey matches all frames.
    }
    content_scripts_.push_back(script);
  }
  return true;
}

bool Extension::LoadPageAction(string16* error) {
  DictionaryValue* page_action_value = NULL;

  if (manifest_->HasKey(keys::kPageActions)) {
    ListValue* list_value = NULL;
    if (!manifest_->GetList(keys::kPageActions, &list_value)) {
      *error = ASCIIToUTF16(errors::kInvalidPageActionsList);
      return false;
    }

    size_t list_value_length = list_value->GetSize();

    if (list_value_length == 0u) {
      // A list with zero items is allowed, and is equivalent to not having
      // a page_actions key in the manifest.  Don't set |page_action_value|.
    } else if (list_value_length == 1u) {
      if (!list_value->GetDictionary(0, &page_action_value)) {
        *error = ASCIIToUTF16(errors::kInvalidPageAction);
        return false;
      }
    } else {  // list_value_length > 1u.
      *error = ASCIIToUTF16(errors::kInvalidPageActionsListSize);
      return false;
    }
  } else if (manifest_->HasKey(keys::kPageAction)) {
    if (!manifest_->GetDictionary(keys::kPageAction, &page_action_value)) {
      *error = ASCIIToUTF16(errors::kInvalidPageAction);
      return false;
    }
  }

  // If page_action_value is not NULL, then there was a valid page action.
  if (page_action_value) {
    page_action_info_ = LoadExtensionActionInfoHelper(
        page_action_value, Extension::ActionInfo::TYPE_PAGE, error);
    if (!page_action_info_.get())
      return false;  // Failed to parse page action definition.
  }

  return true;
}

bool Extension::LoadBrowserAction(string16* error) {
  if (!manifest_->HasKey(keys::kBrowserAction))
    return true;
  DictionaryValue* browser_action_value = NULL;
  if (!manifest_->GetDictionary(keys::kBrowserAction, &browser_action_value)) {
    *error = ASCIIToUTF16(errors::kInvalidBrowserAction);
    return false;
  }

  browser_action_info_ = LoadExtensionActionInfoHelper(
      browser_action_value, Extension::ActionInfo::TYPE_BROWSER, error);
  if (!browser_action_info_.get())
    return false;  // Failed to parse browser action definition.
  return true;
}

bool Extension::LoadScriptBadge(string16* error) {
  if (manifest_->HasKey(keys::kScriptBadge)) {
    if (!FeatureSwitch::script_badges()->IsEnabled()) {
      // So as to not confuse developers if they specify a script badge section
      // in the manifest, show a warning if the script badge declaration isn't
      // going to have any effect.
      install_warnings_.push_back(
          InstallWarning(InstallWarning::FORMAT_TEXT,
                         errors::kScriptBadgeRequiresFlag));
    }

    DictionaryValue* script_badge_value = NULL;
    if (!manifest_->GetDictionary(keys::kScriptBadge, &script_badge_value)) {
      *error = ASCIIToUTF16(errors::kInvalidScriptBadge);
      return false;
    }

    script_badge_info_ = LoadExtensionActionInfoHelper(
        script_badge_value, Extension::ActionInfo::TYPE_SCRIPT_BADGE, error);
    if (!script_badge_info_.get())
      return false;  // Failed to parse script badge definition.
  } else {
    script_badge_info_.reset(new ActionInfo());
  }

  // Script badges always use their extension's title and icon so users can rely
  // on the visual appearance to know which extension is running.  This isn't
  // bulletproof since an malicious extension could use a different 16x16 icon
  // that matches the icon of a trusted extension, and users wouldn't be warned
  // during installation.

  if (!script_badge_info_->default_title.empty()) {
    install_warnings_.push_back(
        InstallWarning(InstallWarning::FORMAT_TEXT,
                       errors::kScriptBadgeTitleIgnored));
  }
  script_badge_info_->default_title = name();

  if (!script_badge_info_->default_icon.empty()) {
    install_warnings_.push_back(
        InstallWarning(InstallWarning::FORMAT_TEXT,
                       errors::kScriptBadgeIconIgnored));
  }

  script_badge_info_->default_icon.Clear();
  for (size_t i = 0; i < extension_misc::kNumScriptBadgeIconSizes; i++) {
    std::string path = icons().Get(extension_misc::kScriptBadgeIconSizes[i],
                                   ExtensionIconSet::MATCH_BIGGER);
    if (!path.empty())
      script_badge_info_->default_icon.Add(
          extension_misc::kScriptBadgeIconSizes[i], path);
  }

  return true;
}

bool Extension::LoadSystemIndicator(APIPermissionSet* api_permissions,
                                    string16* error) {
  if (!manifest_->HasKey(keys::kSystemIndicator)) {
    // There was no manifest entry for the system indicator.
    return true;
  }

  DictionaryValue* system_indicator_value = NULL;
  if (!manifest_->GetDictionary(keys::kSystemIndicator,
                                &system_indicator_value)) {
    *error = ASCIIToUTF16(errors::kInvalidSystemIndicator);
    return false;
  }

  system_indicator_info_ = LoadExtensionActionInfoHelper(
      system_indicator_value,
      Extension::ActionInfo::TYPE_SYSTEM_INDICATOR,
      error);

  if (!system_indicator_info_.get()) {
    return false;
  }

  // Because the manifest was successfully parsed, auto-grant the permission.
  // TODO(dewittj) Add this for all extension action APIs.
  api_permissions->insert(APIPermission::kSystemIndicator);

  return true;
}

bool Extension::LoadIncognitoMode(string16* error) {
  // Apps default to split mode, extensions default to spanning.
  incognito_split_mode_ = is_app();
  if (!manifest_->HasKey(keys::kIncognito))
    return true;
  std::string value;
  if (!manifest_->GetString(keys::kIncognito, &value)) {
    *error = ASCIIToUTF16(errors::kInvalidIncognitoBehavior);
    return false;
  }
  if (value == values::kIncognitoSpanning) {
    incognito_split_mode_ = false;
  } else if (value == values::kIncognitoSplit) {
    incognito_split_mode_ = true;
  } else {
    *error = ASCIIToUTF16(errors::kInvalidIncognitoBehavior);
    return false;
  }
  return true;
}

bool Extension::LoadContentSecurityPolicy(string16* error) {
  const std::string& key = is_platform_app() ?
      keys::kPlatformAppContentSecurityPolicy : keys::kContentSecurityPolicy;

  if (manifest_->HasPath(key)) {
    std::string content_security_policy;
    if (!manifest_->GetString(key, &content_security_policy)) {
      *error = ASCIIToUTF16(errors::kInvalidContentSecurityPolicy);
      return false;
    }
    if (!ContentSecurityPolicyIsLegal(content_security_policy)) {
      *error = ASCIIToUTF16(errors::kInvalidContentSecurityPolicy);
      return false;
    }
    if (manifest_version_ >= 2 &&
        !ContentSecurityPolicyIsSecure(content_security_policy, GetType())) {
      *error = ASCIIToUTF16(errors::kInsecureContentSecurityPolicy);
      return false;
    }

    content_security_policy_ = content_security_policy;
  } else if (manifest_version_ >= 2) {
    // Manifest version 2 introduced a default Content-Security-Policy.
    // TODO(abarth): Should we continue to let extensions override the
    //               default Content-Security-Policy?
    content_security_policy_ = is_platform_app() ?
        kDefaultPlatformAppContentSecurityPolicy :
        kDefaultContentSecurityPolicy;
    CHECK(ContentSecurityPolicyIsSecure(content_security_policy_, GetType()));
  }
  return true;
}

bool Extension::LoadThemeFeatures(string16* error) {
  if (!manifest_->HasKey(keys::kTheme))
    return true;
  DictionaryValue* theme_value = NULL;
  if (!manifest_->GetDictionary(keys::kTheme, &theme_value)) {
    *error = ASCIIToUTF16(errors::kInvalidTheme);
    return false;
  }
  if (!LoadThemeImages(theme_value, error))
    return false;
  if (!LoadThemeColors(theme_value, error))
    return false;
  if (!LoadThemeTints(theme_value, error))
    return false;
  if (!LoadThemeDisplayProperties(theme_value, error))
    return false;

  return true;
}

bool Extension::LoadThemeImages(const DictionaryValue* theme_value,
                                string16* error) {
  const DictionaryValue* images_value = NULL;
  if (theme_value->GetDictionary(keys::kThemeImages, &images_value)) {
    // Validate that the images are all strings
    for (DictionaryValue::key_iterator iter = images_value->begin_keys();
         iter != images_value->end_keys(); ++iter) {
      std::string val;
      if (!images_value->GetString(*iter, &val)) {
        *error = ASCIIToUTF16(errors::kInvalidThemeImages);
        return false;
      }
    }
    theme_images_.reset(images_value->DeepCopy());
  }
  return true;
}

bool Extension::LoadThemeColors(const DictionaryValue* theme_value,
                                string16* error) {
  const DictionaryValue* colors_value = NULL;
  if (theme_value->GetDictionary(keys::kThemeColors, &colors_value)) {
    // Validate that the colors are RGB or RGBA lists
    for (DictionaryValue::key_iterator iter = colors_value->begin_keys();
         iter != colors_value->end_keys(); ++iter) {
      const ListValue* color_list = NULL;
      double alpha = 0.0;
      int color = 0;
      // The color must be a list
      if (!colors_value->GetListWithoutPathExpansion(*iter, &color_list) ||
          // And either 3 items (RGB) or 4 (RGBA)
          ((color_list->GetSize() != 3) &&
           ((color_list->GetSize() != 4) ||
            // For RGBA, the fourth item must be a real or int alpha value.
            // Note that GetDouble() can get an integer value.
            !color_list->GetDouble(3, &alpha))) ||
          // For both RGB and RGBA, the first three items must be ints (R,G,B)
          !color_list->GetInteger(0, &color) ||
          !color_list->GetInteger(1, &color) ||
          !color_list->GetInteger(2, &color)) {
        *error = ASCIIToUTF16(errors::kInvalidThemeColors);
        return false;
      }
    }
    theme_colors_.reset(colors_value->DeepCopy());
  }
  return true;
}

bool Extension::LoadThemeTints(const DictionaryValue* theme_value,
                               string16* error) {
  const DictionaryValue* tints_value = NULL;
  if (!theme_value->GetDictionary(keys::kThemeTints, &tints_value))
    return true;

  // Validate that the tints are all reals.
  for (DictionaryValue::key_iterator iter = tints_value->begin_keys();
       iter != tints_value->end_keys(); ++iter) {
    const ListValue* tint_list = NULL;
    double v = 0.0;
    if (!tints_value->GetListWithoutPathExpansion(*iter, &tint_list) ||
        tint_list->GetSize() != 3 ||
        !tint_list->GetDouble(0, &v) ||
        !tint_list->GetDouble(1, &v) ||
        !tint_list->GetDouble(2, &v)) {
      *error = ASCIIToUTF16(errors::kInvalidThemeTints);
      return false;
    }
  }
  theme_tints_.reset(tints_value->DeepCopy());
  return true;
}

bool Extension::LoadThemeDisplayProperties(const DictionaryValue* theme_value,
                                           string16* error) {
  const DictionaryValue* display_properties_value = NULL;
  if (theme_value->GetDictionary(keys::kThemeDisplayProperties,
                                 &display_properties_value)) {
    theme_display_properties_.reset(
        display_properties_value->DeepCopy());
  }
  return true;
}
SkBitmap* Extension::GetCachedImageImpl(const ExtensionResource& source,
                                        const gfx::Size& max_size) const {
  const FilePath& path = source.relative_path();

  // Look for exact size match.
  ImageCache::iterator i = image_cache_.find(
      ImageCacheKey(path, SizeToString(max_size)));
  if (i != image_cache_.end())
    return &(i->second);

  // If we have the original size version cached, return that if it's small
  // enough.
  i = image_cache_.find(ImageCacheKey(path, std::string()));
  if (i != image_cache_.end()) {
    const SkBitmap& image = i->second;
    if (image.width() <= max_size.width() &&
        image.height() <= max_size.height()) {
      return &(i->second);
    }
  }

  return NULL;
}

// Helper method that loads a UserScript object from a dictionary in the
// content_script list of the manifest.
bool Extension::LoadUserScriptHelper(const DictionaryValue* content_script,
                                     int definition_index,
                                     string16* error,
                                     UserScript* result) {
  // run_at
  if (content_script->HasKey(keys::kRunAt)) {
    std::string run_location;
    if (!content_script->GetString(keys::kRunAt, &run_location)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidRunAt,
          base::IntToString(definition_index));
      return false;
    }

    if (run_location == values::kRunAtDocumentStart) {
      result->set_run_location(UserScript::DOCUMENT_START);
    } else if (run_location == values::kRunAtDocumentEnd) {
      result->set_run_location(UserScript::DOCUMENT_END);
    } else if (run_location == values::kRunAtDocumentIdle) {
      result->set_run_location(UserScript::DOCUMENT_IDLE);
    } else {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidRunAt,
          base::IntToString(definition_index));
      return false;
    }
  }

  // all frames
  if (content_script->HasKey(keys::kAllFrames)) {
    bool all_frames = false;
    if (!content_script->GetBoolean(keys::kAllFrames, &all_frames)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidAllFrames, base::IntToString(definition_index));
      return false;
    }
    result->set_match_all_frames(all_frames);
  }

  // matches (required)
  const ListValue* matches = NULL;
  if (!content_script->GetList(keys::kMatches, &matches)) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidMatches,
        base::IntToString(definition_index));
    return false;
  }

  if (matches->GetSize() == 0) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidMatchCount,
        base::IntToString(definition_index));
    return false;
  }
  for (size_t j = 0; j < matches->GetSize(); ++j) {
    std::string match_str;
    if (!matches->GetString(j, &match_str)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidMatch,
          base::IntToString(definition_index),
          base::IntToString(j),
          errors::kExpectString);
      return false;
    }

    URLPattern pattern(UserScript::kValidUserScriptSchemes);
    if (CanExecuteScriptEverywhere())
      pattern.SetValidSchemes(URLPattern::SCHEME_ALL);

    URLPattern::ParseResult parse_result = pattern.Parse(match_str);
    if (parse_result != URLPattern::PARSE_SUCCESS) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidMatch,
          base::IntToString(definition_index),
          base::IntToString(j),
          URLPattern::GetParseResultString(parse_result));
      return false;
    }

    if (pattern.MatchesScheme(chrome::kFileScheme) &&
        !CanExecuteScriptEverywhere()) {
      wants_file_access_ = true;
      if (!(creation_flags_ & ALLOW_FILE_ACCESS)) {
        pattern.SetValidSchemes(
            pattern.valid_schemes() & ~URLPattern::SCHEME_FILE);
      }
    }

    result->add_url_pattern(pattern);
  }

  // exclude_matches
  if (content_script->HasKey(keys::kExcludeMatches)) {  // optional
    const ListValue* exclude_matches = NULL;
    if (!content_script->GetList(keys::kExcludeMatches, &exclude_matches)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidExcludeMatches,
          base::IntToString(definition_index));
      return false;
    }

    for (size_t j = 0; j < exclude_matches->GetSize(); ++j) {
      std::string match_str;
      if (!exclude_matches->GetString(j, &match_str)) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidExcludeMatch,
            base::IntToString(definition_index),
            base::IntToString(j),
            errors::kExpectString);
        return false;
      }

      URLPattern pattern(UserScript::kValidUserScriptSchemes);
      if (CanExecuteScriptEverywhere())
        pattern.SetValidSchemes(URLPattern::SCHEME_ALL);
      URLPattern::ParseResult parse_result = pattern.Parse(match_str);
      if (parse_result != URLPattern::PARSE_SUCCESS) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidExcludeMatch,
            base::IntToString(definition_index), base::IntToString(j),
            URLPattern::GetParseResultString(parse_result));
        return false;
      }

      result->add_exclude_url_pattern(pattern);
    }
  }

  // include/exclude globs (mostly for Greasemonkey compatibility)
  if (!LoadGlobsHelper(content_script, definition_index, keys::kIncludeGlobs,
                       error, &UserScript::add_glob, result)) {
      return false;
  }

  if (!LoadGlobsHelper(content_script, definition_index, keys::kExcludeGlobs,
                       error, &UserScript::add_exclude_glob, result)) {
      return false;
  }

  // js and css keys
  const ListValue* js = NULL;
  if (content_script->HasKey(keys::kJs) &&
      !content_script->GetList(keys::kJs, &js)) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidJsList,
        base::IntToString(definition_index));
    return false;
  }

  const ListValue* css = NULL;
  if (content_script->HasKey(keys::kCss) &&
      !content_script->GetList(keys::kCss, &css)) {
    *error = ErrorUtils::
        FormatErrorMessageUTF16(errors::kInvalidCssList,
        base::IntToString(definition_index));
    return false;
  }

  // The manifest needs to have at least one js or css user script definition.
  if (((js ? js->GetSize() : 0) + (css ? css->GetSize() : 0)) == 0) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kMissingFile,
        base::IntToString(definition_index));
    return false;
  }

  if (js) {
    for (size_t script_index = 0; script_index < js->GetSize();
         ++script_index) {
      const Value* value;
      std::string relative;
      if (!js->Get(script_index, &value) || !value->GetAsString(&relative)) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidJs,
            base::IntToString(definition_index),
            base::IntToString(script_index));
        return false;
      }
      GURL url = GetResourceURL(relative);
      ExtensionResource resource = GetResource(relative);
      result->js_scripts().push_back(UserScript::File(
          resource.extension_root(), resource.relative_path(), url));
    }
  }

  if (css) {
    for (size_t script_index = 0; script_index < css->GetSize();
         ++script_index) {
      const Value* value;
      std::string relative;
      if (!css->Get(script_index, &value) || !value->GetAsString(&relative)) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidCss,
            base::IntToString(definition_index),
            base::IntToString(script_index));
        return false;
      }
      GURL url = GetResourceURL(relative);
      ExtensionResource resource = GetResource(relative);
      result->css_scripts().push_back(UserScript::File(
          resource.extension_root(), resource.relative_path(), url));
    }
  }

  return true;
}

bool Extension::LoadGlobsHelper(
    const DictionaryValue* content_script,
    int content_script_index,
    const char* globs_property_name,
    string16* error,
    void(UserScript::*add_method)(const std::string& glob),
    UserScript* instance) {
  if (!content_script->HasKey(globs_property_name))
    return true;  // they are optional

  const ListValue* list = NULL;
  if (!content_script->GetList(globs_property_name, &list)) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidGlobList,
        base::IntToString(content_script_index),
        globs_property_name);
    return false;
  }

  for (size_t i = 0; i < list->GetSize(); ++i) {
    std::string glob;
    if (!list->GetString(i, &glob)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidGlob,
          base::IntToString(content_script_index),
          globs_property_name,
          base::IntToString(i));
      return false;
    }

    (instance->*add_method)(glob);
  }

  return true;
}

scoped_ptr<Extension::ActionInfo> Extension::LoadExtensionActionInfoHelper(
    const DictionaryValue* extension_action,
    ActionInfo::Type action_type,
    string16* error) {
  scoped_ptr<ActionInfo> result(new ActionInfo());

  if (manifest_version_ == 1) {
    // kPageActionIcons is obsolete, and used by very few extensions. Continue
    // loading it, but only take the first icon as the default_icon path.
    const ListValue* icons = NULL;
    if (extension_action->HasKey(keys::kPageActionIcons) &&
        extension_action->GetList(keys::kPageActionIcons, &icons)) {
      for (ListValue::const_iterator iter = icons->begin();
           iter != icons->end(); ++iter) {
        std::string path;
        if (!(*iter)->GetAsString(&path) || !NormalizeAndValidatePath(&path)) {
          *error = ASCIIToUTF16(errors::kInvalidPageActionIconPath);
          return scoped_ptr<ActionInfo>();
        }

        result->default_icon.Add(extension_misc::EXTENSION_ICON_ACTION, path);
        break;
      }
    }

    std::string id;
    if (extension_action->HasKey(keys::kPageActionId)) {
      if (!extension_action->GetString(keys::kPageActionId, &id)) {
        *error = ASCIIToUTF16(errors::kInvalidPageActionId);
        return scoped_ptr<ActionInfo>();
      }
      result->id = id;
    }
  }

  // Read the page action |default_icon| (optional).
  // The |default_icon| value can be either dictionary {icon size -> icon path}
  // or non empty string value.
  if (extension_action->HasKey(keys::kPageActionDefaultIcon)) {
    const DictionaryValue* icons_value = NULL;
    std::string default_icon;
    if (extension_action->GetDictionary(keys::kPageActionDefaultIcon,
                                        &icons_value)) {
      if (!LoadIconsFromDictionary(icons_value,
                                   extension_misc::kExtensionActionIconSizes,
                                   extension_misc::kNumExtensionActionIconSizes,
                                   &result->default_icon,
                                   error)) {
        return scoped_ptr<ActionInfo>();
      }
    } else if (extension_action->GetString(keys::kPageActionDefaultIcon,
                                           &default_icon) &&
               NormalizeAndValidatePath(&default_icon)) {
      result->default_icon.Add(extension_misc::EXTENSION_ICON_ACTION,
                               default_icon);
    } else {
      *error = ASCIIToUTF16(errors::kInvalidPageActionIconPath);
      return scoped_ptr<ActionInfo>();
    }
  }

  // Read the page action title from |default_title| if present, |name| if not
  // (both optional).
  if (extension_action->HasKey(keys::kPageActionDefaultTitle)) {
    if (!extension_action->GetString(keys::kPageActionDefaultTitle,
                                     &result->default_title)) {
      *error = ASCIIToUTF16(errors::kInvalidPageActionDefaultTitle);
      return scoped_ptr<ActionInfo>();
    }
  } else if (manifest_version_ == 1 && extension_action->HasKey(keys::kName)) {
    if (!extension_action->GetString(keys::kName, &result->default_title)) {
      *error = ASCIIToUTF16(errors::kInvalidPageActionName);
      return scoped_ptr<ActionInfo>();
    }
  }

  // Read the action's |popup| (optional).
  const char* popup_key = NULL;
  if (extension_action->HasKey(keys::kPageActionDefaultPopup))
    popup_key = keys::kPageActionDefaultPopup;

  if (manifest_version_ == 1 &&
      extension_action->HasKey(keys::kPageActionPopup)) {
    if (popup_key) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidPageActionOldAndNewKeys,
          keys::kPageActionDefaultPopup,
          keys::kPageActionPopup);
      return scoped_ptr<ActionInfo>();
    }
    popup_key = keys::kPageActionPopup;
  }

  if (popup_key) {
    const DictionaryValue* popup = NULL;
    std::string url_str;

    if (extension_action->GetString(popup_key, &url_str)) {
      // On success, |url_str| is set.  Nothing else to do.
    } else if (manifest_version_ == 1 &&
               extension_action->GetDictionary(popup_key, &popup)) {
      if (!popup->GetString(keys::kPageActionPopupPath, &url_str)) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidPageActionPopupPath, "<missing>");
        return scoped_ptr<ActionInfo>();
      }
    } else {
      *error = ASCIIToUTF16(errors::kInvalidPageActionPopup);
      return scoped_ptr<ActionInfo>();
    }

    if (!url_str.empty()) {
      // An empty string is treated as having no popup.
      result->default_popup_url = GetResourceURL(url_str);
      if (!result->default_popup_url.is_valid()) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidPageActionPopupPath, url_str);
        return scoped_ptr<ActionInfo>();
      }
    } else {
      DCHECK(result->default_popup_url.is_empty())
          << "Shouldn't be possible for the popup to be set.";
    }
  }

  return result.Pass();
}

bool Extension::LoadOAuth2Info(string16* error) {
  if (!manifest_->HasKey(keys::kOAuth2))
    return true;

  if (!manifest_->GetString(keys::kOAuth2ClientId, &oauth2_info_.client_id) ||
      oauth2_info_.client_id.empty()) {
    *error = ASCIIToUTF16(errors::kInvalidOAuth2ClientId);
    return false;
  }

  ListValue* list = NULL;
  if (!manifest_->GetList(keys::kOAuth2Scopes, &list)) {
    *error = ASCIIToUTF16(errors::kInvalidOAuth2Scopes);
    return false;
  }

  for (size_t i = 0; i < list->GetSize(); ++i) {
    std::string scope;
    if (!list->GetString(i, &scope)) {
      *error = ASCIIToUTF16(errors::kInvalidOAuth2Scopes);
      return false;
    }
    oauth2_info_.scopes.push_back(scope);
  }

  return true;
}

bool Extension::HasMultipleUISurfaces() const {
  int num_surfaces = 0;

  if (page_action_info())
    ++num_surfaces;

  if (browser_action_info())
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
  if (location() == Extension::COMPONENT)
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
    if (pattern.host() == chrome::kChromeUIThumbnailHost) {
      return permissions.find(APIPermission::kExperimental) !=
          permissions.end();
    }

    // Component extensions can have access to all of chrome://*.
    if (CanExecuteScriptEverywhere())
      return true;

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

bool Extension::CheckPlatformAppFeatures(std::string* utf8_error) const {
  if (!is_platform_app())
    return true;

  if (!has_background_page()) {
    *utf8_error = errors::kBackgroundRequiredForPlatformApps;
    return false;
  }

  if (!incognito_split_mode_) {
    *utf8_error = errors::kInvalidIncognitoModeForPlatformApp;
    return false;
  }

  return true;
}

bool Extension::CheckConflictingFeatures(std::string* utf8_error) const {
  if (has_lazy_background_page() &&
      HasAPIPermission(APIPermission::kWebRequest)) {
    *utf8_error = errors::kWebRequestConflictsWithLazyBackground;
    return false;
  }

  return true;
}

void PrintTo(const Extension::InstallWarning& warning, ::std::ostream* os) {
  *os << "InstallWarning(";
  switch (warning.format) {
    case Extension::InstallWarning::FORMAT_TEXT:
      *os << "FORMAT_TEXT, \"";
      break;
    case Extension::InstallWarning::FORMAT_HTML:
      *os << "FORMAT_HTML, \"";
      break;
  }
  // This is just for test error messages, so no need to escape '"'
  // characters inside the message.
  *os << warning.message << "\")";
}

ExtensionInfo::ExtensionInfo(const DictionaryValue* manifest,
                             const std::string& id,
                             const FilePath& path,
                             Extension::Location location)
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
