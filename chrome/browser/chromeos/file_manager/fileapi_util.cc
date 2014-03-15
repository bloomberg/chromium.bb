// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/fileapi_util.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/common/extension.h"
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
  GURL base_url = fileapi::GetFileSystemRootURI(
      extensions::Extension::GetBaseURLFromExtensionId(extension_id),
      fileapi::kFileSystemTypeExternal);
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
                     const GURL& root_url,
                     const std::string& name,
                     base::File::Error error);

  // Called when the iterator is converted. Adds the |entry_definition| to
  // |results_| and calls ConvertNextIterator() for the next element.
  void OnIteratorConverted(scoped_ptr<FileDefinitionListConverter> self_deleter,
                           FileDefinitionList::const_iterator iterator,
                           const EntryDefinition& entry_definition);

  scoped_refptr<fileapi::FileSystemContext> file_system_context_;
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

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

  fileapi::ExternalFileSystemBackend* backend =
      file_system_context_->external_backend();
  if (!backend) {
    OnIteratorConverted(self_deleter.Pass(),
                        iterator,
                        CreateEntryDefinitionWithError(
                            base::File::FILE_ERROR_INVALID_OPERATION));
    return;
  }

  fileapi::FileSystemURL url = file_system_context_->CreateCrackedFileSystemURL(
      extensions::Extension::GetBaseURLFromExtensionId(extension_id_),
      fileapi::kFileSystemTypeExternal,
      iterator->virtual_path);
  DCHECK(url.is_valid());

  // The converter object will be deleted if the callback is not called because
  // of shutdown during ResolveURL().
  //
  // TODO(mtomasz, nhiroki): Call FileSystemContext::ResolveURL() directly,
  // after removing redundant thread restrictions in there.
  backend->ResolveURL(
      url,
      // Not sandboxed file systems are not creatable via ResolveURL().
      fileapi::OPEN_FILE_SYSTEM_FAIL_IF_NONEXISTENT,
      base::Bind(&FileDefinitionListConverter::OnResolvedURL,
                 base::Unretained(this),
                 base::Passed(&self_deleter),
                 iterator));
}

void FileDefinitionListConverter::OnResolvedURL(
    scoped_ptr<FileDefinitionListConverter> self_deleter,
    FileDefinitionList::const_iterator iterator,
    const GURL& root_url,
    const std::string& name,
    base::File::Error error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  EntryDefinition entry_definition;
  entry_definition.file_system_root_url = root_url.spec();
  entry_definition.file_system_name = name;
  entry_definition.is_directory = iterator->is_directory;
  entry_definition.error = error;

  // Construct a target Entry.fullPath value from the virtual path and the
  // root URL. Eg. Downloads/A/b.txt -> A/b.txt.
  const base::FilePath root_virtual_path =
      file_system_context_->CrackURL(root_url).virtual_path();
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

}  // namespace

EntryDefinition::EntryDefinition() {
}

EntryDefinition::~EntryDefinition() {
}

fileapi::FileSystemContext* GetFileSystemContextForExtensionId(
    Profile* profile,
    const std::string& extension_id) {
  GURL site = extensions::util::GetSiteForExtensionId(extension_id, profile);
  return content::BrowserContext::GetStoragePartitionForSite(profile, site)->
      GetFileSystemContext();
}

fileapi::FileSystemContext* GetFileSystemContextForRenderViewHost(
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
  fileapi::ExternalFileSystemBackend* backend =
      content::BrowserContext::GetStoragePartitionForSite(profile, site)->
          GetFileSystemContext()->external_backend();
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // The converter object destroys itself.
  new FileDefinitionListConverter(
      profile, extension_id, file_definition_list, callback);
}

void ConvertFileDefinitionToEntryDefinition(
    Profile* profile,
    const std::string& extension_id,
    const FileDefinition& file_definition,
    const EntryDefinitionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FileDefinitionList file_definition_list;
  file_definition_list.push_back(file_definition);
  ConvertFileDefinitionListToEntryDefinitionList(
      profile,
      extension_id,
      file_definition_list,
      base::Bind(&OnConvertFileDefinitionDone, callback));
}

}  // namespace util
}  // namespace file_manager
