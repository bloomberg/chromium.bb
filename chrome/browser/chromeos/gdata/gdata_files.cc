// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_files.h"

#include <vector>

#include "base/utf_string_conversions.h"
#include "base/platform_file.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/gdata/gdata_parser.h"

namespace {

// Content refresh time.
const int kRefreshTimeInSec = 5*60;

const char kSlash[] = "/";
const char kEscapedSlash[] = "\xE2\x88\x95";

}  // namespace

namespace gdata {

// GDataFileBase class.

GDataFileBase::GDataFileBase(GDataDirectory* parent)
    : parent_(parent),
      root_(NULL) {
  // Traverse up the tree to find the root which is the directory that has no
  // parent.
  // Only do so if there's parent, because for GDataRootDirectory that has no
  // parent, its overriding AsGDataDirectory() won't be invoked here when
  // constructing its base class; instead GDataFileBase::AsGDataRootDirectory
  // will be the one that's invoked.
  // GDataRootDirectory will initialize root_ itself.
  if (parent) {
    GDataFileBase* current = this;
    while (current->parent()) {
      current = current->parent();
    }
    root_ = current->AsGDataRootDirectory();
    DCHECK(root_);
  }
}

GDataFileBase::~GDataFileBase() {
}

GDataFile* GDataFileBase::AsGDataFile() {
  return NULL;
}

GDataDirectory* GDataFileBase::AsGDataDirectory() {
  return NULL;
}

GDataRootDirectory* GDataFileBase::AsGDataRootDirectory() {
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

void GDataFileBase::SetFileNameFromTitle() {
  file_name_ = EscapeUtf8FileName(title_);
}

// static.
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

// static.
std::string GDataFileBase::EscapeUtf8FileName(const std::string& input) {
  std::string output;
  if (ReplaceChars(input, kSlash, std::string(kEscapedSlash), &output))
    return output;

  return input;
}

// static.
std::string GDataFileBase::UnescapeUtf8FileName(const std::string& input) {
  std::string output = input;
  ReplaceSubstringsAfterOffset(&output, 0, std::string(kEscapedSlash), kSlash);
  return output;
}

// GDataFile class implementation.

GDataFile::GDataFile(GDataDirectory* parent)
    : GDataFileBase(parent),
      kind_(gdata::DocumentEntry::UNKNOWN),
      is_hosted_document_(false) {
  DCHECK(parent);
}

GDataFile::~GDataFile() {
}

GDataFile* GDataFile::AsGDataFile() {
  return this;
}

void GDataFile::SetFileNameFromTitle() {
  if (is_hosted_document_) {
    file_name_ = EscapeUtf8FileName(title_ + document_extension_);
  } else {
    GDataFileBase::SetFileNameFromTitle();
  }
}

GDataFileBase* GDataFile::FromDocumentEntry(GDataDirectory* parent,
                                            DocumentEntry* doc) {
  DCHECK(doc->is_hosted_document() || doc->is_file());
  GDataFile* file = new GDataFile(parent);

  // For regular files, the 'filename' and 'title' attribute in the metadata
  // may be different (e.g. due to rename). To be consistent with the web
  // interface and other client to use the 'title' attribute, instead of
  // 'filename', as the file name in the local snapshot.
  file->title_ = UTF16ToUTF8(doc->title());

  // Check if this entry is a true file, or...
  if (doc->is_file()) {
    file->file_info_.size = doc->file_size();
    file->file_md5_ = doc->file_md5();
  } else {
    // ... a hosted document.
    // Attach .g<something> extension to hosted documents so we can special
    // case their handling in UI.
    // TODO(zelidrag): Figure out better way how to pass entry info like kind
    // to UI through the File API stack.
    file->document_extension_ = doc->GetHostedDocumentExtension();
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
  file->is_hosted_document_ = doc->is_hosted_document();
  file->file_info_.last_modified = doc->updated_time();
  file->file_info_.last_accessed = doc->updated_time();
  file->file_info_.creation_time = doc->published_time();

  // SetFileNameFromTitle() must be called after |title_|,
  // |is_hosted_document_| and |document_extension_| are set.
  file->SetFileNameFromTitle();

  const Link* thumbnail_link = doc->GetLinkByType(Link::THUMBNAIL);
  if (thumbnail_link)
    file->thumbnail_url_ = thumbnail_link->href();

  const Link* alternate_link = doc->GetLinkByType(Link::ALTERNATE);
  if (alternate_link)
    file->edit_url_ = alternate_link->href();

  return file;
}

int GDataFile::GetCacheState() {
  return root_->GetCacheState(resource_id(), file_md5());
}

// GDataDirectory class implementation.

GDataDirectory::GDataDirectory(GDataDirectory* parent)
    : GDataFileBase(parent) {
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
  dir->title_ = UTF16ToUTF8(doc->title());
  // SetFileNameFromTitle() must be called after |title_| is set.
  dir->SetFileNameFromTitle();
  dir->file_info_.last_modified = doc->updated_time();
  dir->file_info_.last_accessed = doc->updated_time();
  dir->file_info_.creation_time = doc->published_time();
  // Extract feed link.
  dir->start_feed_url_ = doc->content_url();
  dir->resource_id_ = doc->resource_id();
  dir->content_url_ = doc->content_url();

  const Link* self_link = doc->GetLinkByType(Link::SELF);
  if (self_link)
    dir->self_url_ = self_link->href();

  const Link* upload_link = doc->GetLinkByType(Link::RESUMABLE_CREATE_MEDIA);
  if (upload_link)
    dir->upload_url_ = upload_link->href();

  return dir;
}

void GDataDirectory::RemoveChildren() {
  // Remove children from resource map first.
  root_->RemoveFilesFromResourceMap(children_);

  // Then delete and remove the children from tree.
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

  // Add file to resource map.
  root_->AddFileToResourceMap(file);
}

bool GDataDirectory::TakeFile(GDataFileBase* file) {
  DCHECK(file);
  DCHECK(file->parent());

  file->parent()->RemoveFileFromChildrenList(file);

  // The file name may have been changed due to prior name de-duplication.
  // We need to first restore the file name based on the title before going
  // through name de-duplication again when it is added to another directory.
  file->SetFileNameFromTitle();
  AddFile(file);

  // Use GDataFileBase::set_parent() to change the parent of GDataFileBase
  // as GDataDirectory:AddFile() does not do that.
  file->set_parent(this);
  return true;
}

bool GDataDirectory::RemoveFile(GDataFileBase* file) {
  DCHECK(file);

  if (!RemoveFileFromChildrenList(file))
    return false;

  delete file;
  return true;
}

bool GDataDirectory::RemoveFileFromChildrenList(GDataFileBase* file) {
  DCHECK(file);

  GDataFileCollection::iterator iter = children_.find(file->file_name());
  if (iter == children_.end())
    return false;

  DCHECK(iter->second);
  DCHECK_EQ(file, iter->second);

  // Remove file from resource map first.
  root_->RemoveFileFromResourceMap(file);

  // Then delete it from tree.
  children_.erase(iter);

  return true;
}

// GDataRootDirectory class implementation.

GDataRootDirectory::GDataRootDirectory()
    : GDataDirectory(NULL) {
  root_ = this;
}

GDataRootDirectory::~GDataRootDirectory() {
  STLDeleteValues(&cache_map_);
  cache_map_.clear();

  resource_map_.clear();
}

GDataRootDirectory* GDataRootDirectory::AsGDataRootDirectory() {
  return this;
}

void GDataRootDirectory::AddFileToResourceMap(GDataFileBase* file) {
  // GDataFileSystem has already locked.
  // Only files have resource.
  if (file->AsGDataFile()) {
    resource_map_.insert(
        std::make_pair(file->AsGDataFile()->resource_id(), file));
  }
}

void GDataRootDirectory::RemoveFileFromResourceMap(GDataFileBase* file) {
  // GDataFileSystem has already locked.
  if (file->AsGDataFile())
    resource_map_.erase(file->AsGDataFile()->resource_id());
}

void GDataRootDirectory::RemoveFilesFromResourceMap(
    const GDataFileCollection& children) {
  // GDataFileSystem has already locked.
  for (GDataFileCollection::const_iterator iter = children.begin();
       iter != children.end(); ++iter) {
    // Recursively call RemoveFilesFromResourceMap for each directory.
    if (iter->second->AsGDataDirectory()) {
      RemoveFilesFromResourceMap(iter->second->AsGDataDirectory()->children());
      continue;
    }

    // Only files have resource.
    if (iter->second->AsGDataFile())
      resource_map_.erase(iter->second->AsGDataFile()->resource_id());
  }
}

GDataFileBase* GDataRootDirectory::GetFileByResource(
    const std::string& resource) {
  // GDataFileSystem has already locked.
  ResourceMap::const_iterator iter = resource_map_.find(resource);
  if (iter == resource_map_.end())
    return NULL;
  return iter->second;
}

void GDataRootDirectory::SetCacheMap(const CacheMap& new_cache_map)  {
  // GDataFileSystem has already locked.

  // Delete everything in cache map before copying.
  STLDeleteValues(&cache_map_);
  cache_map_ = new_cache_map;
}

void GDataRootDirectory::UpdateCacheMap(const std::string& res_id,
                                        const std::string& md5,
                                        mode_t mode_bits) {
  // GDataFileSystem has already locked.

  CacheEntry* entry = NULL;

  CacheMap::iterator iter = cache_map_.find(res_id);
  if (iter == cache_map_.end()) {  // New resource, create new entry.
    entry = new CacheEntry(md5, mode_bits);
    cache_map_.insert(std::make_pair(res_id, entry));
    DVLOG(1) << "Added res=" << res_id
             << ", md5=" << md5
             << ", mode=" << mode_bits;
  } else {  // Resource already exists, update its entry info.
    entry = iter->second;
    entry->md5 = md5;
    entry->mode_bits = mode_bits;
    DVLOG(1) << "Updated res=" << res_id
             << ", md5=" << md5
             << ", mode=" << mode_bits;
  }
}

void GDataRootDirectory::RemoveFromCacheMap(const std::string& res_id) {
  // GDataFileSystem has already locked.

  CacheMap::iterator iter = cache_map_.find(res_id);
  if (iter != cache_map_.end()) {
    // Delete the CacheEntry and remove it from the map.
    delete iter->second;
    cache_map_.erase(iter);
  }
}

bool GDataRootDirectory::CacheFileExists(const std::string& res_id,
                                         const std::string& md5) {
  // GDataFileSystem has already locked.
  CacheMap::const_iterator iter = cache_map_.find(res_id);
  // It's only a valid file if entry exists in cache map and its CACHE_OK bit
  // is set i.e. not corrupted.
  return iter != cache_map_.end() &&
      iter->second->mode_bits & CACHE_OK;
}

int GDataRootDirectory::GetCacheState(const std::string& res_id,
                                      const std::string& md5) {
  // GDataFileSystem has already locked in FindFileDelegate::OnFileFound.

  int cache_state = GDataFile::CACHE_STATE_NONE;

  CacheMap::const_iterator iter = cache_map_.find(res_id);
  if (iter == cache_map_.end()) {
    DVLOG(1) << "Can't find " << res_id << " in cache map";
    return cache_state;
  }

  // Entry is only valid if md5 matches.
  if (iter->second->md5 != md5) {
    DVLOG(1) << "Non-matching md5 for res_id " << res_id
             << " in cache resource";
    return cache_state;
  }

  // Convert file's mode bits to GDataFile::CacheState.
  CacheEntry* entry = iter->second;
  if (entry->mode_bits & CACHE_OK)
    cache_state |= GDataFile::CACHE_STATE_PRESENT;
  if (entry->mode_bits & CACHE_DIRTY)
    cache_state |= GDataFile::CACHE_STATE_DIRTY;
  if (entry->mode_bits & CACHE_PINNED)
    cache_state |= GDataFile::CACHE_STATE_PINNED;

  DVLOG(1) << "Cache state for res_id " << res_id
           << ", md5 " << entry->md5
            << ": " << cache_state;

  return cache_state;
}

}  // namespace gdata
