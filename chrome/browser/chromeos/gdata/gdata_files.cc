// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_files.h"

#include "base/platform_file.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/gdata/gdata.pb.h"
#include "chrome/browser/chromeos/gdata/gdata_directory_service.h"
#include "chrome/browser/chromeos/gdata/gdata_wapi_parser.h"
#include "net/base/escape.h"

namespace gdata {
namespace {

const char kSlash[] = "/";
const char kEscapedSlash[] = "\xE2\x88\x95";

// Extracts resource_id out of edit url.
std::string ExtractResourceId(const GURL& url) {
  return net::UnescapeURLComponent(url.ExtractFileName(),
                                   net::UnescapeRule::URL_SPECIAL_CHARS);
}

}  // namespace

// GDataEntry class.

GDataEntry::GDataEntry(GDataDirectoryService* directory_service)
    : parent_(NULL),
      directory_service_(directory_service),
      deleted_(false) {
  DCHECK(directory_service);
}

GDataEntry::~GDataEntry() {
}

GDataFile* GDataEntry::AsGDataFile() {
  return NULL;
}

GDataDirectory* GDataEntry::AsGDataDirectory() {
  return NULL;
}

void GDataEntry::InitFromDocumentEntry(const DocumentEntry& doc) {
  // For regular files, the 'filename' and 'title' attribute in the metadata
  // may be different (e.g. due to rename). To be consistent with the web
  // interface and other client to use the 'title' attribute, instead of
  // 'filename', as the file name in the local snapshot.
  title_ = UTF16ToUTF8(doc.title());
  // SetBaseNameFromTitle() must be called after |title_| is set.
  SetBaseNameFromTitle();

  file_info_.last_modified = doc.updated_time();
  file_info_.last_accessed = doc.updated_time();
  file_info_.creation_time = doc.published_time();

  resource_id_ = doc.resource_id();
  content_url_ = doc.content_url();
  deleted_ = doc.deleted();

  const Link* edit_link = doc.GetLinkByType(Link::EDIT);
  if (edit_link)
    edit_url_ = edit_link->href();

  const Link* parent_link = doc.GetLinkByType(Link::PARENT);
  if (parent_link)
    parent_resource_id_ = ExtractResourceId(parent_link->href());
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
  path = path.Append(base_name());
  return path;
}

void GDataEntry::SetParent(GDataDirectory* parent) {
  parent_ = parent;
  parent_resource_id_ = parent ? parent->resource_id() : "";
}

void GDataEntry::SetBaseNameFromTitle() {
  base_name_ = EscapeUtf8FileName(title_);
}

// static
std::string GDataEntry::EscapeUtf8FileName(const std::string& input) {
  std::string output;
  if (ReplaceChars(input, kSlash, std::string(kEscapedSlash), &output))
    return output;

  return input;
}

// static
std::string GDataEntry::UnescapeUtf8FileName(const std::string& input) {
  std::string output = input;
  ReplaceSubstringsAfterOffset(&output, 0, std::string(kEscapedSlash), kSlash);
  return output;
}

// GDataFile class implementation.

GDataFile::GDataFile(GDataDirectoryService* directory_service)
    : GDataEntry(directory_service),
      kind_(DocumentEntry::UNKNOWN),
      is_hosted_document_(false) {
  file_info_.is_directory = false;
}

GDataFile::~GDataFile() {
}

GDataFile* GDataFile::AsGDataFile() {
  return this;
}

void GDataFile::SetBaseNameFromTitle() {
  if (is_hosted_document_) {
    base_name_ = EscapeUtf8FileName(title_ + document_extension_);
  } else {
    GDataEntry::SetBaseNameFromTitle();
  }
}

void GDataFile::InitFromDocumentEntry(const DocumentEntry& doc) {
  GDataEntry::InitFromDocumentEntry(doc);

  // Check if this entry is a true file, or...
  if (doc.is_file()) {
    file_info_.size = doc.file_size();
    file_md5_ = doc.file_md5();

    // The resumable-edit-media link should only be present for regular
    // files as hosted documents are not uploadable.
    const Link* upload_link = doc.GetLinkByType(Link::RESUMABLE_EDIT_MEDIA);
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

  const Link* thumbnail_link = doc.GetLinkByType(Link::THUMBNAIL);
  if (thumbnail_link)
    thumbnail_url_ = thumbnail_link->href();

  const Link* alternate_link = doc.GetLinkByType(Link::ALTERNATE);
  if (alternate_link)
    alternate_url_ = alternate_link->href();
}

// GDataDirectory class implementation.

GDataDirectory::GDataDirectory(GDataDirectoryService* directory_service)
    : GDataEntry(directory_service) {
  file_info_.is_directory = true;
}

GDataDirectory::~GDataDirectory() {
  RemoveChildren();
}

GDataDirectory* GDataDirectory::AsGDataDirectory() {
  return this;
}

void GDataDirectory::InitFromDocumentEntry(const DocumentEntry& doc) {
  GDataEntry::InitFromDocumentEntry(doc);

  const Link* upload_link = doc.GetLinkByType(Link::RESUMABLE_CREATE_MEDIA);
  if (upload_link)
    upload_url_ = upload_link->href();
}

void GDataDirectory::AddEntry(GDataEntry* entry) {
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
  entry->set_base_name(full_file_name.value());

  DVLOG(1) << "AddEntry: dir = " << GetFilePath().value()
           << ", file = " + entry->base_name()
           << ", parent resource = " << entry->parent_resource_id()
           << ", resource = " + entry->resource_id();

  // Add entry to resource map.
  directory_service_->AddEntryToResourceMap(entry);

  // Setup child and parent links.
  AddChild(entry);
  entry->SetParent(this);
}

bool GDataDirectory::TakeOverEntries(GDataDirectory* dir) {
  for (GDataFileCollection::const_iterator iter = dir->child_files_.begin();
       iter != dir->child_files_.end(); ++iter) {
    GDataEntry* entry = iter->second;
    directory_service_->RemoveEntryFromResourceMap(entry->resource_id());
    entry->SetParent(NULL);
    AddEntry(entry);
  }
  dir->child_files_.clear();

  for (GDataDirectoryCollection::iterator iter =
      dir->child_directories_.begin();
       iter != dir->child_directories_.end(); ++iter) {
    GDataEntry* entry = iter->second;
    directory_service_->RemoveEntryFromResourceMap(entry->resource_id());
    entry->SetParent(NULL);
    AddEntry(entry);
  }
  dir->child_directories_.clear();
  return true;
}

void GDataDirectory::RemoveEntry(GDataEntry* entry) {
  DCHECK(entry);

  RemoveChild(entry);
  delete entry;
}

GDataEntry* GDataDirectory::FindChild(
    const FilePath::StringType& file_name) const {
  GDataFileCollection::const_iterator iter = child_files_.find(file_name);
  if (iter != child_files_.end())
    return iter->second;

  GDataDirectoryCollection::const_iterator itd =
      child_directories_.find(file_name);
  if (itd != child_directories_.end())
    return itd->second;

  return NULL;
}

void GDataDirectory::AddChild(GDataEntry* entry) {
  DCHECK(entry);

  if (entry->AsGDataFile())
    child_files_.insert(std::make_pair(entry->base_name(),
                                       entry->AsGDataFile()));

  if (entry->AsGDataDirectory())
    child_directories_.insert(std::make_pair(entry->base_name(),
                                             entry->AsGDataDirectory()));
}

void GDataDirectory::RemoveChild(GDataEntry* entry) {
  DCHECK(entry);

  const std::string& base_name(entry->base_name());
  // entry must be present in this directory.
  DCHECK_EQ(entry, FindChild(base_name));
  // Remove entry from resource map first.
  directory_service_->RemoveEntryFromResourceMap(entry->resource_id());

  // Then delete it from tree.
  child_files_.erase(base_name);
  child_directories_.erase(base_name);

  entry->SetParent(NULL);
}

void GDataDirectory::RemoveChildren() {
  RemoveChildFiles();
  RemoveChildDirectories();
}

void GDataDirectory::RemoveChildFiles() {
  DVLOG(1) << "RemoveChildFiles " << resource_id();
  for (GDataFileCollection::const_iterator iter = child_files_.begin();
       iter != child_files_.end(); ++iter) {
    directory_service_->RemoveEntryFromResourceMap(iter->second->resource_id());
  }
  STLDeleteValues(&child_files_);
  child_files_.clear();
}

void GDataDirectory::RemoveChildDirectories() {
  for (GDataDirectoryCollection::iterator iter = child_directories_.begin();
       iter != child_directories_.end(); ++iter) {
    GDataDirectory* dir = iter->second;
    // Remove directories recursively.
    dir->RemoveChildren();
    directory_service_->RemoveEntryFromResourceMap(dir->resource_id());
  }
  STLDeleteValues(&child_directories_);
  child_directories_.clear();
}

void GDataDirectory::GetChildDirectoryPaths(std::set<FilePath>* child_dirs) {
  for (GDataDirectoryCollection::const_iterator it = child_directories_.begin();
       it != child_directories_.end(); ++it) {
    GDataDirectory* child_dir = it->second;
    child_dirs->insert(child_dir->GetFilePath());
    child_dir->GetChildDirectoryPaths(child_dirs);
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

bool GDataEntry::FromProto(const GDataEntryProto& proto) {
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

  // Reject older protobuf that does not contain the upload URL.  This URL is
  // necessary for uploading files.
  if (!proto.has_upload_url()) {
    LOG(ERROR) << "Incompatible proto detected (no upload URL): "
               << proto.title();
    return false;
  }

  return true;
}

void GDataEntry::ToProto(GDataEntryProto* proto) const {
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

void GDataEntry::ToProtoFull(GDataEntryProto* proto) const {
  if (AsGDataFileConst()) {
    AsGDataFileConst()->ToProto(proto);
  } else if (AsGDataDirectoryConst()) {
    // Unlike files, directories don't have directory specific info, so just
    // calling GDataEntry::ToProto().
    ToProto(proto);
  } else {
    NOTREACHED();
  }
}

bool GDataFile::FromProto(const GDataEntryProto& proto) {
  DCHECK(!proto.file_info().is_directory());

  if (!GDataEntry::FromProto(proto))
    return false;

  thumbnail_url_ = GURL(proto.file_specific_info().thumbnail_url());
  alternate_url_ = GURL(proto.file_specific_info().alternate_url());
  content_mime_type_ = proto.file_specific_info().content_mime_type();
  file_md5_ = proto.file_specific_info().file_md5();
  document_extension_ = proto.file_specific_info().document_extension();
  is_hosted_document_ = proto.file_specific_info().is_hosted_document();

  return true;
}

void GDataFile::ToProto(GDataEntryProto* proto) const {
  GDataEntry::ToProto(proto);
  DCHECK(!proto->file_info().is_directory());
  GDataFileSpecificInfo* file_specific_info =
      proto->mutable_file_specific_info();
  file_specific_info->set_thumbnail_url(thumbnail_url_.spec());
  file_specific_info->set_alternate_url(alternate_url_.spec());
  file_specific_info->set_content_mime_type(content_mime_type_);
  file_specific_info->set_file_md5(file_md5_);
  file_specific_info->set_document_extension(document_extension_);
  file_specific_info->set_is_hosted_document(is_hosted_document_);
}

bool GDataDirectory::FromProto(const GDataDirectoryProto& proto) {
  DCHECK(proto.gdata_entry().file_info().is_directory());
  DCHECK(!proto.gdata_entry().has_file_specific_info());

  for (int i = 0; i < proto.child_files_size(); ++i) {
    scoped_ptr<GDataFile> file(directory_service_->CreateGDataFile());
    if (!file->FromProto(proto.child_files(i))) {
      RemoveChildren();
      return false;
    }
    AddEntry(file.release());
  }
  for (int i = 0; i < proto.child_directories_size(); ++i) {
    scoped_ptr<GDataDirectory> dir(directory_service_->CreateGDataDirectory());
    if (!dir->FromProto(proto.child_directories(i))) {
      RemoveChildren();
      return false;
    }
    AddEntry(dir.release());
  }

  // The states of the directory should be updated after children are
  // handled successfully, so that incomplete states are not left.
  if (!GDataEntry::FromProto(proto.gdata_entry()))
    return false;

  return true;
}

void GDataDirectory::ToProto(GDataDirectoryProto* proto) const {
  GDataEntry::ToProto(proto->mutable_gdata_entry());
  DCHECK(proto->gdata_entry().file_info().is_directory());

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

scoped_ptr<GDataEntryProtoVector> GDataDirectory::ToProtoVector() const {
  scoped_ptr<GDataEntryProtoVector> entries(new GDataEntryProtoVector);
  for (GDataFileCollection::const_iterator iter = child_files_.begin();
       iter != child_files_.end(); ++iter) {
    GDataEntryProto proto;
    iter->second->ToProto(&proto);
    entries->push_back(proto);
  }
  for (GDataDirectoryCollection::const_iterator iter =
           child_directories_.begin();
       iter != child_directories_.end(); ++iter) {
    GDataEntryProto proto;
    // Convert to GDataEntry, as we don't want to include children in |proto|.
    static_cast<const GDataEntry*>(iter->second)->ToProtoFull(&proto);
    entries->push_back(proto);
  }

  return entries.Pass();
}

void GDataEntry::SerializeToString(std::string* serialized_proto) const {
  const GDataFile* file = AsGDataFileConst();
  const GDataDirectory* dir = AsGDataDirectoryConst();

  if (file) {
    GDataEntryProto entry_proto;
    file->ToProto(&entry_proto);
    const bool ok = entry_proto.SerializeToString(serialized_proto);
    DCHECK(ok);
  } else if (dir) {
    GDataDirectoryProto dir_proto;
    dir->ToProto(&dir_proto);
    const bool ok = dir_proto.SerializeToString(serialized_proto);
    DCHECK(ok);
  }
}

}  // namespace gdata
