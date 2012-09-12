// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/drive_files.h"

#include "base/platform_file.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/gdata/drive.pb.h"
#include "chrome/browser/chromeos/gdata/drive_file_system_util.h"
#include "chrome/browser/chromeos/gdata/drive_resource_metadata.h"
#include "chrome/browser/chromeos/gdata/gdata_wapi_parser.h"
#include "net/base/escape.h"

namespace gdata {

// DriveEntry class.

DriveEntry::DriveEntry(DriveResourceMetadata* resource_metadata)
    : parent_(NULL),
      resource_metadata_(resource_metadata),
      deleted_(false) {
  DCHECK(resource_metadata);
}

DriveEntry::~DriveEntry() {
}

DriveFile* DriveEntry::AsDriveFile() {
  return NULL;
}

DriveDirectory* DriveEntry::AsDriveDirectory() {
  return NULL;
}

void DriveEntry::InitFromDocumentEntry(const DocumentEntry& doc) {
  // For regular files, the 'filename' and 'title' attribute in the metadata
  // may be different (e.g. due to rename). To be consistent with the web
  // interface and other client to use the 'title' attribute, instead of
  // 'filename', as the file name in the local snapshot.
  title_ = UTF16ToUTF8(doc.title());
  // SetBaseNameFromTitle() must be called after |title_| is set.
  SetBaseNameFromTitle();

  // TODO(satorux): The last modified and the last accessed time shouldn't
  // be treated as the same thing: crbug.com/148434
  file_info_.last_modified = doc.updated_time();
  file_info_.last_accessed = doc.updated_time();
  file_info_.creation_time = doc.published_time();

  resource_id_ = doc.resource_id();
  content_url_ = doc.content_url();
  deleted_ = doc.deleted();

  const Link* edit_link = doc.GetLinkByType(Link::LINK_EDIT);
  if (edit_link)
    edit_url_ = edit_link->href();

  const Link* parent_link = doc.GetLinkByType(Link::LINK_PARENT);
  if (parent_link)
    parent_resource_id_ = util::ExtractResourceIdFromUrl(parent_link->href());
}

const DriveFile* DriveEntry::AsDriveFileConst() const {
  // cast away const and call the non-const version. This is safe.
  return const_cast<DriveEntry*>(this)->AsDriveFile();
}

const DriveDirectory* DriveEntry::AsDriveDirectoryConst() const {
  // cast away const and call the non-const version. This is safe.
  return const_cast<DriveEntry*>(this)->AsDriveDirectory();
}

FilePath DriveEntry::GetFilePath() const {
  FilePath path;
  if (parent())
    path = parent()->GetFilePath();
  path = path.Append(base_name());
  return path;
}

void DriveEntry::SetParent(DriveDirectory* parent) {
  parent_ = parent;
  parent_resource_id_ = parent ? parent->resource_id() : "";
}

void DriveEntry::SetBaseNameFromTitle() {
  base_name_ = util::EscapeUtf8FileName(title_);
}

// DriveFile class implementation.

DriveFile::DriveFile(DriveResourceMetadata* resource_metadata)
    : DriveEntry(resource_metadata),
      kind_(ENTRY_KIND_UNKNOWN),
      is_hosted_document_(false) {
  file_info_.is_directory = false;
}

DriveFile::~DriveFile() {
}

DriveFile* DriveFile::AsDriveFile() {
  return this;
}

void DriveFile::SetBaseNameFromTitle() {
  if (is_hosted_document_) {
    base_name_ = util::EscapeUtf8FileName(title_ + document_extension_);
  } else {
    DriveEntry::SetBaseNameFromTitle();
  }
}

void DriveFile::InitFromDocumentEntry(const DocumentEntry& doc) {
  DriveEntry::InitFromDocumentEntry(doc);

  // Check if this entry is a true file, or...
  if (doc.is_file()) {
    file_info_.size = doc.file_size();
    file_md5_ = doc.file_md5();

    // The resumable-edit-media link should only be present for regular
    // files as hosted documents are not uploadable.
    const Link* upload_link =
        doc.GetLinkByType(Link::LINK_RESUMABLE_EDIT_MEDIA);
    if (upload_link)
      upload_url_ = upload_link->href();
  } else {
    // ... a hosted document.
    // Attach .g<something> extension to hosted documents so we can special
    // case their handling in UI.
    // TODO(zelidrag): Figure out better way how to pass entry info like kind
    // to UI through the File API stack.
    document_extension_ = doc.GetHostedDocumentExtension();
    // We don't know the size of hosted docs and it does not matter since
    // is has no effect on the quota.
    file_info_.size = 0;
  }
  kind_ = doc.kind();
  content_mime_type_ = doc.content_mime_type();
  is_hosted_document_ = doc.is_hosted_document();
  // SetBaseNameFromTitle() must be called after |title_|,
  // |is_hosted_document_| and |document_extension_| are set.
  SetBaseNameFromTitle();

  const Link* thumbnail_link = doc.GetLinkByType(Link::LINK_THUMBNAIL);
  if (thumbnail_link)
    thumbnail_url_ = thumbnail_link->href();

  const Link* alternate_link = doc.GetLinkByType(Link::LINK_ALTERNATE);
  if (alternate_link)
    alternate_url_ = alternate_link->href();
}

// DriveDirectory class implementation.

DriveDirectory::DriveDirectory(DriveResourceMetadata* resource_metadata)
    : DriveEntry(resource_metadata) {
  file_info_.is_directory = true;
}

DriveDirectory::~DriveDirectory() {
  RemoveChildren();
}

DriveDirectory* DriveDirectory::AsDriveDirectory() {
  return this;
}

void DriveDirectory::InitFromDocumentEntry(const DocumentEntry& doc) {
  DriveEntry::InitFromDocumentEntry(doc);

  const Link* upload_link =
      doc.GetLinkByType(Link::LINK_RESUMABLE_CREATE_MEDIA);
  if (upload_link)
    upload_url_ = upload_link->href();
}

void DriveDirectory::AddEntry(DriveEntry* entry) {
  DCHECK(!entry->parent());

  // The entry name may have been changed due to prior name de-duplication.
  // We need to first restore the file name based on the title before going
  // through name de-duplication again when it is added to another directory.
  entry->SetBaseNameFromTitle();

  // Do file name de-duplication - find files with the same name and
  // append a name modifier to the name.
  int max_modifier = 1;
  FilePath full_file_name(entry->base_name());
  const std::string extension = full_file_name.Extension();
  const std::string file_name = full_file_name.RemoveExtension().value();
  while (!FindChild(full_file_name.value()).empty()) {
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
  entry->set_base_name(full_file_name.value());

  DVLOG(1) << "AddEntry: dir = " << GetFilePath().value()
           << ", file = " + entry->base_name()
           << ", parent resource = " << entry->parent_resource_id()
           << ", resource = " + entry->resource_id();

  // Add entry to resource map.
  resource_metadata_->AddEntryToResourceMap(entry);

  // Setup child and parent links.
  if (entry->AsDriveFile())
    child_files_.insert(std::make_pair(entry->base_name(),
                                       entry->resource_id()));

  if (entry->AsDriveDirectory())
    child_directories_.insert(std::make_pair(entry->base_name(),
                                             entry->resource_id()));
  entry->SetParent(this);
}

void DriveDirectory::TakeOverEntries(DriveDirectory* dir) {
  for (ChildMap::const_iterator iter = dir->child_files_.begin();
       iter != dir->child_files_.end(); ++iter) {
    TakeOverEntry(iter->second);
  }
  dir->child_files_.clear();

  for (ChildMap::iterator iter = dir->child_directories_.begin();
       iter != dir->child_directories_.end(); ++iter) {
    TakeOverEntry(iter->second);
  }
  dir->child_directories_.clear();
}

void DriveDirectory::TakeOverEntry(const std::string& resource_id) {
  DriveEntry* entry = resource_metadata_->GetEntryByResourceId(resource_id);
  DCHECK(entry);
  resource_metadata_->RemoveEntryFromResourceMap(resource_id);
  entry->SetParent(NULL);
  AddEntry(entry);
}

void DriveDirectory::RemoveEntry(DriveEntry* entry) {
  DCHECK(entry);

  RemoveChild(entry);
  delete entry;
}

std::string DriveDirectory::FindChild(
    const FilePath::StringType& file_name) const {
  ChildMap::const_iterator iter = child_files_.find(file_name);
  if (iter != child_files_.end())
    return iter->second;

  iter = child_directories_.find(file_name);
  if (iter != child_directories_.end())
    return iter->second;

  return std::string();
}

void DriveDirectory::RemoveChild(DriveEntry* entry) {
  DCHECK(entry);

  const std::string& base_name(entry->base_name());
  // entry must be present in this directory.
  DCHECK_EQ(entry->resource_id(), FindChild(base_name));
  // Remove entry from resource map first.
  resource_metadata_->RemoveEntryFromResourceMap(entry->resource_id());

  // Then delete it from tree.
  child_files_.erase(base_name);
  child_directories_.erase(base_name);

  entry->SetParent(NULL);
}

void DriveDirectory::RemoveChildren() {
  RemoveChildFiles();
  RemoveChildDirectories();
}

void DriveDirectory::RemoveChildFiles() {
  DVLOG(1) << "RemoveChildFiles " << resource_id();
  for (ChildMap::const_iterator iter = child_files_.begin();
       iter != child_files_.end(); ++iter) {
    DriveEntry* child = resource_metadata_->GetEntryByResourceId(iter->second);
    DCHECK(child);
    resource_metadata_->RemoveEntryFromResourceMap(iter->second);
    delete child;
  }
  child_files_.clear();
}

void DriveDirectory::RemoveChildDirectories() {
  for (ChildMap::iterator iter = child_directories_.begin();
       iter != child_directories_.end(); ++iter) {
    DriveDirectory* dir = resource_metadata_->GetEntryByResourceId(
        iter->second)->AsDriveDirectory();
    DCHECK(dir);
    // Remove directories recursively.
    dir->RemoveChildren();
    resource_metadata_->RemoveEntryFromResourceMap(iter->second);
    delete dir;
  }
  child_directories_.clear();
}

void DriveDirectory::GetChildDirectoryPaths(std::set<FilePath>* child_dirs) {
  for (ChildMap::const_iterator iter = child_directories_.begin();
       iter != child_directories_.end(); ++iter) {
    DriveDirectory* dir = resource_metadata_->GetEntryByResourceId(
        iter->second)->AsDriveDirectory();
    DCHECK(dir);
    child_dirs->insert(dir->GetFilePath());
    dir->GetChildDirectoryPaths(child_dirs);
  }
}

// Convert to/from proto.

// static
void DriveEntry::ConvertProtoToPlatformFileInfo(
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
void DriveEntry::ConvertPlatformFileInfoToProto(
    const base::PlatformFileInfo& file_info,
    PlatformFileInfoProto* proto) {
  proto->set_size(file_info.size);
  proto->set_is_directory(file_info.is_directory);
  proto->set_is_symbolic_link(file_info.is_symbolic_link);
  proto->set_last_modified(file_info.last_modified.ToInternalValue());
  proto->set_last_accessed(file_info.last_accessed.ToInternalValue());
  proto->set_creation_time(file_info.creation_time.ToInternalValue());
}

void DriveEntry::FromProto(const DriveEntryProto& proto) {
  ConvertProtoToPlatformFileInfo(proto.file_info(), &file_info_);

  // Don't copy from proto.base_name() as base_name_ is computed in
  // SetBaseNameFromTitle().
  title_ = proto.title();
  resource_id_ = proto.resource_id();
  parent_resource_id_ = proto.parent_resource_id();
  edit_url_ = GURL(proto.edit_url());
  content_url_ = GURL(proto.content_url());
  upload_url_ = GURL(proto.upload_url());
  SetBaseNameFromTitle();
}

void DriveEntry::ToProto(DriveEntryProto* proto) const {
  ConvertPlatformFileInfoToProto(file_info_, proto->mutable_file_info());

  // The base_name field is used in GetFileInfoByPathAsync(). As shown in
  // FromProto(), the value is discarded when deserializing from proto.
  proto->set_base_name(base_name_);
  proto->set_title(title_);
  proto->set_resource_id(resource_id_);
  proto->set_parent_resource_id(parent_resource_id_);
  proto->set_edit_url(edit_url_.spec());
  proto->set_content_url(content_url_.spec());
  proto->set_upload_url(upload_url_.spec());
}

void DriveEntry::ToProtoFull(DriveEntryProto* proto) const {
  if (AsDriveFileConst()) {
    AsDriveFileConst()->ToProto(proto);
  } else if (AsDriveDirectoryConst()) {
    // Unlike files, directories don't have directory specific info, so just
    // calling DriveEntry::ToProto().
    ToProto(proto);
  } else {
    NOTREACHED();
  }
}

void DriveFile::FromProto(const DriveEntryProto& proto) {
  DCHECK(!proto.file_info().is_directory());

  DriveEntry::FromProto(proto);

  thumbnail_url_ = GURL(proto.file_specific_info().thumbnail_url());
  alternate_url_ = GURL(proto.file_specific_info().alternate_url());
  content_mime_type_ = proto.file_specific_info().content_mime_type();
  file_md5_ = proto.file_specific_info().file_md5();
  document_extension_ = proto.file_specific_info().document_extension();
  is_hosted_document_ = proto.file_specific_info().is_hosted_document();
}

void DriveFile::ToProto(DriveEntryProto* proto) const {
  DriveEntry::ToProto(proto);
  DCHECK(!proto->file_info().is_directory());
  DriveFileSpecificInfo* file_specific_info =
      proto->mutable_file_specific_info();
  file_specific_info->set_thumbnail_url(thumbnail_url_.spec());
  file_specific_info->set_alternate_url(alternate_url_.spec());
  file_specific_info->set_content_mime_type(content_mime_type_);
  file_specific_info->set_file_md5(file_md5_);
  file_specific_info->set_document_extension(document_extension_);
  file_specific_info->set_is_hosted_document(is_hosted_document_);
}

void DriveDirectory::FromProto(const DriveDirectoryProto& proto) {
  DCHECK(proto.drive_entry().file_info().is_directory());
  DCHECK(!proto.drive_entry().has_file_specific_info());

  for (int i = 0; i < proto.child_files_size(); ++i) {
    scoped_ptr<DriveFile> file(resource_metadata_->CreateDriveFile());
    file->FromProto(proto.child_files(i));
    AddEntry(file.release());
  }
  for (int i = 0; i < proto.child_directories_size(); ++i) {
    scoped_ptr<DriveDirectory> dir(resource_metadata_->CreateDriveDirectory());
    dir->FromProto(proto.child_directories(i));
    AddEntry(dir.release());
  }

  // The states of the directory should be updated after children are
  // handled successfully, so that incomplete states are not left.
  DriveEntry::FromProto(proto.drive_entry());
}

void DriveDirectory::ToProto(DriveDirectoryProto* proto) const {
  DriveEntry::ToProto(proto->mutable_drive_entry());
  DCHECK(proto->drive_entry().file_info().is_directory());

  for (ChildMap::const_iterator iter = child_files_.begin();
       iter != child_files_.end(); ++iter) {
    DriveFile* file = resource_metadata_->GetEntryByResourceId(
        iter->second)->AsDriveFile();
    DCHECK(file);
    file->ToProto(proto->add_child_files());
  }
  for (ChildMap::const_iterator iter = child_directories_.begin();
       iter != child_directories_.end(); ++iter) {
    DriveDirectory* dir = resource_metadata_->GetEntryByResourceId(
        iter->second)->AsDriveDirectory();
    DCHECK(dir);
    dir->ToProto(proto->add_child_directories());
  }
}

scoped_ptr<DriveEntryProtoVector> DriveDirectory::ToProtoVector() const {
  scoped_ptr<DriveEntryProtoVector> entries(new DriveEntryProtoVector);
  // Use ToProtoFull, as we don't want to include children in |proto|.
  for (ChildMap::const_iterator iter = child_files_.begin();
       iter != child_files_.end(); ++iter) {
    DriveEntryProto proto;
    resource_metadata_->GetEntryByResourceId(iter->second)->ToProtoFull(&proto);
    entries->push_back(proto);
  }
  for (ChildMap::const_iterator iter = child_directories_.begin();
       iter != child_directories_.end(); ++iter) {
    DriveEntryProto proto;
    resource_metadata_->GetEntryByResourceId(iter->second)->ToProtoFull(&proto);
    entries->push_back(proto);
  }

  return entries.Pass();
}

void DriveEntry::SerializeToString(std::string* serialized_proto) const {
  const DriveFile* file = AsDriveFileConst();
  const DriveDirectory* dir = AsDriveDirectoryConst();

  if (file) {
    DriveEntryProto entry_proto;
    file->ToProto(&entry_proto);
    const bool ok = entry_proto.SerializeToString(serialized_proto);
    DCHECK(ok);
  } else if (dir) {
    DriveDirectoryProto dir_proto;
    dir->ToProto(&dir_proto);
    const bool ok = dir_proto.SerializeToString(serialized_proto);
    DCHECK(ok);
  }
}

}  // namespace gdata
