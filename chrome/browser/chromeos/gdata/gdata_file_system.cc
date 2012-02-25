// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_file_system.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/utf_string_conversions.h"
#include "base/platform_file.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "base/values.h"
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

// Escapes file names since hosted documents from gdata can actually have
// forward slashes in their titles.
std::string EscapeFileName(const std::string& input) {
  std::string tmp;
  std::string output;
  if (ReplaceChars(input, "%", std::string("%25"), &tmp) &&
      ReplaceChars(tmp, "/", std::string("%2F"), &output)) {
    return output;
  }

  return input;
}

}  // namespace

namespace gdata {

// Delegate used to find a directory element for file system updates.
class UpdateDirectoryDelegate : public FindFileDelegate {
 public:
  UpdateDirectoryDelegate() : directory_(NULL) {
  }

  GDataDirectory* directory() { return directory_; }

 private:
  // GDataFileSystem::FindFileDelegate overrides.
  virtual void OnFileFound(gdata::GDataFile*) OVERRIDE {
    directory_ = NULL;
  }

  // GDataFileSystem::FindFileDelegate overrides.
  virtual void OnDirectoryFound(const FilePath&,
                                GDataDirectory* dir) OVERRIDE {
    if (!dir->file_info().is_directory)
      return;

    directory_ = dir;
  }

  virtual FindFileTraversalCommand OnEnterDirectory(const FilePath&,
                                                    GDataDirectory*) OVERRIDE {
    // Keep traversing while doing read only lookups.
    return FIND_FILE_CONTINUES;
  }

  virtual void OnError(base::PlatformFileError) OVERRIDE {
    directory_ = NULL;
  }

  GDataDirectory* directory_;
};

FindFileDelegate::~FindFileDelegate() {
}

// GDataFileBase class.

GDataFileBase::GDataFileBase() {
}

GDataFileBase::~GDataFileBase() {
}

GDataFile* GDataFileBase::AsGDataFile() {
  return NULL;
}

GDataDirectory* GDataFileBase::AsGDataDirectory() {
  return NULL;
}


GDataFileBase* GDataFileBase::FromDocumentEntry(DocumentEntry* doc) {
  if (doc->is_folder())
    return GDataDirectory::FromDocumentEntry(doc);
  else if (doc->is_hosted_document() || doc->is_file())
    return GDataFile::FromDocumentEntry(doc);

  return NULL;
}

GDataFileBase* GDataFile::FromDocumentEntry(DocumentEntry* doc) {
  DCHECK(doc->is_hosted_document() || doc->is_file());
  GDataFile* file = new GDataFile();
  // Check if this entry is a true file, or...
  if (doc->is_file()) {
    file->original_file_name_ = UTF16ToUTF8(doc->filename());
    file->file_name_ =
        EscapeFileName(file->original_file_name_);
    file->file_info_.size = doc->file_size();
    file->file_md5_ = doc->file_md5();
  } else {
    // ... a hosted document.
    file->original_file_name_ = UTF16ToUTF8(doc->title());
    // Attach .g<something> extension to hosted documents so we can special
    // case their handling in UI.
    // TODO(zelidrag): Figure out better way how to pass entry info like kind
    // to UI through the File API stack.
    file->file_name_ = EscapeFileName(
        base::StringPrintf("%s.g%s",
                           file->original_file_name_.c_str(),
                           doc->GetEntryKindText().c_str()));
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
  return file;
}

// GDataFile class implementation.

GDataFile::GDataFile() : kind_(gdata::DocumentEntry::UNKNOWN) {
}

GDataFile::~GDataFile() {
}

GDataFile* GDataFile::AsGDataFile() {
  return this;
}

// GDataDirectory class implementation.

GDataDirectory::GDataDirectory() {
  file_info_.is_directory = true;
}

GDataDirectory::~GDataDirectory() {
  RemoveChildren();
}

GDataDirectory* GDataDirectory::AsGDataDirectory() {
  return this;
}

// static
GDataFileBase* GDataDirectory::FromDocumentEntry(DocumentEntry* doc) {
  DCHECK(doc->is_folder());
  GDataDirectory* dir = new GDataDirectory();
  dir->file_name_ = UTF16ToUTF8(doc->title());
  dir->file_info_.last_modified = doc->updated_time();
  dir->file_info_.last_accessed = doc->updated_time();
  dir->file_info_.creation_time = doc->published_time();
  // Extract feed link.
  dir->start_feed_url_ = doc->content_url();
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

// GDataFileSystem class implementatsion.

GDataFileSystem::GDataFileSystem() {
  root_.reset(new GDataDirectory());
  root_->set_file_name(kGDataRootDirectory);
}

GDataFileSystem::~GDataFileSystem() {
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
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &GDataFileSystem::RefreshFeedOnUIThread,
          this,
          params.feed_url,
          base::Bind(&GDataFileSystem::OnGetDocuments,
                     this,
                     params)));
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

void GDataFileSystem::RefreshFeedOnUIThread(const GURL& feed_url,
    const GetDataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DocumentsService::GetInstance()->GetDocuments(feed_url, callback);
}

void GDataFileSystem::OnGetDocuments(
    const FindFileParams& params,
    GDataErrorCode status,
    base::Value* data) {
  base::PlatformFileError error = GDataToPlatformError(status);

  if (error == base::PLATFORM_FILE_OK &&
      (!data || data->GetType() != Value::TYPE_DICTIONARY)) {
    LOG(WARNING) << "No feed content!";
    error = base::PLATFORM_FILE_ERROR_FAILED;
  }

  if (error != base::PLATFORM_FILE_OK) {
    params.delegate->OnError(error);
    return;
  }

  GURL next_feed_url;
  error = UpdateDirectoryWithDocumentFeed(
     params.directory_path, params.feed_url, data, params.initial_feed,
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
  scoped_refptr<UpdateDirectoryDelegate> update_delegate(
      new UpdateDirectoryDelegate());
  UnsafeFindFileByPath(directory_path,
                       update_delegate);

  GDataDirectory* dir = update_delegate->directory();
  if (!dir)
    return base::PLATFORM_FILE_ERROR_FAILED;

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

    GDataFileBase* file = GDataFileBase::FromDocumentEntry(doc);
    if (file)
      dir->AddFile(file);
  }
  return base::PLATFORM_FILE_OK;
}

}  // namespace gdata
