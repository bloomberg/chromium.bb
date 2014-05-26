// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/file_util.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/message_bundle.h"
#include "grit/extensions_strings.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace extensions {
namespace file_util {
namespace {

// Returns true if the given file path exists and is not zero-length.
bool ValidateFilePath(const base::FilePath& path) {
  int64 size = 0;
  if (!base::PathExists(path) ||
      !base::GetFileSize(path, &size) ||
      size == 0) {
    return false;
  }

  return true;
}

}  // namespace

const base::FilePath::CharType kTempDirectoryName[] = FILE_PATH_LITERAL("Temp");

base::FilePath InstallExtension(const base::FilePath& unpacked_source_dir,
                                const std::string& id,
                                const std::string& version,
                                const base::FilePath& extensions_dir) {
  base::FilePath extension_dir = extensions_dir.AppendASCII(id);
  base::FilePath version_dir;

  // Create the extension directory if it doesn't exist already.
  if (!base::PathExists(extension_dir)) {
    if (!base::CreateDirectory(extension_dir))
      return base::FilePath();
  }

  // Get a temp directory on the same file system as the profile.
  base::FilePath install_temp_dir = GetInstallTempDir(extensions_dir);
  base::ScopedTempDir extension_temp_dir;
  if (install_temp_dir.empty() ||
      !extension_temp_dir.CreateUniqueTempDirUnderPath(install_temp_dir)) {
    LOG(ERROR) << "Creating of temp dir under in the profile failed.";
    return base::FilePath();
  }
  base::FilePath crx_temp_source =
      extension_temp_dir.path().Append(unpacked_source_dir.BaseName());
  if (!base::Move(unpacked_source_dir, crx_temp_source)) {
    LOG(ERROR) << "Moving extension from : " << unpacked_source_dir.value()
               << " to : " << crx_temp_source.value() << " failed.";
    return base::FilePath();
  }

  // Try to find a free directory. There can be legitimate conflicts in the case
  // of overinstallation of the same version.
  const int kMaxAttempts = 100;
  for (int i = 0; i < kMaxAttempts; ++i) {
    base::FilePath candidate = extension_dir.AppendASCII(
        base::StringPrintf("%s_%u", version.c_str(), i));
    if (!base::PathExists(candidate)) {
      version_dir = candidate;
      break;
    }
  }

  if (version_dir.empty()) {
    LOG(ERROR) << "Could not find a home for extension " << id << " with "
               << "version " << version << ".";
    return base::FilePath();
  }

  if (!base::Move(crx_temp_source, version_dir)) {
    LOG(ERROR) << "Installing extension from : " << crx_temp_source.value()
               << " into : " << version_dir.value() << " failed.";
    return base::FilePath();
  }

  return version_dir;
}

void UninstallExtension(const base::FilePath& extensions_dir,
                        const std::string& id) {
  // We don't care about the return value. If this fails (and it can, due to
  // plugins that aren't unloaded yet), it will get cleaned up by
  // ExtensionGarbageCollector::GarbageCollectExtensions.
  base::DeleteFile(extensions_dir.AppendASCII(id), true);  // recursive.
}

scoped_refptr<Extension> LoadExtension(const base::FilePath& extension_path,
                                       Manifest::Location location,
                                       int flags,
                                       std::string* error) {
  return LoadExtension(extension_path, std::string(), location, flags, error);
}

scoped_refptr<Extension> LoadExtension(const base::FilePath& extension_path,
                                       const std::string& extension_id,
                                       Manifest::Location location,
                                       int flags,
                                       std::string* error) {
  scoped_ptr<base::DictionaryValue> manifest(
      LoadManifest(extension_path, error));
  if (!manifest.get())
    return NULL;
  if (!extension_l10n_util::LocalizeExtension(
          extension_path, manifest.get(), error)) {
    return NULL;
  }

  scoped_refptr<Extension> extension(Extension::Create(
      extension_path, location, *manifest, flags, extension_id, error));
  if (!extension.get())
    return NULL;

  std::vector<InstallWarning> warnings;
  if (!ValidateExtension(extension.get(), error, &warnings))
    return NULL;
  extension->AddInstallWarnings(warnings);

  return extension;
}

base::DictionaryValue* LoadManifest(const base::FilePath& extension_path,
                                    std::string* error) {
  return LoadManifest(extension_path, kManifestFilename, error);
}

base::DictionaryValue* LoadManifest(
    const base::FilePath& extension_path,
    const base::FilePath::CharType* manifest_filename,
    std::string* error) {
  base::FilePath manifest_path = extension_path.Append(manifest_filename);
  if (!base::PathExists(manifest_path)) {
    *error = l10n_util::GetStringUTF8(IDS_EXTENSION_MANIFEST_UNREADABLE);
    return NULL;
  }

  JSONFileValueSerializer serializer(manifest_path);
  scoped_ptr<base::Value> root(serializer.Deserialize(NULL, error));
  if (!root.get()) {
    if (error->empty()) {
      // If |error| is empty, than the file could not be read.
      // It would be cleaner to have the JSON reader give a specific error
      // in this case, but other code tests for a file error with
      // error->empty().  For now, be consistent.
      *error = l10n_util::GetStringUTF8(IDS_EXTENSION_MANIFEST_UNREADABLE);
    } else {
      *error = base::StringPrintf(
          "%s  %s", manifest_errors::kManifestParseError, error->c_str());
    }
    return NULL;
  }

  if (!root->IsType(base::Value::TYPE_DICTIONARY)) {
    *error = l10n_util::GetStringUTF8(IDS_EXTENSION_MANIFEST_INVALID);
    return NULL;
  }

  return static_cast<base::DictionaryValue*>(root.release());
}

bool ValidateExtension(const Extension* extension,
                       std::string* error,
                       std::vector<InstallWarning>* warnings) {
  // Ask registered manifest handlers to validate their paths.
  if (!ManifestHandler::ValidateExtension(extension, error, warnings))
    return false;

  // Check children of extension root to see if any of them start with _ and is
  // not on the reserved list. We only warn, and do not block the loading of the
  // extension.
  std::string warning;
  if (!CheckForIllegalFilenames(extension->path(), &warning))
    warnings->push_back(InstallWarning(warning));

  // Check that extensions don't include private key files.
  std::vector<base::FilePath> private_keys =
      FindPrivateKeyFiles(extension->path());
  if (extension->creation_flags() & Extension::ERROR_ON_PRIVATE_KEY) {
    if (!private_keys.empty()) {
      // Only print one of the private keys because l10n_util doesn't have a way
      // to translate a list of strings.
      *error =
          l10n_util::GetStringFUTF8(IDS_EXTENSION_CONTAINS_PRIVATE_KEY,
                                    private_keys.front().LossyDisplayName());
      return false;
    }
  } else {
    for (size_t i = 0; i < private_keys.size(); ++i) {
      warnings->push_back(InstallWarning(
          l10n_util::GetStringFUTF8(IDS_EXTENSION_CONTAINS_PRIVATE_KEY,
                                    private_keys[i].LossyDisplayName())));
    }
    // Only warn; don't block loading the extension.
  }
  return true;
}

std::vector<base::FilePath> FindPrivateKeyFiles(
    const base::FilePath& extension_dir) {
  std::vector<base::FilePath> result;
  // Pattern matching only works at the root level, so filter manually.
  base::FileEnumerator traversal(
      extension_dir, /*recursive=*/true, base::FileEnumerator::FILES);
  for (base::FilePath current = traversal.Next(); !current.empty();
       current = traversal.Next()) {
    if (!current.MatchesExtension(kExtensionKeyFileExtension))
      continue;

    std::string key_contents;
    if (!base::ReadFileToString(current, &key_contents)) {
      // If we can't read the file, assume it's not a private key.
      continue;
    }
    std::string key_bytes;
    if (!Extension::ParsePEMKeyBytes(key_contents, &key_bytes)) {
      // If we can't parse the key, assume it's ok too.
      continue;
    }

    result.push_back(current);
  }
  return result;
}

bool CheckForIllegalFilenames(const base::FilePath& extension_path,
                              std::string* error) {
  // Reserved underscore names.
  static const base::FilePath::CharType* reserved_names[] = {
      kLocaleFolder, kPlatformSpecificFolder, FILE_PATH_LITERAL("__MACOSX"), };
  CR_DEFINE_STATIC_LOCAL(
      std::set<base::FilePath::StringType>,
      reserved_underscore_names,
      (reserved_names, reserved_names + arraysize(reserved_names)));

  // Enumerate all files and directories in the extension root.
  // There is a problem when using pattern "_*" with FileEnumerator, so we have
  // to cheat with find_first_of and match all.
  const int kFilesAndDirectories =
      base::FileEnumerator::DIRECTORIES | base::FileEnumerator::FILES;
  base::FileEnumerator all_files(extension_path, false, kFilesAndDirectories);

  base::FilePath file;
  while (!(file = all_files.Next()).empty()) {
    base::FilePath::StringType filename = file.BaseName().value();
    // Skip all that don't start with "_".
    if (filename.find_first_of(FILE_PATH_LITERAL("_")) != 0)
      continue;
    if (reserved_underscore_names.find(filename) ==
        reserved_underscore_names.end()) {
      *error = base::StringPrintf(
          "Cannot load extension with file or directory name %s. "
          "Filenames starting with \"_\" are reserved for use by the system.",
          file.BaseName().AsUTF8Unsafe().c_str());
      return false;
    }
  }

  return true;
}

base::FilePath GetInstallTempDir(const base::FilePath& extensions_dir) {
  // We do file IO in this function, but only when the current profile's
  // Temp directory has never been used before, or in a rare error case.
  // Developers are not likely to see these situations often, so do an
  // explicit thread check.
  base::ThreadRestrictions::AssertIOAllowed();

  // Create the temp directory as a sub-directory of the Extensions directory.
  // This guarantees it is on the same file system as the extension's eventual
  // install target.
  base::FilePath temp_path = extensions_dir.Append(kTempDirectoryName);
  if (base::PathExists(temp_path)) {
    if (!base::DirectoryExists(temp_path)) {
      DLOG(WARNING) << "Not a directory: " << temp_path.value();
      return base::FilePath();
    }
    if (!base::PathIsWritable(temp_path)) {
      DLOG(WARNING) << "Can't write to path: " << temp_path.value();
      return base::FilePath();
    }
    // This is a directory we can write to.
    return temp_path;
  }

  // Directory doesn't exist, so create it.
  if (!base::CreateDirectory(temp_path)) {
    DLOG(WARNING) << "Couldn't create directory: " << temp_path.value();
    return base::FilePath();
  }
  return temp_path;
}

void DeleteFile(const base::FilePath& path, bool recursive) {
  base::DeleteFile(path, recursive);
}

base::FilePath ExtensionURLToRelativeFilePath(const GURL& url) {
  std::string url_path = url.path();
  if (url_path.empty() || url_path[0] != '/')
    return base::FilePath();

  // Drop the leading slashes and convert %-encoded UTF8 to regular UTF8.
  std::string file_path = net::UnescapeURLComponent(url_path,
      net::UnescapeRule::SPACES | net::UnescapeRule::URL_SPECIAL_CHARS);
  size_t skip = file_path.find_first_not_of("/\\");
  if (skip != file_path.npos)
    file_path = file_path.substr(skip);

  base::FilePath path = base::FilePath::FromUTF8Unsafe(file_path);

  // It's still possible for someone to construct an annoying URL whose path
  // would still wind up not being considered relative at this point.
  // For example: chrome-extension://id/c:////foo.html
  if (path.IsAbsolute())
    return base::FilePath();

  return path;
}

base::FilePath ExtensionResourceURLToFilePath(const GURL& url,
                                              const base::FilePath& root) {
  std::string host = net::UnescapeURLComponent(url.host(),
      net::UnescapeRule::SPACES | net::UnescapeRule::URL_SPECIAL_CHARS);
  if (host.empty())
    return base::FilePath();

  base::FilePath relative_path = ExtensionURLToRelativeFilePath(url);
  if (relative_path.empty())
    return base::FilePath();

  base::FilePath path = root.AppendASCII(host).Append(relative_path);
  if (!base::PathExists(path))
    return base::FilePath();
  path = base::MakeAbsoluteFilePath(path);
  if (path.empty() || !root.IsParent(path))
    return base::FilePath();
  return path;
}

bool ValidateExtensionIconSet(const ExtensionIconSet& icon_set,
                              const Extension* extension,
                              int error_message_id,
                              std::string* error) {
  for (ExtensionIconSet::IconMap::const_iterator iter = icon_set.map().begin();
       iter != icon_set.map().end();
       ++iter) {
    const base::FilePath path =
        extension->GetResource(iter->second).GetFilePath();
    if (!ValidateFilePath(path)) {
      *error = l10n_util::GetStringFUTF8(error_message_id,
                                         base::UTF8ToUTF16(iter->second));
      return false;
    }
  }
  return true;
}

MessageBundle* LoadMessageBundle(
    const base::FilePath& extension_path,
    const std::string& default_locale,
    std::string* error) {
  error->clear();
  // Load locale information if available.
  base::FilePath locale_path = extension_path.Append(kLocaleFolder);
  if (!base::PathExists(locale_path))
    return NULL;

  std::set<std::string> locales;
  if (!extension_l10n_util::GetValidLocales(locale_path, &locales, error))
    return NULL;

  if (default_locale.empty() || locales.find(default_locale) == locales.end()) {
    *error = l10n_util::GetStringUTF8(
        IDS_EXTENSION_LOCALES_NO_DEFAULT_LOCALE_SPECIFIED);
    return NULL;
  }

  MessageBundle* message_bundle =
      extension_l10n_util::LoadMessageCatalogs(
          locale_path,
          default_locale,
          extension_l10n_util::CurrentLocaleOrDefault(),
          locales,
          error);

  return message_bundle;
}

std::map<std::string, std::string>* LoadMessageBundleSubstitutionMap(
    const base::FilePath& extension_path,
    const std::string& extension_id,
    const std::string& default_locale) {
  std::map<std::string, std::string>* return_value =
      new std::map<std::string, std::string>();
  if (!default_locale.empty()) {
    // Touch disk only if extension is localized.
    std::string error;
    scoped_ptr<MessageBundle> bundle(
        LoadMessageBundle(extension_path, default_locale, &error));

    if (bundle.get())
      *return_value = *bundle->dictionary();
  }

  // Add @@extension_id reserved message here, so it's available to
  // non-localized extensions too.
  return_value->insert(
      std::make_pair(MessageBundle::kExtensionIdKey, extension_id));

  return return_value;
}

base::FilePath GetVerifiedContentsPath(const base::FilePath& extension_path) {
  return extension_path.Append(kMetadataFolder)
      .Append(kVerifiedContentsFilename);
}
base::FilePath GetComputedHashesPath(const base::FilePath& extension_path) {
  return extension_path.Append(kMetadataFolder).Append(kComputedHashesFilename);
}

}  // namespace file_util
}  // namespace extensions
