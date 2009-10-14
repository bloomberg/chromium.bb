// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_file_util.h"

#include "app/l10n_util.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/scoped_temp_dir.h"
#include "base/string_util.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_l10n_util.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/json_value_serializer.h"
#include "net/base/file_stream.h"

namespace extension_file_util {

const char kInstallDirectoryName[] = "Extensions";
// TODO(mpcomplete): obsolete. remove after migration period.
// http://code.google.com/p/chromium/issues/detail?id=19733
const char kCurrentVersionFileName[] = "Current Version";

bool MoveDirSafely(const FilePath& source_dir, const FilePath& dest_dir) {
  if (file_util::PathExists(dest_dir)) {
    if (!file_util::Delete(dest_dir, true))
      return false;
  } else {
    FilePath parent = dest_dir.DirName();
    if (!file_util::DirectoryExists(parent)) {
      if (!file_util::CreateDirectory(parent))
        return false;
    }
  }

  if (!file_util::Move(source_dir, dest_dir))
    return false;

  return true;
}

Extension::InstallType CompareToInstalledVersion(
    const FilePath& extensions_dir,
    const std::string& extension_id,
    const std::string& current_version_str,
    const std::string& new_version_str,
    FilePath* version_dir) {
  FilePath dest_dir = extensions_dir.AppendASCII(extension_id);
  FilePath current_version_dir = dest_dir.AppendASCII(current_version_str);
  *version_dir = dest_dir.AppendASCII(new_version_str);

  if (current_version_str.empty())
    return Extension::NEW_INSTALL;

  scoped_ptr<Version> current_version(
    Version::GetVersionFromString(current_version_str));
  scoped_ptr<Version> new_version(
    Version::GetVersionFromString(new_version_str));
  int comp = new_version->CompareTo(*current_version);
  if (comp > 0)
    return Extension::UPGRADE;
  if (comp < 0)
    return Extension::DOWNGRADE;

  // Same version. Treat corrupted existing installation as new install case.
  if (!SanityCheckExtension(current_version_dir))
    return Extension::NEW_INSTALL;

  return Extension::REINSTALL;
}

bool SanityCheckExtension(const FilePath& dir) {
  // Verify that the directory actually exists.
  // TODO(erikkay): A further step would be to verify that the extension
  // has actually loaded successfully.
  FilePath manifest_file(dir.AppendASCII(Extension::kManifestFilename));
  return file_util::PathExists(dir) && file_util::PathExists(manifest_file);
}

bool InstallExtension(const FilePath& src_dir,
                      const FilePath& version_dir,
                      std::string* error) {
  // If anything fails after this, we want to delete the extension dir.
  ScopedTempDir scoped_version_dir;
  scoped_version_dir.Set(version_dir);

  if (!MoveDirSafely(src_dir, version_dir)) {
    *error = "Could not move extension directory into profile.";
    return false;
  }

  scoped_version_dir.Take();
  return true;
}

Extension* LoadExtension(const FilePath& extension_path,
                         bool require_key,
                         std::string* error) {
  FilePath manifest_path =
      extension_path.AppendASCII(Extension::kManifestFilename);
  if (!file_util::PathExists(manifest_path)) {
    *error = extension_manifest_errors::kManifestUnreadable;
    return NULL;
  }

  JSONFileValueSerializer serializer(manifest_path);
  scoped_ptr<Value> root(serializer.Deserialize(error));
  if (!root.get()) {
    if (error->empty()) {
      // If |error| is empty, than the file could not be read.
      // It would be cleaner to have the JSON reader give a specific error
      // in this case, but other code tests for a file error with
      // error->empty().  For now, be consistent.
      *error = extension_manifest_errors::kManifestUnreadable;
    } else {
      *error = StringPrintf("%s  %s",
                            extension_manifest_errors::kManifestParseError,
                            error->c_str());
    }
    return NULL;
  }

  if (!root->IsType(Value::TYPE_DICTIONARY)) {
    *error = extension_manifest_errors::kInvalidManifest;
    return NULL;
  }

  DictionaryValue* manifest = static_cast<DictionaryValue*>(root.get());
  ExtensionMessageBundle* message_bundle =
    LoadLocaleInfo(extension_path, *manifest, error);
  if (!message_bundle && !error->empty())
    return NULL;

  scoped_ptr<Extension> extension(new Extension(extension_path));
  // Assign message bundle to extension.
  extension->set_message_bundle(message_bundle);
  if (!extension->InitFromValue(*manifest, require_key, error))
    return NULL;

  if (!ValidateExtension(extension.get(), error))
    return NULL;

  return extension.release();
}

bool ValidateExtension(Extension* extension, std::string* error) {
  // Validate icons exist.
  for (std::map<int, std::string>::const_iterator iter =
       extension->icons().begin(); iter != extension->icons().end(); ++iter) {
    if (extension->GetResource(iter->second).GetFilePath().empty()) {
      *error = StringPrintf("Could not load extension icon '%s'.",
                            iter->second.c_str());
      return false;
    }
  }

  // Theme resource validation.
  if (extension->IsTheme()) {
    DictionaryValue* images_value = extension->GetThemeImages();
    if (images_value) {
      for (DictionaryValue::key_iterator iter = images_value->begin_keys();
           iter != images_value->end_keys(); ++iter) {
        std::string val;
        if (images_value->GetString(*iter, &val)) {
          FilePath image_path = extension->path().AppendASCII(val);
          if (!file_util::PathExists(image_path)) {
            *error = StringPrintf(
                "Could not load '%s' for theme.",
                WideToUTF8(image_path.ToWStringHack()).c_str());
            return false;
          }
        }
      }
    }

    // Themes cannot contain other extension types.
    return true;
  }

  // Validate that claimed script resources actually exist.
  for (size_t i = 0; i < extension->content_scripts().size(); ++i) {
    const UserScript& script = extension->content_scripts()[i];

    for (size_t j = 0; j < script.js_scripts().size(); j++) {
      const FilePath& path =
        script.js_scripts()[j].resource().GetFilePath();
      if (path.empty()) {
        *error = StringPrintf("Could not load '%s' for content script.",
                              WideToUTF8(path.ToWStringHack()).c_str());
        return false;
      }
    }

    for (size_t j = 0; j < script.css_scripts().size(); j++) {
      const FilePath& path =
        script.css_scripts()[j].resource().GetFilePath();
      if (path.empty()) {
        *error = StringPrintf("Could not load '%s' for content script.",
                              WideToUTF8(path.ToWStringHack()).c_str());
        return false;
      }
    }
  }

  // Validate claimed plugin paths.
  for (size_t i = 0; i < extension->plugins().size(); ++i) {
    const Extension::PluginInfo& plugin = extension->plugins()[i];
    if (!file_util::PathExists(plugin.path)) {
      *error = StringPrintf("Could not load '%s' for plugin.",
                            WideToUTF8(plugin.path.ToWStringHack()).c_str());
      return false;
    }
  }

  // Validate claimed privacy blacklists paths.
  for (size_t i = 0; i < extension->privacy_blacklists().size(); ++i) {
    const Extension::PrivacyBlacklistInfo& blacklist =
        extension->privacy_blacklists()[i];
    if (!file_util::PathExists(blacklist.path)) {
      *error = StringPrintf("Could not load '%s' for privacy blacklist.",
                            WideToUTF8(blacklist.path.ToWStringHack()).c_str());
      return false;
    }
  }

  // Validate icon location for page actions.
  const ExtensionAction* page_action = extension->page_action();
  if (page_action) {
    const std::vector<std::string>& icon_paths = page_action->icon_paths();
    for (std::vector<std::string>::const_iterator iter = icon_paths.begin();
         iter != icon_paths.end(); ++iter) {
      if (extension->GetResource(*iter).GetFilePath().empty()) {
        *error = StringPrintf("Could not load icon '%s' for page action.",
                              iter->c_str());
        return false;
      }
    }
  }

  // Validate icon location for browser actions.
  const ExtensionAction* browser_action = extension->browser_action();
  if (browser_action) {
    const std::vector<std::string>& icon_paths = browser_action->icon_paths();
    for (std::vector<std::string>::const_iterator iter = icon_paths.begin();
         iter != icon_paths.end(); ++iter) {
      if (extension->GetResource(*iter).GetFilePath().empty()) {
        *error = StringPrintf("Could not load icon '%s' for browser action.",
                              iter->c_str());
        return false;
      }
    }
  }

  // Check children of extension root to see if any of them start with _ and is
  // not on the reserved list.
  if (!CheckForIllegalFilenames(extension->path(), error)) {
    return false;
  }

  return true;
}

void UninstallExtension(const std::string& id, const FilePath& extensions_dir) {
  // First, delete the Current Version file. If the directory delete fails, then
  // at least the extension won't be loaded again.
  FilePath extension_root = extensions_dir.AppendASCII(id);

  if (!file_util::PathExists(extension_root)) {
    LOG(WARNING) << "Asked to remove a non-existent extension " << id;
    return;
  }

  FilePath current_version_file = extension_root.AppendASCII(
      kCurrentVersionFileName);
  if (!file_util::PathExists(current_version_file)) {
    // This is OK, since we're phasing out the current version file.
  } else {
    if (!file_util::Delete(current_version_file, false)) {
      LOG(WARNING) << "Could not delete Current Version file for extension "
                   << id;
      return;
    }
  }

  // OK, now try and delete the entire rest of the directory. One major place
  // this can fail is if the extension contains a plugin (stupid plugins). It's
  // not a big deal though, because we'll notice next time we startup that the
  // Current Version file is gone and finish the delete then.
  if (!file_util::Delete(extension_root, true))
    LOG(WARNING) << "Could not delete directory for extension " << id;
}

void GarbageCollectExtensions(const FilePath& install_directory,
                              const std::set<std::string>& installed_ids) {
  // Nothing to clean up if it doesn't exist.
  if (!file_util::DirectoryExists(install_directory))
    return;

  LOG(INFO) << "Loading installed extensions...";
  file_util::FileEnumerator enumerator(install_directory,
                                       false,  // Not recursive.
                                       file_util::FileEnumerator::DIRECTORIES);
  FilePath extension_path;
  for (extension_path = enumerator.Next(); !extension_path.value().empty();
       extension_path = enumerator.Next()) {
    std::string extension_id = WideToASCII(
        extension_path.BaseName().ToWStringHack());

    // If there is no entry in the prefs file, just delete the directory and
    // move on. This can legitimately happen when an uninstall does not
    // complete, for example, when a plugin is in use at uninstall time.
    if (installed_ids.count(extension_id) == 0) {
      LOG(INFO) << "Deleting unreferenced install for directory "
                << WideToASCII(extension_path.ToWStringHack()) << ".";
      file_util::Delete(extension_path, true);  // Recursive.
      continue;
    }

    // Ignore directories that aren't valid IDs.
    if (!Extension::IdIsValid(extension_id)) {
      LOG(WARNING) << "Invalid extension ID encountered in extensions "
                      "directory: " << extension_id;
      LOG(INFO) << "Deleting invalid extension directory "
                << WideToASCII(extension_path.ToWStringHack()) << ".";
      file_util::Delete(extension_path, true);  // Recursive.
      continue;
    }
  }
}

ExtensionMessageBundle* LoadLocaleInfo(const FilePath& extension_path,
                                       const DictionaryValue& manifest,
                                       std::string* error) {
  error->clear();
  // Load locale information if available.
  FilePath locale_path = extension_path.AppendASCII(Extension::kLocaleFolder);
  if (!file_util::PathExists(locale_path))
    return NULL;

  std::set<std::string> locales;
  if (!extension_l10n_util::GetValidLocales(locale_path, &locales, error))
    return NULL;

  std::string default_locale =
    extension_l10n_util::GetDefaultLocaleFromManifest(manifest, error);
  if (default_locale.empty() ||
      locales.find(default_locale) == locales.end()) {
    *error = extension_manifest_errors::kLocalesNoDefaultLocaleSpecified;
    return NULL;
  }

  // We can't call g_browser_process->GetApplicationLocale() since we are not
  // on the main thread.
  static std::string app_locale = l10n_util::GetApplicationLocale(L"");
  if (locales.find(app_locale) == locales.end())
    app_locale = "";
  ExtensionMessageBundle* message_bundle =
    extension_l10n_util::LoadMessageCatalogs(locale_path,
                                             default_locale,
                                             app_locale,
                                             error);
  return message_bundle;
}

bool CheckForIllegalFilenames(const FilePath& extension_path,
                              std::string* error) {
  // Reserved underscore names.
  static const char* reserved_names[] = {
    Extension::kLocaleFolder
  };
  static std::set<std::string> reserved_underscore_names(
    reserved_names, reserved_names + arraysize(reserved_names));

  // Enumerate all files and directories in the extension root.
  // There is a problem when using pattern "_*" with FileEnumerator, so we have
  // to cheat with find_first_of and match all.
  file_util::FileEnumerator all_files(
    extension_path,
    false,
    static_cast<file_util::FileEnumerator::FILE_TYPE>(
        file_util::FileEnumerator::DIRECTORIES |
          file_util::FileEnumerator::FILES));

  FilePath files;
  while (!(files = all_files.Next()).empty()) {
    std::string filename =
        WideToASCII(files.BaseName().ToWStringHack());
    // Skip all that don't start with "_".
    if (filename.find_first_of("_") != 0) continue;
    if (reserved_underscore_names.find(filename) ==
          reserved_underscore_names.end()) {
      *error = StringPrintf(
        "Cannot load extension with file or directory name %s. "
        "Filenames starting with \"_\" are reserved for use by the system.",
        filename.c_str());
      return false;
    }
  }

  return true;
}

}  // namespace extension_file_util
