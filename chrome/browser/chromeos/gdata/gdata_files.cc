// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_files.h"

#include <vector>

#include "base/utf_string_conversions.h"
#include "base/platform_file.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/gdata/gdata.pb.h"
#include "chrome/browser/chromeos/gdata/gdata_parser.h"
#include "net/base/escape.h"

namespace {

// Content refresh time.
#ifndef NDEBUG
const int kRefreshTimeInSec = 10;
#else
const int kRefreshTimeInSec = 5*60;
#endif

const char kSlash[] = "/";
const char kEscapedSlash[] = "\xE2\x88\x95";

std::string CacheSubDirectoryTypeToString(
    gdata::GDataRootDirectory::CacheSubDirectoryType subdir) {
  switch (subdir) {
    case gdata::GDataRootDirectory::CACHE_TYPE_META: return "meta";
    case gdata::GDataRootDirectory::CACHE_TYPE_PINNED: return "pinned";
    case gdata::GDataRootDirectory::CACHE_TYPE_OUTGOING: return "outgoing";
    case gdata::GDataRootDirectory::CACHE_TYPE_PERSISTENT: return "persistent";
    case gdata::GDataRootDirectory::CACHE_TYPE_TMP: return "tmp";
    case gdata::GDataRootDirectory::CACHE_TYPE_TMP_DOWNLOADS:
      return "tmp_downloads";
    case gdata::GDataRootDirectory::CACHE_TYPE_TMP_DOCUMENTS:
      return "tmp_documents";
  }
  NOTREACHED();
  return "unknown subdir";
}

// Extracts resource_id out of edit url.
std::string ExtractResourceId(const GURL& url) {
  return net::UnescapeURLComponent(url.ExtractFileName(),
                                   net::UnescapeRule::URL_SPECIAL_CHARS);
}

}  // namespace

namespace gdata {

// GDataFileBase class.

GDataFileBase::GDataFileBase(GDataDirectory* parent, GDataRootDirectory* root)
    : parent_(parent),
      root_(root),
      deleted_(false) {
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
                                                DocumentEntry* doc,
                                                GDataRootDirectory* root) {
  DCHECK(doc);
  if (doc->is_folder())
    return GDataDirectory::FromDocumentEntry(parent, doc, root);
  else if (doc->is_hosted_document() || doc->is_file())
    return GDataFile::FromDocumentEntry(parent, doc, root);

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

GDataFile::GDataFile(GDataDirectory* parent, GDataRootDirectory* root)
    : GDataFileBase(parent, root),
      kind_(gdata::DocumentEntry::UNKNOWN),
      is_hosted_document_(false) {
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

// static.
GDataFileBase* GDataFile::FromDocumentEntry(GDataDirectory* parent,
                                            DocumentEntry* doc,
                                            GDataRootDirectory* root) {
  DCHECK(doc->is_hosted_document() || doc->is_file());
  GDataFile* file = new GDataFile(parent, root);

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
  const Link* edit_link = doc->GetLinkByType(Link::EDIT);
  DCHECK(edit_link) << "No edit link for file " << file->title_;
  if (edit_link)
    file->edit_url_ = edit_link->href();
  file->content_url_ = doc->content_url();
  file->content_mime_type_ = doc->content_mime_type();
  file->etag_ = doc->etag();
  file->resource_id_ = doc->resource_id();
  file->id_ = doc->id();
  file->is_hosted_document_ = doc->is_hosted_document();
  file->file_info_.last_modified = doc->updated_time();
  file->file_info_.last_accessed = doc->updated_time();
  file->file_info_.creation_time = doc->published_time();
  file->deleted_ = doc->deleted();
  const Link* parent_link = doc->GetLinkByType(Link::PARENT);
  if (parent_link)
    file->parent_resource_id_ = ExtractResourceId(parent_link->href());

  // SetFileNameFromTitle() must be called after |title_|,
  // |is_hosted_document_| and |document_extension_| are set.
  file->SetFileNameFromTitle();

  const Link* thumbnail_link = doc->GetLinkByType(Link::THUMBNAIL);
  if (thumbnail_link)
    file->thumbnail_url_ = thumbnail_link->href();

  const Link* alternate_link = doc->GetLinkByType(Link::ALTERNATE);
  if (alternate_link)
    file->alternate_url_ = alternate_link->href();

  return file;
}

// GDataDirectory class implementation.

GDataDirectory::GDataDirectory(GDataDirectory* parent, GDataRootDirectory* root)
    : GDataFileBase(parent, root), origin_(UNINITIALIZED) {
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
                                                 DocumentEntry* doc,
                                                 GDataRootDirectory* root) {
  DCHECK(doc->is_folder());
  GDataDirectory* dir = new GDataDirectory(parent, root);
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
  dir->deleted_ = doc->deleted();

  const Link* edit_link = doc->GetLinkByType(Link::EDIT);
  DCHECK(edit_link) << "No edit link for dir " << dir->title_;
  if (edit_link)
    dir->edit_url_ = edit_link->href();

  const Link* parent_link = doc->GetLinkByType(Link::PARENT);
  if (parent_link)
    dir->parent_resource_id_ = ExtractResourceId(parent_link->href());

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

bool GDataDirectory::NeedsRefresh() const {
  // Already refreshing by someone else.
  if (origin_ == REFRESHING)
    return false;

  // Refresh is needed for content read from disk cache or stale content.
  if (origin_ == FROM_CACHE)
    return true;

  if ((base::Time::Now() - refresh_time_).InSeconds() < kRefreshTimeInSec)
    return false;

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
  DVLOG(1) << "Adding: "
           << this->GetFilePath().value()
           << "/" + file->file_name()
           << ", resource " << file->parent_resource_id()
           << "/" + file->resource_id();

  // Add file to resource map.
  root_->AddFileToResourceMap(file);

  file->set_parent(this);
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

  return true;
}

bool GDataDirectory::TakeOverFiles(GDataDirectory* dir) {
  for (GDataFileCollection::iterator iter = dir->children_.begin();
       iter != dir->children_.end(); ++iter) {
    GDataFileBase* file = iter->second;
    file->SetFileNameFromTitle();
    AddFile(file);
  }
  dir->children_.clear();
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

// GDataRootDirectory::CacheEntry struct  implementation.

std::string GDataRootDirectory::CacheEntry::ToString() const {
  std::vector<std::string> cache_states;
  if (GDataFile::IsCachePresent(cache_state))
    cache_states.push_back("present");
  if (GDataFile::IsCachePinned(cache_state))
    cache_states.push_back("pinned");
  if (GDataFile::IsCacheDirty(cache_state))
    cache_states.push_back("dirty");

  return base::StringPrintf("md5=%s, subdir=%s, cache_state=%s",
                            md5.c_str(),
                            CacheSubDirectoryTypeToString(sub_dir_type).c_str(),
                            JoinString(cache_states, ',').c_str());
}

// GDataRootDirectory class implementation.

GDataRootDirectory::GDataRootDirectory()
    : ALLOW_THIS_IN_INITIALIZER_LIST(GDataDirectory(NULL, this)),
      largest_changestamp_(0), serialized_size_(0) {
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
  resource_map_.insert(std::make_pair(file->resource_id(), file));
}

void GDataRootDirectory::RemoveFileFromResourceMap(GDataFileBase* file) {
  // GDataFileSystem has already locked.
  resource_map_.erase(file->resource_id());
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

    resource_map_.erase(iter->second->resource_id());
  }
}

GDataFileBase* GDataRootDirectory::GetFileByResourceId(
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

void GDataRootDirectory::UpdateCacheMap(const std::string& resource_id,
                                        const std::string& md5,
                                        CacheSubDirectoryType subdir,
                                        int cache_state) {
  // GDataFileSystem has already locked.

  CacheMap::iterator iter = cache_map_.find(resource_id);
  if (iter == cache_map_.end()) {  // New resource, create new entry.
    // Makes no sense to create new entry if cache state is NONE.
    DCHECK(cache_state != GDataFile::CACHE_STATE_NONE);
    if (cache_state != GDataFile::CACHE_STATE_NONE) {
      CacheEntry* entry = new CacheEntry(md5, subdir, cache_state);
      cache_map_.insert(std::make_pair(resource_id, entry));
      DVLOG(1) << "Added res_id=" << resource_id
               << ", " << entry->ToString();
    }
  } else {  // Resource exists.
    CacheEntry* entry = iter->second;
    // If cache state is NONE, delete entry from cache map.
    if (cache_state == GDataFile::CACHE_STATE_NONE) {
      DVLOG(1) << "Deleting res_id=" << resource_id
               << ", " << entry->ToString();
      delete entry;
      cache_map_.erase(iter);
    } else {  // Otherwise, update entry in cache map.
      entry->md5 = md5;
      entry->sub_dir_type = subdir;
      entry->cache_state = cache_state;
      DVLOG(1) << "Updated res_id=" << resource_id
               << ", " << entry->ToString();
    }
  }
}

void GDataRootDirectory::RemoveFromCacheMap(const std::string& resource_id) {
  // GDataFileSystem has already locked.

  CacheMap::iterator iter = cache_map_.find(resource_id);
  if (iter != cache_map_.end()) {
    // Delete the CacheEntry and remove it from the map.
    delete iter->second;
    cache_map_.erase(iter);
  }
}

GDataRootDirectory::CacheEntry* GDataRootDirectory::GetCacheEntry(
    const std::string& resource_id,
    const std::string& md5) {
  // GDataFileSystem has already locked.

  CacheMap::iterator iter = cache_map_.find(resource_id);
  if (iter == cache_map_.end()) {
    DVLOG(1) << "Can't find " << resource_id << " in cache map";
    return NULL;
  }

  CacheEntry* entry = iter->second;

  // If entry is not dirty, it's only valid if matches with non-empty |md5|.
  // If entry is dirty, its md5 may have been replaced by "local" during cache
  // initialization, so we don't compare md5.
  if (!entry->IsDirty() && !md5.empty() && entry->md5 != md5) {
    DVLOG(1) << "Non-matching md5: want=" << md5
             << ", found=[res_id=" << resource_id
             << ", " << entry->ToString()
             << "]";
    return NULL;
  }

  DVLOG(1) << "Found entry for res_id=" << resource_id
           << ", " << entry->ToString();

  return entry;
}

void GDataRootDirectory::RemoveTemporaryFilesFromCacheMap() {
  CacheMap::iterator iter = cache_map_.begin();
  while (iter != cache_map_.end()) {
    CacheEntry* entry = iter->second;
    if (entry->sub_dir_type == CACHE_TYPE_TMP) {
      delete entry;
      // Post-increment the iterator to avoid iterator invalidation.
      cache_map_.erase(iter++);
    } else {
      ++iter;
    }
  }
}

// Convert to/from proto.

void GDataFileBase::FromProto(const GDataFileBaseProto& proto) {
  file_info_.size = proto.file_info().size();
  file_info_.is_directory = proto.file_info().is_directory();
  file_info_.is_symbolic_link = proto.file_info().is_symbolic_link();
  file_info_.last_modified = base::Time::FromInternalValue(
      proto.file_info().last_modified());
  file_info_.last_accessed = base::Time::FromInternalValue(
      proto.file_info().last_accessed());
  file_info_.creation_time = base::Time::FromInternalValue(
      proto.file_info().creation_time());

  file_name_ = proto.file_name();
  title_ = proto.title();
  resource_id_ = proto.resource_id();
  edit_url_ = GURL(proto.edit_url());
  content_url_ = GURL(proto.content_url());
}

void GDataFileBase::ToProto(GDataFileBaseProto* proto) const {
  PlatformFileInfoProto* proto_file_info = proto->mutable_file_info();
  proto_file_info->set_size(file_info_.size);
  proto_file_info->set_is_directory(file_info_.is_directory);
  proto_file_info->set_is_symbolic_link(file_info_.is_symbolic_link);
  proto_file_info->set_last_modified(
      file_info_.last_modified.ToInternalValue());
  proto_file_info->set_last_accessed(
      file_info_.last_accessed.ToInternalValue());
  proto_file_info->set_creation_time(
      file_info_.creation_time.ToInternalValue());

  proto->set_file_name(file_name_);
  proto->set_title(title_);
  proto->set_resource_id(resource_id_);
  proto->set_edit_url(edit_url_.spec());
  proto->set_content_url(content_url_.spec());
}

void GDataFile::FromProto(const GDataFileProto& proto) {
  GDataFileBase::FromProto(proto.gdata_file_base());
  kind_ = DocumentEntry::EntryKind(proto.kind());
  thumbnail_url_ = GURL(proto.thumbnail_url());
  alternate_url_ = GURL(proto.alternate_url());
  content_mime_type_ = proto.content_mime_type();
  etag_ = proto.etag();
  id_ = proto.id();
  file_md5_ = proto.file_md5();
  document_extension_ = proto.document_extension();
  is_hosted_document_ = proto.is_hosted_document();
}

void GDataFile::ToProto(GDataFileProto* proto) const {
  GDataFileBase::ToProto(proto->mutable_gdata_file_base());
  proto->set_kind(kind_);
  proto->set_thumbnail_url(thumbnail_url_.spec());
  proto->set_alternate_url(alternate_url_.spec());
  proto->set_content_mime_type(content_mime_type_);
  proto->set_etag(etag_);
  proto->set_id(id_);
  proto->set_file_md5(file_md5_);
  proto->set_document_extension(document_extension_);
  proto->set_is_hosted_document(is_hosted_document_);
}

void GDataDirectory::FromProto(const GDataDirectoryProto& proto) {
  GDataFileBase::FromProto(proto.gdata_file_base());
  refresh_time_ = base::Time::FromInternalValue(proto.refresh_time());
  start_feed_url_ = GURL(proto.start_feed_url());
  next_feed_url_ = GURL(proto.next_feed_url());
  upload_url_ = GURL(proto.upload_url());
  origin_ = ContentOrigin(proto.origin());
  for (int i = 0; i < proto.child_files_size(); ++i) {
    scoped_ptr<GDataFile> file(new GDataFile(this, root_));
    file->FromProto(proto.child_files(i));
    AddFile(file.release());
  }
  for (int i = 0; i < proto.child_directories_size(); ++i) {
    scoped_ptr<GDataDirectory> dir(new GDataDirectory(this, root_));
    dir->FromProto(proto.child_directories(i));
    AddFile(dir.release());
  }
}

void GDataDirectory::ToProto(GDataDirectoryProto* proto) const {
  GDataFileBase::ToProto(proto->mutable_gdata_file_base());
  proto->set_refresh_time(refresh_time_.ToInternalValue());
  proto->set_start_feed_url(start_feed_url_.spec());
  proto->set_next_feed_url(next_feed_url_.spec());
  proto->set_upload_url(upload_url_.spec());
  proto->set_origin(origin_);
  for (GDataFileCollection::const_iterator iter = children_.begin();
       iter != children_.end(); ++iter) {
    GDataFile* file = iter->second->AsGDataFile();
    GDataDirectory* dir = iter->second->AsGDataDirectory();
    if (file)
      file->ToProto(proto->add_child_files());
    if (dir)
      dir->ToProto(proto->add_child_directories());
  }
}

void GDataRootDirectory::FromProto(const GDataRootDirectoryProto& proto) {
  root_ = this;
  GDataDirectory::FromProto(proto.gdata_directory());
  largest_changestamp_ = proto.largest_changestamp();
}

void GDataRootDirectory::ToProto(GDataRootDirectoryProto* proto) const {
  GDataDirectory::ToProto(proto->mutable_gdata_directory());
  proto->set_largest_changestamp(largest_changestamp_);
}

void GDataRootDirectory::SerializeToString(std::string* proto_string) const {
  scoped_ptr<GDataRootDirectoryProto> proto(
      new GDataRootDirectoryProto());
  ToProto(proto.get());
  const bool ok = proto->SerializeToString(proto_string);
  DCHECK(ok);
}

bool GDataRootDirectory::ParseFromString(const std::string& proto_string) {
  scoped_ptr<GDataRootDirectoryProto> proto(
      new GDataRootDirectoryProto());
  bool ok = proto->ParseFromString(proto_string);
  if (ok) {
    FromProto(*proto.get());
    set_origin(FROM_CACHE);
    set_refresh_time(base::Time::Now());
  }
  return ok;
}

}  // namespace gdata
