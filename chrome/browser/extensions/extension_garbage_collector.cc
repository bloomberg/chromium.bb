// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_garbage_collector.h"

#include <map>
#include <string>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

ExtensionGarbageCollector::ExtensionGarbageCollector(
    ExtensionService* extension_service)
    : extension_service_(extension_service) {
}

ExtensionGarbageCollector::~ExtensionGarbageCollector() {
}

void ExtensionGarbageCollector::GarbageCollectExtensions() {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  FilePath install_directory = extension_service_->install_directory();

  // NOTE: It's important to use the file thread here because when the
  // ExtensionService installs an extension, it begins on the file thread, and
  // there are ordering dependencies between all of the extension-file-
  // management tasks. If we do not follow the same thread pattern as the
  // ExtensionService, then a race condition develops.
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(
           &ExtensionGarbageCollector::
               CheckExtensionDirectoriesOnBackgroundThread,
           base::Unretained(this),
           install_directory));
}

void ExtensionGarbageCollector::CheckExtensionDirectoriesOnBackgroundThread(
    const FilePath& install_directory) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  DVLOG(1) << "Garbage collecting extensions...";

  if (!file_util::DirectoryExists(install_directory))
    return;

  std::set<FilePath> extension_paths;

  file_util::FileEnumerator enumerator(install_directory,
                                       false,  // Not recursive.
                                       file_util::FileEnumerator::DIRECTORIES);

  for (FilePath extension_path = enumerator.Next(); !extension_path.empty();
       extension_path = enumerator.Next()) {
    std::string extension_id = extension_path.BaseName().MaybeAsASCII();
    if (!Extension::IdIsValid(extension_id))
      extension_id.clear();

    // Delete directories that aren't valid IDs.
    if (extension_id.empty()) {
      DVLOG(1) << "Deleting invalid extension directory "
               << extension_path.value() << ".";
      file_util::Delete(extension_path, true);  // Recursive.
      continue;
    }

    extension_paths.insert(extension_path);
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(
           &ExtensionGarbageCollector::CheckExtensionPreferences,
           base::Unretained(this),
           extension_paths));
}

void ExtensionGarbageCollector::CheckExtensionPreferences(
    const std::set<FilePath>& extension_paths) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  ExtensionPrefs* extension_prefs = extension_service_->extension_prefs();

  if (extension_prefs->pref_service()->ReadOnly())
    return;

  scoped_ptr<ExtensionPrefs::ExtensionsInfo> info(
      extension_prefs->GetInstalledExtensionsInfo());

  std::map<std::string, FilePath> extension_pref_paths;
  for (size_t i = 0; i < info->size(); ++i) {
    extension_pref_paths[info->at(i)->extension_id] =
        info->at(i)->extension_path;
  }

  std::set<std::string> extension_ids;

  // Check each directory for a reference in the preferences.
  for (std::set<FilePath>::const_iterator path = extension_paths.begin();
       path != extension_paths.end();
       ++path) {
    std::string extension_id;
    extension_id = path->BaseName().MaybeAsASCII();
    if (extension_id.empty())
      continue;

    extension_ids.insert(extension_id);

    std::map<std::string, FilePath>::const_iterator iter =
        extension_pref_paths.find(extension_id);

    if (iter == extension_pref_paths.end()) {
      DVLOG(1) << "Deleting unreferenced install for directory "
               << path->LossyDisplayName() << ".";
      content::BrowserThread::PostTask(
          content::BrowserThread::FILE, FROM_HERE,
          base::Bind(
              &extension_file_util::DeleteFile,
              *path,
              true));  // recursive.
      continue;
    }

    // Clean up old version directories.
    content::BrowserThread::PostTask(
        content::BrowserThread::FILE, FROM_HERE,
        base::Bind(
             &ExtensionGarbageCollector::CleanupOldVersionsOnBackgroundThread,
             base::Unretained(this), *path, iter->second.BaseName()));
  }

  // Check each entry in the preferences for an existing path.
  for (std::map<std::string, FilePath>::const_iterator extension =
           extension_pref_paths.begin();
       extension != extension_pref_paths.end();
       ++extension) {
    std::set<std::string>::const_iterator iter = extension_ids.find(
        extension->first);

    if (iter != extension_ids.end())
      continue;

    std::string extension_id = extension->first;

    DVLOG(1) << "Could not access local content for extension with id "
             << extension_id << "; uninstalling extension.";

    // If the extension failed to load fully (e.g. the user deleted an unpacked
    // extension's manifest or the manifest points to the wrong path), we cannot
    // use UninstallExtension, which relies on a valid Extension object.
    if (!extension_service_->GetInstalledExtension(extension_id)) {
      scoped_ptr<ExtensionInfo> info(extension_service_->extension_prefs()
          ->GetInstalledExtensionInfo(extension_id));
      extension_service_->extension_prefs()->OnExtensionUninstalled(
          extension_id, info->extension_location, false);
    } else {
      extension_service_->UninstallExtension(extension_id, false, NULL);
    }
  }
}

void ExtensionGarbageCollector::CleanupOldVersionsOnBackgroundThread(
    const FilePath& path,
    const FilePath& current_version) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  file_util::FileEnumerator versions_enumerator(
      path,
      false,  // Not recursive.
      file_util::FileEnumerator::DIRECTORIES);
  for (FilePath version_dir = versions_enumerator.Next();
       !version_dir.value().empty();
       version_dir = versions_enumerator.Next()) {
    if (version_dir.BaseName() != current_version) {
      DVLOG(1) << "Deleting old version for directory "
               << version_dir.LossyDisplayName() << ".";
      if (!file_util::Delete(version_dir, true))  // Recursive.
        NOTREACHED();
    }
  }
}

}  // namespace extensions
