// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_file_system.h"

#include <vector>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/utf_string_conversions.h"
#include "base/platform_file.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/chromeos/gdata/gdata.h"
#include "chrome/browser/chromeos/gdata/gdata_parser.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/escape.h"
#include "webkit/fileapi/file_system_file_util_proxy.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_util.h"

using content::BrowserThread;

namespace {

// Content refresh time.
const int kRefreshTimeInSec = 5*60;

const char kGDataRootDirectory[] = "gdata";
const char kFeedField[] = "feed";

// Converts gdata error code into file platform error code.
base::PlatformFileError GDataToPlatformError(gdata::GDataErrorCode status) {
  switch (status) {
    case gdata::HTTP_SUCCESS:
    case gdata::HTTP_CREATED:
      return base::PLATFORM_FILE_OK;
    case gdata::HTTP_UNAUTHORIZED:
    case gdata::HTTP_FORBIDDEN:
      return base::PLATFORM_FILE_ERROR_ACCESS_DENIED;
    case gdata::HTTP_NOT_FOUND:
      return base::PLATFORM_FILE_ERROR_NOT_FOUND;
    case gdata::GDATA_PARSE_ERROR:
    case gdata::GDATA_FILE_ERROR:
      return base::PLATFORM_FILE_ERROR_ABORT;
    default:
      return base::PLATFORM_FILE_ERROR_FAILED;
  }
}

// Escapes forward slashes from file names with magic unicode character \u2215
// pretty much looks the same in UI.
std::string EscapeUtf8FileName(const std::string& input) {
  std::string output;
  if (ReplaceChars(input, "/", std::string("\xE2\x88\x95"), &output))
    return output;

  return input;
}

}  // namespace

namespace gdata {

// FindFileDelegate class implementation.

FindFileDelegate::~FindFileDelegate() {
}

// ReadOnlyFindFileDelegate class implementation.

ReadOnlyFindFileDelegate::ReadOnlyFindFileDelegate() : file_(NULL) {
}

void ReadOnlyFindFileDelegate::OnFileFound(gdata::GDataFile* file) {
  // file_ should be set only once since OnFileFound() is a terminal
  // function.
  DCHECK(!file_);
  DCHECK(!file->file_info().is_directory);
  file_ = file;
}

void ReadOnlyFindFileDelegate::OnDirectoryFound(const FilePath&,
                                                GDataDirectory* dir) {
  // file_ should be set only once since OnDirectoryFound() is a terminal
  // function.
  DCHECK(!file_);
  DCHECK(dir->file_info().is_directory);
  file_ = dir;
}

FindFileDelegate::FindFileTraversalCommand
ReadOnlyFindFileDelegate::OnEnterDirectory(const FilePath&, GDataDirectory*) {
  // Keep traversing while doing read only lookups.
  return FIND_FILE_CONTINUES;
}

void ReadOnlyFindFileDelegate::OnError(base::PlatformFileError) {
  file_ = NULL;
}

// FindFileDelegateReplyBase class implementation.

FindFileDelegateReplyBase::FindFileDelegateReplyBase(
      GDataFileSystem* file_system,
      const FilePath& search_file_path,
      bool require_content)
      : file_system_(file_system),
        search_file_path_(search_file_path),
        require_content_(require_content) {
  reply_message_proxy_ = base::MessageLoopProxy::current();
}

FindFileDelegateReplyBase::~FindFileDelegateReplyBase() {
}

FindFileDelegate::FindFileTraversalCommand
FindFileDelegateReplyBase::OnEnterDirectory(
      const FilePath& current_directory_path,
      GDataDirectory* current_dir) {
  return CheckAndRefreshContent(current_directory_path, current_dir) ?
             FIND_FILE_CONTINUES : FIND_FILE_TERMINATES;
}

// Checks if the content of the |directory| under |directory_path| needs to be
// refreshed. Returns true if directory content is fresh, otherwise it kicks
// off content request request. After feed content content is received and
// processed in GDataFileSystem::OnGetDocuments(), that function will also
// restart the initiated FindFileByPath() request.
bool FindFileDelegateReplyBase::CheckAndRefreshContent(
    const FilePath& directory_path, GDataDirectory* directory) {
  GURL feed_url;
  if (directory->NeedsRefresh(&feed_url)) {
    // If content is stale/non-existing, first fetch the content of the
    // directory in order to traverse it further.
    file_system_->StartDirectoryRefresh(
        GDataFileSystem::FindFileParams(
            search_file_path_,
            require_content_,
            directory_path,
            feed_url,
            true,    /* is_initial_feed */
            this));
    return false;
  }
  return true;
}

// GDataFileBase class.

GDataFileBase::GDataFileBase(GDataDirectory* parent) : parent_(parent) {
}

GDataFileBase::~GDataFileBase() {
}

GDataFile* GDataFileBase::AsGDataFile() {
  return NULL;
}

FilePath GDataFileBase::GetFilePath() {
  FilePath path;
  std::vector<FilePath::StringType> parts;
  for (GDataFileBase* file = this; file != NULL; file = file->parent())
    parts.push_back(file->file_name());

  // Paste paths parts back together in reverse order from upward tree
  // traversal.
  for (std::vector<FilePath::StringType>::reverse_iterator iter =
           parts.rbegin();
       iter != parts.rend(); ++iter) {
    path = path.Append(*iter);
  }
  return path;
}

GDataDirectory* GDataFileBase::AsGDataDirectory() {
  return NULL;
}


GDataFileBase* GDataFileBase::FromDocumentEntry(GDataDirectory* parent,
                                                DocumentEntry* doc) {
  DCHECK(parent);
  DCHECK(doc);
  if (doc->is_folder())
    return GDataDirectory::FromDocumentEntry(parent, doc);
  else if (doc->is_hosted_document() || doc->is_file())
    return GDataFile::FromDocumentEntry(parent, doc);

  return NULL;
}

GDataFileBase* GDataFile::FromDocumentEntry(GDataDirectory* parent,
                                            DocumentEntry* doc) {
  DCHECK(doc->is_hosted_document() || doc->is_file());
  GDataFile* file = new GDataFile(parent);
  // Check if this entry is a true file, or...
  if (doc->is_file()) {
    file->original_file_name_ = UTF16ToUTF8(doc->filename());
    file->file_name_ =
        EscapeUtf8FileName(file->original_file_name_);
    file->file_info_.size = doc->file_size();
    file->file_md5_ = doc->file_md5();
  } else {
    DCHECK(doc->is_hosted_document());
    // ... a hosted document.
    file->original_file_name_ = UTF16ToUTF8(doc->title());
    // Attach .g<something> extension to hosted documents so we can special
    // case their handling in UI.
    // TODO(zelidrag): Figure out better way how to pass entry info like kind
    // to UI through the File API stack.
    file->file_name_ = EscapeUtf8FileName(
        file->original_file_name_ + doc->GetHostedDocumentExtension());
    // We don't know the size of hosted docs and it does not matter since
    // is has no effect on the quota.
    file->file_info_.size = 0;
  }
  file->kind_ = doc->kind();
  const Link* self_link = doc->GetLinkByType(Link::SELF);
  if (self_link)
    file->self_url_ = self_link->href();
  file->content_url_ = doc->content_url();
  file->content_mime_type_ = doc->content_mime_type();
  file->etag_ = doc->etag();
  file->resource_id_ = doc->resource_id();
  file->id_ = doc->id();
  file->file_info_.last_modified = doc->updated_time();
  file->file_info_.last_accessed = doc->updated_time();
  file->file_info_.creation_time = doc->published_time();

  const Link* thumbnail_link = doc->GetLinkByType(Link::THUMBNAIL);
  if (thumbnail_link)
    file->thumbnail_url_ = thumbnail_link->href();

  const Link* alternate_link = doc->GetLinkByType(Link::ALTERNATE);
  if (alternate_link)
    file->edit_url_ = alternate_link->href();

  // TODO(gspencer): Add support for fetching cache state from the cache,
  // when the cache code is done.

  return file;
}

// GDataFile class implementation.

GDataFile::GDataFile(GDataDirectory* parent)
    : GDataFileBase(parent),
      kind_(gdata::DocumentEntry::UNKNOWN),
      cache_state_(CACHE_STATE_NONE) {
  DCHECK(parent);
}

GDataFile::~GDataFile() {
}

GDataFile* GDataFile::AsGDataFile() {
  return this;
}

// GDataDirectory class implementation.

GDataDirectory::GDataDirectory(GDataDirectory* parent) : GDataFileBase(parent) {
  file_info_.is_directory = true;
}

GDataDirectory::~GDataDirectory() {
  RemoveChildren();
}

GDataDirectory* GDataDirectory::AsGDataDirectory() {
  return this;
}

// static
GDataFileBase* GDataDirectory::FromDocumentEntry(GDataDirectory* parent,
                                                 DocumentEntry* doc) {
  DCHECK(parent);
  DCHECK(doc->is_folder());
  GDataDirectory* dir = new GDataDirectory(parent);
  dir->original_file_name_ = UTF16ToUTF8(doc->title());
  dir->file_name_ = EscapeUtf8FileName(dir->original_file_name_);
  dir->file_info_.last_modified = doc->updated_time();
  dir->file_info_.last_accessed = doc->updated_time();
  dir->file_info_.creation_time = doc->published_time();
  // Extract feed link.
  dir->start_feed_url_ = doc->content_url();
  dir->content_url_ = doc->content_url();

  const Link* upload_link = doc->GetLinkByType(Link::RESUMABLE_CREATE_MEDIA);
  if (upload_link)
    dir->upload_url_ = upload_link->href();

  return dir;
}

void GDataDirectory::RemoveChildren() {
  STLDeleteValues(&children_);
  children_.clear();
}

bool GDataDirectory::NeedsRefresh(GURL* feed_url) {
  if ((base::Time::Now() - refresh_time_).InSeconds() < kRefreshTimeInSec)
    return false;

  *feed_url = start_feed_url_;
  return true;
}

void GDataDirectory::AddFile(GDataFileBase* file) {
  // Do file name de-duplication - find files with the same name and
  // append a name modifier to the name.
  int max_modifier = 1;
  FilePath full_file_name(file->file_name());
  std::string extension = full_file_name.Extension();
  std::string file_name = full_file_name.RemoveExtension().value();
  while (children_.find(full_file_name.value()) !=  children_.end()) {
    if (!extension.empty()) {
      full_file_name = FilePath(base::StringPrintf("%s (%d)%s",
                                                   file_name.c_str(),
                                                   ++max_modifier,
                                                   extension.c_str()));
    } else {
      full_file_name = FilePath(base::StringPrintf("%s (%d)",
                                                   file_name.c_str(),
                                                   ++max_modifier));
    }
  }
  if (full_file_name.value() != file->file_name())
    file->set_file_name(full_file_name.value());
  children_.insert(std::make_pair(file->file_name(), file));
}

bool GDataDirectory::RemoveFile(GDataFileBase* file) {
  GDataFileCollection::iterator iter = children_.find(file->file_name());
  if (children_.find(file->file_name()) ==  children_.end())
    return false;

  DCHECK(iter->second);
  delete iter->second;
  children_.erase(iter);
  return true;
}

// GDataFileSystem::FindFileParams struct implementation.

GDataFileSystem::FindFileParams::FindFileParams(
    const FilePath& in_file_path,
    bool in_require_content,
    const FilePath& in_directory_path,
    const GURL& in_feed_url,
    bool in_initial_feed,
    scoped_refptr<FindFileDelegate> in_delegate)
    : file_path(in_file_path),
      require_content(in_require_content),
      directory_path(in_directory_path),
      feed_url(in_feed_url),
      initial_feed(in_initial_feed),
      delegate(in_delegate) {
}

GDataFileSystem::FindFileParams::~FindFileParams() {
}

// GDataFileSystem::CreateDirectoryParams struct implementation.

GDataFileSystem::CreateDirectoryParams::CreateDirectoryParams(
    const FilePath& created_directory_path,
    const FilePath& target_directory_path,
    bool is_exclusive,
    bool is_recursive,
    const FileOperationCallback& callback)
    : created_directory_path(created_directory_path),
      target_directory_path(target_directory_path),
      is_exclusive(is_exclusive),
      is_recursive(is_recursive),
      callback(callback) {
}

GDataFileSystem::CreateDirectoryParams::~CreateDirectoryParams() {
}


// GDataFileSystem class implementatsion.

GDataFileSystem::GDataFileSystem(Profile* profile)
    : profile_(profile),
      documents_service_(new DocumentsService),
      uploader_(new GDataUploader(ALLOW_THIS_IN_INITIALIZER_LIST(this))),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  documents_service_->Initialize(profile_);
  uploader_->Initialize(profile_);
  root_.reset(new GDataDirectory(NULL));
  root_->set_file_name(kGDataRootDirectory);

  // Should be created from the file browser extension API on UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

GDataFileSystem::~GDataFileSystem() {
  // Should be deleted as part of Profile on UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void GDataFileSystem::Shutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Cancel all the in-flight operations.
  // This asynchronously cancels the URL fetch operations.
  documents_service_->CancelAll();
}

void GDataFileSystem::Authenticate(const AuthStatusCallback& callback) {
  // TokenFetcher, used in DocumentsService, must be run on UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  documents_service_->Authenticate(callback);
}

void GDataFileSystem::FindFileByPath(
    const FilePath& file_path, scoped_refptr<FindFileDelegate> delegate) {
  base::AutoLock lock(lock_);
  UnsafeFindFileByPath(file_path, delegate);
}

void GDataFileSystem::StartDirectoryRefresh(
    const FindFileParams& params) {
  // Kick off document feed fetching here if we don't have complete data
  // to finish this call.
  documents_service_->GetDocuments(
      params.feed_url,
      base::Bind(&GDataFileSystem::OnGetDocuments,
                 weak_ptr_factory_.GetWeakPtr(),
                 params));
}

void GDataFileSystem::Remove(const FilePath& file_path,
    bool is_recursive,
    const FileOperationCallback& callback) {
  GDataFileBase* file_info = GetGDataFileInfoFromPath(file_path);
  if (!file_info) {
    if (!callback.is_null()) {
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND));
    }
    return;
  }

  documents_service_->DeleteDocument(
      file_info->self_url(),
      base::Bind(&GDataFileSystem::OnRemovedDocument,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 file_path));
}

void GDataFileSystem::CreateDirectory(
    const FilePath& directory_path,
    bool is_exclusive,
    bool is_recursive,
    const FileOperationCallback& callback) {
  FilePath last_parent_dir_path;
  FilePath first_missing_path;
  GURL last_parent_dir_url;
  FindMissingDirectoryResult result =
      FindFirstMissingParentDirectory(directory_path,
                                      &last_parent_dir_url,
                                      &first_missing_path);
  switch (result) {
    case FOUND_INVALID: {
      if (!callback.is_null()) {
        MessageLoop::current()->PostTask(FROM_HERE,
            base::Bind(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND));
      }

      return;
    }
    case DIRECTORY_ALREADY_PRESENT: {
      if (!callback.is_null()) {
        MessageLoop::current()->PostTask(FROM_HERE,
            base::Bind(callback,
                       is_exclusive ? base::PLATFORM_FILE_ERROR_EXISTS :
                                      base::PLATFORM_FILE_OK));
      }

      return;
    }
    case FOUND_MISSING: {
      // There is a missing folder to be created here, move on with the rest of
      // this function.
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }

  // Do we have a parent directory here as well? We can't then create target
  // directory if this is not a recursive operation.
  if (directory_path !=  first_missing_path && !is_recursive) {
    if (!callback.is_null()) {
      MessageLoop::current()->PostTask(FROM_HERE,
           base::Bind(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND));
    }
    return;
  }

  documents_service_->CreateDirectory(
      last_parent_dir_url,
      first_missing_path.BaseName().value(),
      base::Bind(&GDataFileSystem::OnCreateDirectoryCompleted,
                 weak_ptr_factory_.GetWeakPtr(),
                 CreateDirectoryParams(
                     first_missing_path,
                     directory_path,
                     is_exclusive,
                     is_recursive,
                     callback)));
}

void GDataFileSystem::GetFile(const FilePath& file_path,
                              const GetFileCallback& callback) {
  GDataFileBase* file_info = GetGDataFileInfoFromPath(file_path);
  if (!file_info) {
    if (!callback.is_null()) {
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(callback,
                     base::PLATFORM_FILE_ERROR_NOT_FOUND,
                     FilePath()));
    }
    return;
  }

  // TODO(satorux): We should get a file from the cache if it's present, but
  // the caching layer is not implemented yet. For now, always download from
  // the cloud.
  documents_service_->DownloadFile(
      file_info->content_url(),
      base::Bind(&GDataFileSystem::OnFileDownloaded,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void GDataFileSystem::InitiateUpload(
    const std::string& file_name,
    const std::string& content_type,
    int64 content_length,
    const FilePath& destination_directory,
    const InitiateUploadOperationCallback& callback) {
  GURL destination_directory_url =
      GetUploadUrlForDirectory(destination_directory);

  if (destination_directory_url.is_empty()) {
    if (!callback.is_null()) {
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(callback,
                     HTTP_BAD_REQUEST, GURL()));
    }
    return;
  }

  documents_service_->InitiateUpload(
          InitiateUploadParams(file_name,
                                 content_type,
                                 content_length,
                                 destination_directory_url),
          base::Bind(&GDataFileSystem::OnUploadLocationReceived,
                     weak_ptr_factory_.GetWeakPtr(),
                     callback,
                     // MessageLoopProxy is used to run |callback| on the
                     // thread where this function was called.
                     base::MessageLoopProxy::current()));
}

void GDataFileSystem::OnUploadLocationReceived(
    const InitiateUploadOperationCallback& callback,
    scoped_refptr<base::MessageLoopProxy> message_loop_proxy,
    GDataErrorCode code,
    const GURL& upload_location) {

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!callback.is_null()) {
    message_loop_proxy->PostTask(FROM_HERE, base::Bind(callback, code,
                                                       upload_location));
  }
}

void GDataFileSystem::ResumeUpload(
    const ResumeUploadParams& params,
    const ResumeUploadOperationCallback& callback) {
  documents_service_->ResumeUpload(
          params,
          base::Bind(&GDataFileSystem::OnResumeUpload,
                     weak_ptr_factory_.GetWeakPtr(),
                     callback,
                     // MessageLoopProxy is used to run |callback| on the
                     // thread where this function was called.
                     base::MessageLoopProxy::current()));
}

void GDataFileSystem::OnResumeUpload(
    const ResumeUploadOperationCallback& callback,
    scoped_refptr<base::MessageLoopProxy> message_loop_proxy,
    GDataErrorCode code,
    int64 start_range_received,
    int64 end_range_received) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!callback.is_null()) {
    message_loop_proxy->PostTask(
        FROM_HERE,
        base::Bind(callback, code, start_range_received, end_range_received));
  }
  // TODO(achuith): Figure out when we are done with upload and
  // add appropriate entry to the file system that represents the new file.
}


void GDataFileSystem::UnsafeFindFileByPath(
    const FilePath& file_path, scoped_refptr<FindFileDelegate> delegate) {
  lock_.AssertAcquired();

  std::vector<FilePath::StringType> components;
  file_path.GetComponents(&components);

  GDataDirectory* current_dir = root_.get();
  FilePath directory_path;
  for (size_t i = 0; i < components.size() && current_dir; i++) {
    directory_path = directory_path.Append(current_dir->file_name());

    // Last element must match, if not last then it must be a directory.
    if (i == components.size() - 1) {
      if (current_dir->file_name() == components[i])
        delegate->OnDirectoryFound(directory_path, current_dir);
      else
        delegate->OnError(base::PLATFORM_FILE_ERROR_NOT_FOUND);

      return;
    }

    if (delegate->OnEnterDirectory(directory_path, current_dir) ==
        FindFileDelegate::FIND_FILE_TERMINATES) {
      return;
    }

    // Not the last part of the path, search for the next segment.
    GDataFileCollection::const_iterator file_iter =
        current_dir->children().find(components[i + 1]);
    if (file_iter == current_dir->children().end()) {
      delegate->OnError(base::PLATFORM_FILE_ERROR_NOT_FOUND);
      return;
    }

    // Found file, must be the last segment.
    if (file_iter->second->file_info().is_directory) {
      // Found directory, continue traversal.
      current_dir = file_iter->second->AsGDataDirectory();
    } else {
      if ((i + 1) == (components.size() - 1))
        delegate->OnFileFound(file_iter->second->AsGDataFile());
      else
        delegate->OnError(base::PLATFORM_FILE_ERROR_NOT_FOUND);

      return;
    }
  }
  delegate->OnError(base::PLATFORM_FILE_ERROR_NOT_FOUND);
}

GDataFileBase* GDataFileSystem::GetGDataFileInfoFromPath(
    const FilePath& file_path) {
  base::AutoLock lock(lock_);
  // Find directory element within the cached file system snapshot.
  scoped_refptr<ReadOnlyFindFileDelegate> find_delegate(
      new ReadOnlyFindFileDelegate());
  UnsafeFindFileByPath(file_path, find_delegate);
  if (!find_delegate->file())
    return NULL;

  return find_delegate->file();
}

void GDataFileSystem::OnCreateDirectoryCompleted(
    const CreateDirectoryParams& params,
    GDataErrorCode status,
    scoped_ptr<base::Value> data) {

  base::PlatformFileError error = GDataToPlatformError(status);
  if (error != base::PLATFORM_FILE_OK) {
    if (!params.callback.is_null())
      params.callback.Run(error);

    return;
  }

  base::DictionaryValue* dict_value = NULL;
  base::Value* created_entry = NULL;
  if (data.get() && data->GetAsDictionary(&dict_value) && dict_value)
    dict_value->Get("entry", &created_entry);
  error = AddNewDirectory(params.created_directory_path, created_entry);

  if (error != base::PLATFORM_FILE_OK) {
    if (!params.callback.is_null())
      params.callback.Run(error);

    return;
  }

  // Not done yet with recursive directory creation?
  if (params.target_directory_path != params.created_directory_path &&
      params.is_recursive) {
    CreateDirectory(params.target_directory_path,
                    params.is_exclusive,
                    params.is_recursive,
                    params.callback);
    return;
  }

  if (!params.callback.is_null()) {
    // Finally done with the create request.
    params.callback.Run(base::PLATFORM_FILE_OK);
  }
}

void GDataFileSystem::OnGetDocuments(
    const FindFileParams& params,
    GDataErrorCode status,
    scoped_ptr<base::Value> data) {

  base::PlatformFileError error = GDataToPlatformError(status);

  if (error == base::PLATFORM_FILE_OK &&
      (!data.get() || data->GetType() != Value::TYPE_DICTIONARY)) {
    LOG(WARNING) << "No feed content!";
    error = base::PLATFORM_FILE_ERROR_FAILED;
  }

  if (error != base::PLATFORM_FILE_OK) {
    params.delegate->OnError(error);
    return;
  }

  GURL next_feed_url;
  error = UpdateDirectoryWithDocumentFeed(
     params.directory_path, params.feed_url, data.get(), params.initial_feed,
     &next_feed_url);
  if (error != base::PLATFORM_FILE_OK) {
    params.delegate->OnError(error);
    return;
  }

  // Fetch the rest of the content if the feed is not completed.
  if (!next_feed_url.is_empty()) {
    StartDirectoryRefresh(FindFileParams(params.file_path,
                                         params.require_content,
                                         params.directory_path,
                                         next_feed_url,
                                         false,  /* initial_feed */
                                         params.delegate));
    return;
  }

  // Continue file content search operation.
  FindFileByPath(params.file_path,
                 params.delegate);
}


void GDataFileSystem::OnRemovedDocument(
    const FileOperationCallback& callback,
    const FilePath& file_path,
    GDataErrorCode status,
    const GURL& document_url) {
  base::PlatformFileError error = GDataToPlatformError(status);

  if (error == base::PLATFORM_FILE_OK)
    error = RemoveFileFromFileSystem(file_path);

  if (!callback.is_null()) {
    callback.Run(error);
  }
}

void GDataFileSystem::OnFileDownloaded(
    const GetFileCallback& callback,
    GDataErrorCode status,
    const GURL& content_url,
    const FilePath& file_path) {
  base::PlatformFileError error = GDataToPlatformError(status);

  if (!callback.is_null()) {
    callback.Run(error, file_path);
  }
}

base::PlatformFileError GDataFileSystem::RemoveFileFromFileSystem(
    const FilePath& file_path) {
  // We need to lock here as well (despite FindFileByPath lock) since directory
  // instance below is a 'live' object.
  base::AutoLock lock(lock_);

  // Find directory element within the cached file system snapshot.
  scoped_refptr<ReadOnlyFindFileDelegate> update_delegate(
      new ReadOnlyFindFileDelegate());
  UnsafeFindFileByPath(file_path, update_delegate);

  GDataFileBase* file = update_delegate->file();

  if (!file)
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  // You can't remove root element.
  if (!file->parent())
    return base::PLATFORM_FILE_ERROR_ACCESS_DENIED;

  if (!file->parent()->RemoveFile(file))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  return base::PLATFORM_FILE_OK;
}


base::PlatformFileError GDataFileSystem::UpdateDirectoryWithDocumentFeed(
    const FilePath& directory_path, const GURL& feed_url,
    base::Value* data, bool is_initial_feed, GURL* next_feed) {
  base::DictionaryValue* feed_dict = NULL;
  scoped_ptr<DocumentFeed> feed;
  if (!static_cast<base::DictionaryValue*>(data)->GetDictionary(
          kFeedField, &feed_dict)) {
    return base::PLATFORM_FILE_ERROR_FAILED;
  }

  // Parse the document feed.
  feed.reset(DocumentFeed::CreateFrom(feed_dict));
  if (!feed.get())
    return base::PLATFORM_FILE_ERROR_FAILED;

  // We need to lock here as well (despite FindFileByPath lock) since directory
  // instance below is a 'live' object.
  base::AutoLock lock(lock_);

  // Find directory element within the cached file system snapshot.
  scoped_refptr<ReadOnlyFindFileDelegate> update_delegate(
      new ReadOnlyFindFileDelegate());
  UnsafeFindFileByPath(directory_path, update_delegate);

  GDataFileBase* file = update_delegate->file();
  if (!file)
    return base::PLATFORM_FILE_ERROR_FAILED;

  GDataDirectory* dir = file->AsGDataDirectory();
  if (!dir)
    return base::PLATFORM_FILE_ERROR_FAILED;

  // Get upload url from the root feed. Links for all other collections will be
  // handled in GDatadirectory::FromDocumentEntry();
  if (is_initial_feed && dir == root_.get()) {
    const Link* root_feed_upload_link =
        feed->GetLinkByType(Link::RESUMABLE_CREATE_MEDIA);
    if (root_feed_upload_link)
      dir->set_upload_url(root_feed_upload_link->href());
  }

  dir->set_start_feed_url(feed_url);
  dir->set_refresh_time(base::Time::Now());
  if (feed->GetNextFeedURL(next_feed))
    dir->set_next_feed_url(*next_feed);

  // Remove all child elements if we are refreshing the entire content.
  if (is_initial_feed)
    dir->RemoveChildren();

  for (ScopedVector<DocumentEntry>::const_iterator iter =
           feed->entries().begin();
       iter != feed->entries().end(); ++iter) {
    DocumentEntry* doc = *iter;

    // For now, skip elements of the root directory feed that have parent.
    // TODO(zelidrag): In theory, we could reconstruct the entire FS snapshot
    // of the root file feed only instead of fetching one dir/collection at the
    // time.
    if (dir == root_.get()) {
      const Link* parent_link = doc->GetLinkByType(Link::PARENT);
      if (parent_link)
        continue;
    }

    GDataFileBase* file = GDataFileBase::FromDocumentEntry(dir, doc);
    if (file)
      dir->AddFile(file);
  }
  return base::PLATFORM_FILE_OK;
}

base::PlatformFileError GDataFileSystem::AddNewDirectory(
    const FilePath& directory_path, base::Value* entry_value) {
  if (!entry_value)
    return base::PLATFORM_FILE_ERROR_FAILED;

  scoped_ptr<DocumentEntry> entry(DocumentEntry::CreateFrom(entry_value));

  if (!entry.get())
    return base::PLATFORM_FILE_ERROR_FAILED;

  // We need to lock here as well (despite FindFileByPath lock) since directory
  // instance below is a 'live' object.
  base::AutoLock lock(lock_);

  // Find parent directory element within the cached file system snapshot.
  scoped_refptr<ReadOnlyFindFileDelegate> update_delegate(
      new ReadOnlyFindFileDelegate());
  UnsafeFindFileByPath(directory_path.DirName(), update_delegate);

  GDataFileBase* file = update_delegate->file();
  if (!file)
    return base::PLATFORM_FILE_ERROR_FAILED;

  // Check if parent is a directory since in theory since this is a callback
  // something could in the meantime have nuked the parent dir and created a
  // file with the exact same name.
  GDataDirectory* parent_dir = file->AsGDataDirectory();
  if (!parent_dir)
    return base::PLATFORM_FILE_ERROR_FAILED;

  GDataFileBase* new_file = GDataFileBase::FromDocumentEntry(parent_dir,
                                                             entry.get());
  if (!new_file)
    return base::PLATFORM_FILE_ERROR_FAILED;

  parent_dir->AddFile(new_file);

  return base::PLATFORM_FILE_OK;
}


GDataFileSystem::FindMissingDirectoryResult
GDataFileSystem::FindFirstMissingParentDirectory(
    const FilePath& directory_path,
    GURL* last_dir_content_url,
    FilePath* first_missing_parent_path) {
  // Let's find which how deep is the existing directory structure and
  // get the first element that's missing.
  std::vector<FilePath::StringType> path_parts;
  directory_path.GetComponents(&path_parts);
  FilePath current_path;

  base::AutoLock lock(lock_);
  for (std::vector<FilePath::StringType>::const_iterator iter =
          path_parts.begin();
       iter != path_parts.end(); ++iter) {
    current_path = current_path.Append(*iter);
    scoped_refptr<ReadOnlyFindFileDelegate> find_delegate(
        new ReadOnlyFindFileDelegate());
    UnsafeFindFileByPath(current_path, find_delegate);
    if (find_delegate->file()) {
      if (find_delegate->file()->file_info().is_directory) {
        *last_dir_content_url = find_delegate->file()->content_url();
      } else {
        // Huh, the segment found is a file not a directory?
        return FOUND_INVALID;
      }
    } else {
      *first_missing_parent_path = current_path;
      return FOUND_MISSING;
    }
  }
  return DIRECTORY_ALREADY_PRESENT;
}

GURL GDataFileSystem::GetUploadUrlForDirectory(
    const FilePath& destination_directory) {
  // Find directory element within the cached file system snapshot.
  scoped_refptr<ReadOnlyFindFileDelegate> find_delegate(
      new ReadOnlyFindFileDelegate());
  base::AutoLock lock(lock_);
  UnsafeFindFileByPath(destination_directory, find_delegate);
  GDataDirectory* dir = find_delegate->file() ?
      find_delegate->file()->AsGDataDirectory() : NULL;
  return dir ? dir->upload_url() : GURL();
}

// static
GDataFileSystem* GDataFileSystemFactory::GetForProfile(
    Profile* profile) {
  return static_cast<GDataFileSystem*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
GDataFileSystemFactory* GDataFileSystemFactory::GetInstance() {
  return Singleton<GDataFileSystemFactory>::get();
}

GDataFileSystemFactory::GDataFileSystemFactory()
    : ProfileKeyedServiceFactory("GDataFileSystem",
                                 ProfileDependencyManager::GetInstance()) {
}

GDataFileSystemFactory::~GDataFileSystemFactory() {
}

ProfileKeyedService* GDataFileSystemFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new GDataFileSystem(profile);
}

}  // namespace gdata
