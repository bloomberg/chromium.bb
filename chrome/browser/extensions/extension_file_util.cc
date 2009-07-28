// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_file_util.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/scoped_temp_dir.h"
#include "base/string_util.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/json_value_serializer.h"
#include "net/base/file_stream.h"

namespace extension_file_util {

const char kInstallDirectoryName[] = "Extensions";
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

bool SetCurrentVersion(const FilePath& dest_dir, const std::string& version,
                       std::string* error) {
  // Write out the new CurrentVersion file.
  // <profile>/Extension/<name>/CurrentVersion
  FilePath current_version = dest_dir.AppendASCII(kCurrentVersionFileName);
  FilePath current_version_old =
    current_version.InsertBeforeExtension(FILE_PATH_LITERAL("_old"));
  if (file_util::PathExists(current_version_old)) {
    if (!file_util::Delete(current_version_old, false)) {
      *error = "Couldn't remove CurrentVersion_old file.";
      return false;
    }
  }

  if (file_util::PathExists(current_version)) {
    if (!file_util::Move(current_version, current_version_old)) {
      *error = "Couldn't move CurrentVersion file.";
      return false;
    }
  }
  net::FileStream stream;
  int flags = base::PLATFORM_FILE_CREATE_ALWAYS | base::PLATFORM_FILE_WRITE;
  if (stream.Open(current_version, flags) != 0)
    return false;
  if (stream.Write(version.c_str(), version.size(), NULL) < 0) {
    // Restore the old CurrentVersion.
    if (file_util::PathExists(current_version_old)) {
      if (!file_util::Move(current_version_old, current_version)) {
        LOG(WARNING) << "couldn't restore " << current_version_old.value() <<
            " to " << current_version.value();

        // TODO(erikkay): This is an ugly state to be in.  Try harder?
      }
    }
    *error = "Couldn't create CurrentVersion file.";
    return false;
  }
  return true;
}

bool ReadCurrentVersion(const FilePath& dir, std::string* version_string) {
  FilePath current_version = dir.AppendASCII(kCurrentVersionFileName);
  if (file_util::PathExists(current_version)) {
    if (file_util::ReadFileToString(current_version, version_string)) {
      TrimWhitespace(*version_string, TRIM_ALL, version_string);
      return true;
    }
  }
  return false;
}

Extension::InstallType CompareToInstalledVersion(
    const FilePath& install_directory, const std::string& id,
    const std::string& new_version_str, std::string *current_version_str) {
  CHECK(current_version_str);
  FilePath dir(install_directory.AppendASCII(id.c_str()));
  if (!ReadCurrentVersion(dir, current_version_str))
    return Extension::NEW_INSTALL;

  scoped_ptr<Version> current_version(
    Version::GetVersionFromString(*current_version_str));
  scoped_ptr<Version> new_version(
    Version::GetVersionFromString(new_version_str));
  int comp = new_version->CompareTo(*current_version);
  if (comp > 0)
    return Extension::UPGRADE;
  else if (comp == 0)
    return Extension::REINSTALL;
  else
    return Extension::DOWNGRADE;
}

bool SanityCheckExtension(const FilePath& dir) {
  // Verify that the directory actually exists.
  // TODO(erikkay): A further step would be to verify that the extension
  // has actually loaded successfully.
  FilePath manifest_file(dir.AppendASCII(Extension::kManifestFilename));
  return file_util::PathExists(dir) && file_util::PathExists(manifest_file);
}

bool InstallExtension(const FilePath& src_dir,
                      const FilePath& extensions_dir,
                      const std::string& extension_id,
                      const std::string& extension_version,
                      FilePath* version_dir,
                      Extension::InstallType* install_type,
                      std::string* error) {
  FilePath dest_dir = extensions_dir.AppendASCII(extension_id);
  *version_dir = dest_dir.AppendASCII(extension_version);

  std::string current_version;
  *install_type = CompareToInstalledVersion(
      extensions_dir, extension_id, extension_version, &current_version);

  // Do not allow downgrade.
  if (*install_type == Extension::DOWNGRADE)
    return true;

  if (*install_type == Extension::REINSTALL) {
    if (!SanityCheckExtension(*version_dir)) {
      // Treat corrupted existing installation as new install case.
      *install_type = Extension::NEW_INSTALL;
    } else {
      return true;
    }
  }

  // If anything fails after this, we want to delete the extension dir.
  ScopedTempDir scoped_version_dir;
  scoped_version_dir.Set(*version_dir);

  if (!MoveDirSafely(src_dir, *version_dir)) {
    *error = "Could not move extension directory into profile.";
    return false;
  }

  if (!SetCurrentVersion(dest_dir, extension_version, error))
    return false;

  scoped_version_dir.Take();
  return true;
}

Extension* LoadExtension(const FilePath& extension_path, bool require_key,
                         std::string* error) {
  FilePath manifest_path =
      extension_path.AppendASCII(Extension::kManifestFilename);
  if (!file_util::PathExists(manifest_path)) {
    *error = extension_manifest_errors::kInvalidManifest;
    return NULL;
  }

  JSONFileValueSerializer serializer(manifest_path);
  scoped_ptr<Value> root(serializer.Deserialize(error));
  if (!root.get())
    return NULL;

  if (!root->IsType(Value::TYPE_DICTIONARY)) {
    *error = extension_manifest_errors::kInvalidManifest;
    return NULL;
  }

  scoped_ptr<Extension> extension(new Extension(extension_path));
  if (!extension->InitFromValue(*static_cast<DictionaryValue*>(root.get()),
                                require_key, error))
    return NULL;

  // Validate icons exist.
  for (std::map<int, std::string>::const_iterator iter =
       extension->icons().begin(); iter != extension->icons().end(); ++iter) {
    if (!file_util::PathExists(extension->GetResourcePath(iter->second))) {
      *error = StringPrintf("Could not load extension icon '%s'.",
                            iter->second.c_str());
      return NULL;
    }
  }

  // Theme resource validation.
  if (extension->IsTheme()) {
    DictionaryValue* images_value = extension->GetThemeImages();
    DictionaryValue::key_iterator iter = images_value->begin_keys();
    if (images_value) {
      while (iter != images_value->end_keys()) {
        std::string val;
        if (images_value->GetString(*iter , &val)) {
          FilePath image_path = extension->path().AppendASCII(val);
          if (!file_util::PathExists(image_path)) {
            *error = StringPrintf(
                "Could not load '%s' for theme.",
                WideToUTF8(image_path.ToWStringHack()).c_str());
            return NULL;
          }
        }
        ++iter;
      }
    }

    // Themes cannot contain other extension types.
    return extension.release();
  }

  // Validate that claimed script resources actually exist.
  for (size_t i = 0; i < extension->content_scripts().size(); ++i) {
    const UserScript& script = extension->content_scripts()[i];

    for (size_t j = 0; j < script.js_scripts().size(); j++) {
      const FilePath& path = script.js_scripts()[j].path();
      if (!file_util::PathExists(path)) {
        *error = StringPrintf("Could not load '%s' for content script.",
                              WideToUTF8(path.ToWStringHack()).c_str());
        return NULL;
      }
    }

    for (size_t j = 0; j < script.css_scripts().size(); j++) {
      const FilePath& path = script.css_scripts()[j].path();
      if (!file_util::PathExists(path)) {
        *error = StringPrintf("Could not load '%s' for content script.",
                              WideToUTF8(path.ToWStringHack()).c_str());
        return NULL;
      }
    }
  }

  for (size_t i = 0; i < extension->plugins().size(); ++i) {
    const Extension::PluginInfo& plugin = extension->plugins()[i];
    if (!file_util::PathExists(plugin.path)) {
      *error = StringPrintf("Could not load '%s' for plugin.",
                            WideToUTF8(plugin.path.ToWStringHack()).c_str());
      return NULL;
    }
  }

  // Validate icon location for page actions.
  const PageActionMap& page_actions = extension->page_actions();
  for (PageActionMap::const_iterator i(page_actions.begin());
       i != page_actions.end(); ++i) {
    PageAction* page_action = i->second;
    const std::vector<FilePath>& icon_paths = page_action->icon_paths();
    for (std::vector<FilePath>::const_iterator iter = icon_paths.begin();
         iter != icon_paths.end(); ++iter) {
      FilePath path = *iter;
      if (!file_util::PathExists(path)) {
        *error = StringPrintf("Could not load icon '%s' for page action.",
                              WideToUTF8(path.ToWStringHack()).c_str());
        return NULL;
      }
    }
  }

  return extension.release();
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
    LOG(WARNING) << "Extension " << id
                 << " does not have a Current Version file.";
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

void GarbageCollectExtensions(const FilePath& install_directory) {
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

    // If there is no Current Version file, just delete the directory and move
    // on. This can legitimately happen when an uninstall does not complete, for
    // example, when a plugin is in use at uninstall time.
    FilePath current_version_path = extension_path.AppendASCII(
        kCurrentVersionFileName);
    if (!file_util::PathExists(current_version_path)) {
      LOG(INFO) << "Deleting incomplete install for directory "
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

}  // extensionfile_util
