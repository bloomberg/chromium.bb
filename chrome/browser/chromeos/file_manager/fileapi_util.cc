// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/fileapi_util.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/common/extension.h"
#include "google_apis/drive/task_util.h"
#include "net/base/escape.h"
#include "url/gurl.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/open_file_system_mode.h"
#include "webkit/common/fileapi/file_system_util.h"

using content::BrowserThread;

namespace file_manager {
namespace util {

namespace {

GURL ConvertRelativeFilePathToFileSystemUrl(const base::FilePath& relative_path,
                                            const std::string& extension_id) {
  GURL base_url = storage::GetFileSystemRootURI(
      extensions::Extension::GetBaseURLFromExtensionId(extension_id),
      storage::kFileSystemTypeExternal);
  return GURL(base_url.spec() +
              net::EscapeUrlEncodedData(relative_path.AsUTF8Unsafe(),
                                        false));  // Space to %20 instead of +.
}

// Creates an ErrorDefinition with an error set to |error|.
EntryDefinition CreateEntryDefinitionWithError(base::File::Error error) {
  EntryDefinition result;
  result.error = error;
  return result;
}

// Helper class for performing conversions from file definitions to entry
// definitions. It is possible to do it without a class, but the code would be
// crazy and super tricky.
//
// This class copies the input |file_definition_list|,
// so there is no need to worry about validity of passed |file_definition_list|
// reference. Also, it automatically deletes itself after converting finished,
// or if shutdown is invoked during ResolveURL(). Must be called on UI thread.
class FileDefinitionListConverter {
 public:
  FileDefinitionListConverter(Profile* profile,
                              const std::string& extension_id,
                              const FileDefinitionList& file_definition_list,
                              const EntryDefinitionListCallback& callback);
  ~FileDefinitionListConverter() {}

 private:
  // Converts the element under the iterator to an entry. First, converts
  // the virtual path to an URL, and calls OnResolvedURL(). In case of error
  // calls OnIteratorConverted with an error entry definition.
  void ConvertNextIterator(scoped_ptr<FileDefinitionListConverter> self_deleter,
                           FileDefinitionList::const_iterator iterator);

  // Creates an entry definition from the URL as well as the file definition.
  // Then, calls OnIteratorConverted with the created entry definition.
  void OnResolvedURL(scoped_ptr<FileDefinitionListConverter> self_deleter,
                     FileDefinitionList::const_iterator iterator,
                     base::File::Error error,
                     const storage::FileSystemInfo& info,
                     const base::FilePath& file_path,
                     storage::FileSystemContext::ResolvedEntryType type);

  // Called when the iterator is converted. Adds the |entry_definition| to
  // |results_| and calls ConvertNextIterator() for the next element.
  void OnIteratorConverted(scoped_ptr<FileDefinitionListConverter> self_deleter,
                           FileDefinitionList::const_iterator iterator,
                           const EntryDefinition& entry_definition);

  scoped_refptr<storage::FileSystemContext> file_system_context_;
  const std::string extension_id_;
  const FileDefinitionList file_definition_list_;
  const EntryDefinitionListCallback callback_;
  scoped_ptr<EntryDefinitionList> result_;
};

FileDefinitionListConverter::FileDefinitionListConverter(
    Profile* profile,
    const std::string& extension_id,
    const FileDefinitionList& file_definition_list,
    const EntryDefinitionListCallback& callback)
    : extension_id_(extension_id),
      file_definition_list_(file_definition_list),
      callback_(callback),
      result_(new EntryDefinitionList) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // File browser APIs are meant to be used only from extension context, so
  // the extension's site is the one in whose file system context the virtual
  // path should be found.
  GURL site = extensions::util::GetSiteForExtensionId(extension_id_, profile);
  file_system_context_ =
      content::BrowserContext::GetStoragePartitionForSite(
          profile, site)->GetFileSystemContext();

  // Deletes the converter, once the scoped pointer gets out of scope. It is
  // either, if the conversion is finished, or ResolveURL() is terminated, and
  // the callback not called because of shutdown.
  scoped_ptr<FileDefinitionListConverter> self_deleter(this);
  ConvertNextIterator(self_deleter.Pass(), file_definition_list_.begin());
}

void FileDefinitionListConverter::ConvertNextIterator(
    scoped_ptr<FileDefinitionListConverter> self_deleter,
    FileDefinitionList::const_iterator iterator) {
  if (iterator == file_definition_list_.end()) {
    // The converter object will be destroyed since |self_deleter| gets out of
    // scope.
    callback_.Run(result_.Pass());
    return;
  }

  if (!file_system_context_.get()) {
    OnIteratorConverted(self_deleter.Pass(),
                        iterator,
                        CreateEntryDefinitionWithError(
                            base::File::FILE_ERROR_INVALID_OPERATION));
    return;
  }

  storage::FileSystemURL url = file_system_context_->CreateCrackedFileSystemURL(
      extensions::Extension::GetBaseURLFromExtensionId(extension_id_),
      storage::kFileSystemTypeExternal,
      iterator->virtual_path);
  DCHECK(url.is_valid());

  // The converter object will be deleted if the callback is not called because
  // of shutdown during ResolveURL().
  file_system_context_->ResolveURL(
      url,
      base::Bind(&FileDefinitionListConverter::OnResolvedURL,
                 base::Unretained(this),
                 base::Passed(&self_deleter),
                 iterator));
}

void FileDefinitionListConverter::OnResolvedURL(
    scoped_ptr<FileDefinitionListConverter> self_deleter,
    FileDefinitionList::const_iterator iterator,
    base::File::Error error,
    const storage::FileSystemInfo& info,
    const base::FilePath& file_path,
    storage::FileSystemContext::ResolvedEntryType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (error != base::File::FILE_OK) {
    OnIteratorConverted(self_deleter.Pass(),
                        iterator,
                        CreateEntryDefinitionWithError(error));
    return;
  }

  EntryDefinition entry_definition;
  entry_definition.file_system_root_url = info.root_url.spec();
  entry_definition.file_system_name = info.name;
  switch (type) {
    case storage::FileSystemContext::RESOLVED_ENTRY_FILE:
      entry_definition.is_directory = false;
      break;
    case storage::FileSystemContext::RESOLVED_ENTRY_DIRECTORY:
      entry_definition.is_directory = true;
      break;
    case storage::FileSystemContext::RESOLVED_ENTRY_NOT_FOUND:
      entry_definition.is_directory = iterator->is_directory;
      break;
  }
  entry_definition.error = base::File::FILE_OK;

  // Construct a target Entry.fullPath value from the virtual path and the
  // root URL. Eg. Downloads/A/b.txt -> A/b.txt.
  const base::FilePath root_virtual_path =
      file_system_context_->CrackURL(info.root_url).virtual_path();
  DCHECK(root_virtual_path == iterator->virtual_path ||
         root_virtual_path.IsParent(iterator->virtual_path));
  base::FilePath full_path;
  root_virtual_path.AppendRelativePath(iterator->virtual_path, &full_path);
  entry_definition.full_path = full_path;

  OnIteratorConverted(self_deleter.Pass(), iterator, entry_definition);
}

void FileDefinitionListConverter::OnIteratorConverted(
    scoped_ptr<FileDefinitionListConverter> self_deleter,
    FileDefinitionList::const_iterator iterator,
    const EntryDefinition& entry_definition) {
  result_->push_back(entry_definition);
  ConvertNextIterator(self_deleter.Pass(), ++iterator);
}

// Helper function to return the converted definition entry directly, without
// the redundant container.
void OnConvertFileDefinitionDone(
    const EntryDefinitionCallback& callback,
    scoped_ptr<EntryDefinitionList> entry_definition_list) {
  DCHECK_EQ(1u, entry_definition_list->size());
  callback.Run(entry_definition_list->at(0));
}

// Used to implement CheckIfDirectoryExists().
void CheckIfDirectoryExistsOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const GURL& url,
    const storage::FileSystemOperationRunner::StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  storage::FileSystemURL file_system_url = file_system_context->CrackURL(url);
  file_system_context->operation_runner()->DirectoryExists(
      file_system_url, callback);
}

}  // namespace

EntryDefinition::EntryDefinition() {
}

EntryDefinition::~EntryDefinition() {
}

storage::FileSystemContext* GetFileSystemContextForExtensionId(
    Profile* profile,
    const std::string& extension_id) {
  GURL site = extensions::util::GetSiteForExtensionId(extension_id, profile);
  return content::BrowserContext::GetStoragePartitionForSite(profile, site)->
      GetFileSystemContext();
}

storage::FileSystemContext* GetFileSystemContextForRenderViewHost(
    Profile* profile,
    content::RenderViewHost* render_view_host) {
  content::SiteInstance* site_instance = render_view_host->GetSiteInstance();
  return content::BrowserContext::GetStoragePartition(profile, site_instance)->
      GetFileSystemContext();
}

base::FilePath ConvertDrivePathToRelativeFileSystemPath(
    Profile* profile,
    const std::string& extension_id,
    const base::FilePath& drive_path) {
  // "/special/drive-xxx"
  base::FilePath path = drive::util::GetDriveMountPointPath(profile);
  // appended with (|drive_path| - "drive").
  drive::util::GetDriveGrandRootPath().AppendRelativePath(drive_path, &path);

  base::FilePath relative_path;
  ConvertAbsoluteFilePathToRelativeFileSystemPath(profile,
                                                  extension_id,
                                                  path,
                                                  &relative_path);
  return relative_path;
}

GURL ConvertDrivePathToFileSystemUrl(Profile* profile,
                                     const base::FilePath& drive_path,
                                     const std::string& extension_id) {
  const base::FilePath relative_path =
      ConvertDrivePathToRelativeFileSystemPath(profile, extension_id,
                                               drive_path);
  if (relative_path.empty())
    return GURL();
  return ConvertRelativeFilePathToFileSystemUrl(relative_path, extension_id);
}

bool ConvertAbsoluteFilePathToFileSystemUrl(Profile* profile,
                                            const base::FilePath& absolute_path,
                                            const std::string& extension_id,
                                            GURL* url) {
  base::FilePath relative_path;
  if (!ConvertAbsoluteFilePathToRelativeFileSystemPath(profile,
                                                       extension_id,
                                                       absolute_path,
                                                       &relative_path)) {
    return false;
  }
  *url = ConvertRelativeFilePathToFileSystemUrl(relative_path, extension_id);
  return true;
}

bool ConvertAbsoluteFilePathToRelativeFileSystemPath(
    Profile* profile,
    const std::string& extension_id,
    const base::FilePath& absolute_path,
    base::FilePath* virtual_path) {
  // File browser APIs are meant to be used only from extension context, so the
  // extension's site is the one in whose file system context the virtual path
  // should be found.
  GURL site = extensions::util::GetSiteForExtensionId(extension_id, profile);
  storage::ExternalFileSystemBackend* backend =
      content::BrowserContext::GetStoragePartitionForSite(profile, site)
          ->GetFileSystemContext()
          ->external_backend();
  if (!backend)
    return false;

  // Find if this file path is managed by the external backend.
  if (!backend->GetVirtualPath(absolute_path, virtual_path))
    return false;

  return true;
}

void ConvertFileDefinitionListToEntryDefinitionList(
    Profile* profile,
    const std::string& extension_id,
    const FileDefinitionList& file_definition_list,
    const EntryDefinitionListCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // The converter object destroys itself.
  new FileDefinitionListConverter(
      profile, extension_id, file_definition_list, callback);
}

void ConvertFileDefinitionToEntryDefinition(
    Profile* profile,
    const std::string& extension_id,
    const FileDefinition& file_definition,
    const EntryDefinitionCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  FileDefinitionList file_definition_list;
  file_definition_list.push_back(file_definition);
  ConvertFileDefinitionListToEntryDefinitionList(
      profile,
      extension_id,
      file_definition_list,
      base::Bind(&OnConvertFileDefinitionDone, callback));
}

void CheckIfDirectoryExists(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const GURL& url,
    const storage::FileSystemOperationRunner::StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Check the existence of directory using file system API implementation on
  // behalf of the file manager app. We need to grant access beforehand.
  storage::ExternalFileSystemBackend* backend =
      file_system_context->external_backend();
  DCHECK(backend);
  backend->GrantFullAccessToExtension(kFileManagerAppId);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&CheckIfDirectoryExistsOnIOThread,
                 file_system_context,
                 url,
                 google_apis::CreateRelayCallback(callback)));
}

}  // namespace util
}  // namespace file_manager
