// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_files.h"

#include <vector>

#include "base/utf_string_conversions.h"
#include "base/platform_file.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/gdata/find_entry_delegate.h"
#include "chrome/browser/chromeos/gdata/gdata.pb.h"
#include "chrome/browser/chromeos/gdata/gdata_parser.h"
#include "net/base/escape.h"

namespace gdata {
namespace {

const char kSlash[] = "/";
const char kEscapedSlash[] = "\xE2\x88\x95";
const FilePath::CharType kGDataRootDirectory[] = FILE_PATH_LITERAL("gdata");

std::string CacheSubDirectoryTypeToString(
    GDataRootDirectory::CacheSubDirectoryType subdir) {
  switch (subdir) {
    case GDataRootDirectory::CACHE_TYPE_META: return "meta";
    case GDataRootDirectory::CACHE_TYPE_PINNED: return "pinned";
    case GDataRootDirectory::CACHE_TYPE_OUTGOING: return "outgoing";
    case GDataRootDirectory::CACHE_TYPE_PERSISTENT: return "persistent";
    case GDataRootDirectory::CACHE_TYPE_TMP: return "tmp";
    case GDataRootDirectory::CACHE_TYPE_TMP_DOWNLOADS:
      return "tmp_downloads";
    case GDataRootDirectory::CACHE_TYPE_TMP_DOCUMENTS:
      return "tmp_documents";
    case GDataRootDirectory::NUM_CACHE_TYPES:
      NOTREACHED();
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

// GDataEntry class.

GDataEntry::GDataEntry(GDataDirectory* parent, GDataRootDirectory* root)
    : root_(root),
      deleted_(false) {
  SetParent(parent);
}

GDataEntry::~GDataEntry() {
}

GDataFile* GDataEntry::AsGDataFile() {
  return NULL;
}

GDataDirectory* GDataEntry::AsGDataDirectory() {
  return NULL;
}

GDataRootDirectory* GDataEntry::AsGDataRootDirectory() {
  return NULL;
}

const GDataFile* GDataEntry::AsGDataFileConst() const {
  // cast away const and call the non-const version. This is safe.
  return const_cast<GDataEntry*>(this)->AsGDataFile();
}

const GDataDirectory* GDataEntry::AsGDataDirectoryConst() const {
  // cast away const and call the non-const version. This is safe.
  return const_cast<GDataEntry*>(this)->AsGDataDirectory();
}

FilePath GDataEntry::GetFilePath() const {
  FilePath path;
  if (parent())
    path = parent()->GetFilePath();
  path = path.Append(file_name());
  return path;
}

void GDataEntry::SetParent(GDataDirectory* parent) {
  parent_ = parent;
  parent_resource_id_ = parent ? parent->resource_id() : "";
}

void GDataEntry::SetFileNameFromTitle() {
  file_name_ = EscapeUtf8FileName(title_);
}

// static.
GDataEntry* GDataEntry::FromDocumentEntry(GDataDirectory* parent,
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
std::string GDataEntry::EscapeUtf8FileName(const std::string& input) {
  std::string output;
  if (ReplaceChars(input, kSlash, std::string(kEscapedSlash), &output))
    return output;

  return input;
}

// static.
std::string GDataEntry::UnescapeUtf8FileName(const std::string& input) {
  std::string output = input;
  ReplaceSubstringsAfterOffset(&output, 0, std::string(kEscapedSlash), kSlash);
  return output;
}

// GDataFile class implementation.

GDataFile::GDataFile(GDataDirectory* parent, GDataRootDirectory* root)
    : GDataEntry(parent, root),
      kind_(DocumentEntry::UNKNOWN),
      is_hosted_document_(false) {
  file_info_.is_directory = false;
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
    GDataEntry::SetFileNameFromTitle();
  }
}

// static.
GDataEntry* GDataFile::FromDocumentEntry(GDataDirectory* parent,
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
    : GDataEntry(parent, root), origin_(UNINITIALIZED) {
  file_info_.is_directory = true;
}

GDataDirectory::~GDataDirectory() {
  RemoveChildren();
}

GDataDirectory* GDataDirectory::AsGDataDirectory() {
  return this;
}

// static
GDataEntry* GDataDirectory::FromDocumentEntry(GDataDirectory* parent,
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

void GDataDirectory::AddEntry(GDataEntry* entry) {
  // The entry name may have been changed due to prior name de-duplication.
  // We need to first restore the file name based on the title before going
  // through name de-duplication again when it is added to another directory.
  entry->SetFileNameFromTitle();

  // Do file name de-duplication - find files with the same name and
  // append a name modifier to the name.
  int max_modifier = 1;
  FilePath full_file_name(entry->file_name());
  const std::string extension = full_file_name.Extension();
  const std::string file_name = full_file_name.RemoveExtension().value();
  while (FindChild(full_file_name.value())) {
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
  entry->set_file_name(full_file_name.value());

  DVLOG(1) << "AddEntry: dir = " << GetFilePath().value()
           << ", file = " + entry->file_name()
           << ", parent resource = " << entry->parent_resource_id()
           << ", resource = " + entry->resource_id();


  // Add entry to resource map.
  root_->AddEntryToResourceMap(entry);
  // Setup child and parent links.
  AddChild(entry);
  entry->SetParent(this);
}

bool GDataDirectory::TakeEntry(GDataEntry* entry) {
  DCHECK(entry);
  DCHECK(entry->parent());

  entry->parent()->RemoveChild(entry);
  AddEntry(entry);

  return true;
}

bool GDataDirectory::TakeOverEntries(GDataDirectory* dir) {
  for (GDataFileCollection::iterator iter = dir->child_files_.begin();
       iter != dir->child_files_.end(); ++iter) {
    AddEntry(iter->second);
  }
  dir->child_files_.clear();

  for (GDataDirectoryCollection::iterator iter =
      dir->child_directories_.begin();
       iter != dir->child_directories_.end(); ++iter) {
    AddEntry(iter->second);
  }
  dir->child_directories_.clear();
  return true;
}

bool GDataDirectory::RemoveEntry(GDataEntry* entry) {
  DCHECK(entry);

  if (!RemoveChild(entry))
    return false;

  delete entry;
  return true;
}

GDataEntry* GDataDirectory::FindChild(
    const FilePath::StringType& file_name) const {
  GDataFileCollection::const_iterator it = child_files_.find(file_name);
  if (it != child_files_.end())
    return it->second;

  GDataDirectoryCollection::const_iterator itd =
      child_directories_.find(file_name);
  if (itd != child_directories_.end())
    return itd->second;

  return NULL;
}

void GDataDirectory::AddChild(GDataEntry* entry) {
  DCHECK(entry);

  GDataFile* file = entry->AsGDataFile();
  if (file)
    child_files_.insert(std::make_pair(entry->file_name(), file));

  GDataDirectory* directory = entry->AsGDataDirectory();
  if (directory)
    child_directories_.insert(std::make_pair(entry->file_name(), directory));
}

bool GDataDirectory::RemoveChild(GDataEntry* entry) {
  DCHECK(entry);

  const std::string file_name(entry->file_name());
  GDataEntry* found_entry = FindChild(file_name);
  if (!found_entry)
    return false;

  DCHECK_EQ(entry, found_entry);

  // Remove entry from resource map first.
  root_->RemoveEntryFromResourceMap(entry);

  // Then delete it from tree.
  child_files_.erase(file_name);
  child_directories_.erase(file_name);

  return true;
}

void GDataDirectory::RemoveChildren() {
  // Remove child files first.
  for (GDataFileCollection::const_iterator iter = child_files_.begin();
       iter != child_files_.end(); ++iter) {
    root_->RemoveEntryFromResourceMap(iter->second);
  }
  STLDeleteValues(&child_files_);
  child_files_.clear();

  for (GDataDirectoryCollection::iterator iter = child_directories_.begin();
       iter != child_directories_.end(); ++iter) {
    GDataDirectory* dir = iter->second;
    // Remove directories recursively.
    dir->RemoveChildren();
    root_->RemoveEntryFromResourceMap(dir);
  }
  STLDeleteValues(&child_directories_);
  child_directories_.clear();
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
  title_ = kGDataRootDirectory;
  SetFileNameFromTitle();
}

GDataRootDirectory::~GDataRootDirectory() {
  STLDeleteValues(&cache_map_);
  cache_map_.clear();

  resource_map_.clear();
}

GDataRootDirectory* GDataRootDirectory::AsGDataRootDirectory() {
  return this;
}

void GDataRootDirectory::AddEntryToResourceMap(GDataEntry* entry) {
  // GDataFileSystem has already locked.
  DVLOG(1) << "AddEntryToResourceMap " << entry->resource_id();
  resource_map_.insert(std::make_pair(entry->resource_id(), entry));
}

void GDataRootDirectory::RemoveEntryFromResourceMap(GDataEntry* entry) {
  // GDataFileSystem has already locked.
  resource_map_.erase(entry->resource_id());
}

void GDataRootDirectory::FindEntryByPath(
    const FilePath& file_path,
    FindEntryDelegate* delegate) {
  // GDataFileSystem has already locked.
  DCHECK(delegate);

  std::vector<FilePath::StringType> components;
  file_path.GetComponents(&components);

  GDataDirectory* current_dir = this;
  FilePath directory_path;
  for (size_t i = 0; i < components.size() && current_dir; i++) {
    directory_path = directory_path.Append(current_dir->file_name());

    // Last element must match, if not last then it must be a directory.
    if (i == components.size() - 1) {
      if (current_dir->file_name() == components[i])
        delegate->OnDone(base::PLATFORM_FILE_OK, directory_path, current_dir);
      else
        delegate->OnDone(base::PLATFORM_FILE_ERROR_NOT_FOUND, FilePath(), NULL);

      return;
    }

    // Not the last part of the path, search for the next segment.
    GDataEntry* entry = current_dir->FindChild(components[i + 1]);
    if (!entry) {
      delegate->OnDone(base::PLATFORM_FILE_ERROR_NOT_FOUND, FilePath(), NULL);
      return;
    }

    // Found file, must be the last segment.
    if (entry->file_info().is_directory) {
      // Found directory, continue traversal.
      current_dir = entry->AsGDataDirectory();
    } else {
      if ((i + 1) == (components.size() - 1)) {
        delegate->OnDone(base::PLATFORM_FILE_OK,
                         directory_path,
                         entry);
      } else {
        delegate->OnDone(base::PLATFORM_FILE_ERROR_NOT_FOUND, FilePath(), NULL);
      }

      return;
    }
  }
  delegate->OnDone(base::PLATFORM_FILE_ERROR_NOT_FOUND, FilePath(), NULL);
}

GDataEntry* GDataRootDirectory::GetEntryByResourceId(
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

// static
void GDataEntry::ConvertProtoToPlatformFileInfo(
    const PlatformFileInfoProto& proto,
    base::PlatformFileInfo* file_info) {
  file_info->size = proto.size();
  file_info->is_directory = proto.is_directory();
  file_info->is_symbolic_link = proto.is_symbolic_link();
  file_info->last_modified = base::Time::FromInternalValue(
      proto.last_modified());
  file_info->last_accessed = base::Time::FromInternalValue(
      proto.last_accessed());
  file_info->creation_time = base::Time::FromInternalValue(
      proto.creation_time());
}

// static
void GDataEntry::ConvertPlatformFileInfoToProto(
    const base::PlatformFileInfo& file_info,
    PlatformFileInfoProto* proto) {
  proto->set_size(file_info.size);
  proto->set_is_directory(file_info.is_directory);
  proto->set_is_symbolic_link(file_info.is_symbolic_link);
  proto->set_last_modified(file_info.last_modified.ToInternalValue());
  proto->set_last_accessed(file_info.last_accessed.ToInternalValue());
  proto->set_creation_time(file_info.creation_time.ToInternalValue());
}

void GDataEntry::FromProto(const GDataEntryProto& proto) {
  ConvertProtoToPlatformFileInfo(proto.file_info(), &file_info_);

  // Don't copy from proto.file_name() as file_name_ is computed in
  // SetFileNameFromTitle().
  title_ = proto.title();
  resource_id_ = proto.resource_id();
  parent_resource_id_ = proto.parent_resource_id();
  edit_url_ = GURL(proto.edit_url());
  content_url_ = GURL(proto.content_url());
  SetFileNameFromTitle();
}

void GDataEntry::ToProto(GDataEntryProto* proto) const {
  ConvertPlatformFileInfoToProto(file_info_, proto->mutable_file_info());

  // The file_name field is used in GetFileInfoByPathAsync(). As shown in
  // FromProto(), the value is discarded when deserializing from proto.
  proto->set_file_name(file_name_);
  proto->set_title(title_);
  proto->set_resource_id(resource_id_);
  proto->set_parent_resource_id(parent_resource_id_);
  proto->set_edit_url(edit_url_.spec());
  proto->set_content_url(content_url_.spec());
}

void GDataFile::FromProto(const GDataFileProto& proto) {
  DCHECK(!proto.gdata_entry().file_info().is_directory());
  GDataEntry::FromProto(proto.gdata_entry());
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
  GDataEntry::ToProto(proto->mutable_gdata_entry());
  DCHECK(!proto->gdata_entry().file_info().is_directory());
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
  DCHECK(proto.gdata_entry().file_info().is_directory());
  GDataEntry::FromProto(proto.gdata_entry());
  refresh_time_ = base::Time::FromInternalValue(proto.refresh_time());
  start_feed_url_ = GURL(proto.start_feed_url());
  next_feed_url_ = GURL(proto.next_feed_url());
  upload_url_ = GURL(proto.upload_url());
  origin_ = ContentOrigin(proto.origin());
  for (int i = 0; i < proto.child_files_size(); ++i) {
    scoped_ptr<GDataFile> file(new GDataFile(this, root_));
    file->FromProto(proto.child_files(i));
    AddEntry(file.release());
  }
  for (int i = 0; i < proto.child_directories_size(); ++i) {
    scoped_ptr<GDataDirectory> dir(new GDataDirectory(this, root_));
    dir->FromProto(proto.child_directories(i));
    AddEntry(dir.release());
  }
}

void GDataDirectory::ToProto(GDataDirectoryProto* proto) const {
  GDataEntry::ToProto(proto->mutable_gdata_entry());
  DCHECK(proto->gdata_entry().file_info().is_directory());
  proto->set_refresh_time(refresh_time_.ToInternalValue());
  proto->set_start_feed_url(start_feed_url_.spec());
  proto->set_next_feed_url(next_feed_url_.spec());
  proto->set_upload_url(upload_url_.spec());
  proto->set_origin(origin_);
  for (GDataFileCollection::const_iterator iter = child_files_.begin();
       iter != child_files_.end(); ++iter) {
    GDataFile* file = iter->second;
    file->ToProto(proto->add_child_files());
  }
  for (GDataDirectoryCollection::const_iterator iter =
       child_directories_.begin();
       iter != child_directories_.end(); ++iter) {
    GDataDirectory* dir = iter->second;
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

void GDataEntry::SerializeToString(std::string* serialized_proto) const {
  const GDataFile* file = AsGDataFileConst();
  const GDataDirectory* dir = AsGDataDirectoryConst();

  if (file) {
    scoped_ptr<GDataFileProto> proto(new GDataFileProto());
    file->ToProto(proto.get());
    const bool ok = proto->SerializeToString(serialized_proto);
    DCHECK(ok);
  } else if (dir) {
    scoped_ptr<GDataDirectoryProto> proto(new GDataDirectoryProto());
    dir->ToProto(proto.get());
    const bool ok = proto->SerializeToString(serialized_proto);
    DCHECK(ok);
  }
}

// static
scoped_ptr<GDataEntry> GDataEntry::FromProtoString(
    const std::string& serialized_proto) {
  // First try to parse as GDataDirectoryProto. Note that this can succeed for
  // a serialized_proto that's really a GDataFileProto - we have to check
  // is_directory to be sure.
  scoped_ptr<GDataDirectoryProto> dir_proto(new GDataDirectoryProto());
  bool ok = dir_proto->ParseFromString(serialized_proto);
  if (ok && dir_proto->gdata_entry().file_info().is_directory()) {
    GDataDirectory* dir = new GDataDirectory(NULL, NULL);
    dir->FromProto(*dir_proto);
    return scoped_ptr<GDataEntry>(dir);
  }

  scoped_ptr<GDataFileProto> file_proto(new GDataFileProto());
  ok = file_proto->ParseFromString(serialized_proto);
  if (ok) {
    DCHECK(!file_proto->gdata_entry().file_info().is_directory());
    GDataFile* file = new GDataFile(NULL, NULL);
    file->FromProto(*file_proto);
    return scoped_ptr<GDataEntry>(file);
  }
  return scoped_ptr<GDataEntry>(NULL);
}

void GDataRootDirectory::SerializeToString(
    std::string* serialized_proto) const {
  scoped_ptr<GDataRootDirectoryProto> proto(
      new GDataRootDirectoryProto());
  ToProto(proto.get());
  const bool ok = proto->SerializeToString(serialized_proto);
  DCHECK(ok);
}

bool GDataRootDirectory::ParseFromString(const std::string& serialized_proto) {
  scoped_ptr<GDataRootDirectoryProto> proto(
      new GDataRootDirectoryProto());
  bool ok = proto->ParseFromString(serialized_proto);
  if (ok) {
    FromProto(*proto.get());
    set_origin(FROM_CACHE);
    set_refresh_time(base::Time::Now());
  }
  return ok;
}

}  // namespace gdata
