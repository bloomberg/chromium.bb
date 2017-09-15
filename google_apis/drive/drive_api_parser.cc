// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/drive/drive_api_parser.h"

#include <stddef.h>

#include <memory>

#include "base/json/json_value_converter.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "google_apis/drive/time_util.h"

namespace google_apis {

namespace {

const int64_t kUnsetFileSize = -1;

bool CreateFileResourceFromValue(const base::Value* value,
                                 std::unique_ptr<FileResource>* file) {
  *file = FileResource::CreateFrom(*value);
  return !!*file;
}

bool CreateTeamDriveResourceFromValue(
    const base::Value* value,
    std::unique_ptr<TeamDriveResource>* file) {
  *file = TeamDriveResource::CreateFrom(*value);
  return !!*file;
}

// Converts |url_string| to |result|.  Always returns true to be used
// for JSONValueConverter::RegisterCustomField method.
// TODO(mukai): make it return false in case of invalid |url_string|.
bool GetGURLFromString(const base::StringPiece& url_string, GURL* result) {
  *result = GURL(url_string);
  return true;
}

// Converts |value| to |result|.
bool GetParentsFromValue(const base::Value* value,
                         std::vector<ParentReference>* result) {
  DCHECK(value);
  DCHECK(result);

  const base::ListValue* list_value = NULL;
  if (!value->GetAsList(&list_value))
    return false;

  base::JSONValueConverter<ParentReference> converter;
  result->resize(list_value->GetSize());
  for (size_t i = 0; i < list_value->GetSize(); ++i) {
    const base::Value* parent_value = NULL;
    if (!list_value->Get(i, &parent_value) ||
        !converter.Convert(*parent_value, &(*result)[i]))
      return false;
  }

  return true;
}

// Converts |value| to |result|. The key of |value| is app_id, and its value
// is URL to open the resource on the web app.
bool GetOpenWithLinksFromDictionaryValue(
    const base::Value* value,
    std::vector<FileResource::OpenWithLink>* result) {
  DCHECK(value);
  DCHECK(result);

  const base::DictionaryValue* dictionary_value;
  if (!value->GetAsDictionary(&dictionary_value))
    return false;

  result->reserve(dictionary_value->size());
  for (base::DictionaryValue::Iterator iter(*dictionary_value);
       !iter.IsAtEnd(); iter.Advance()) {
    std::string string_value;
    if (!iter.value().GetAsString(&string_value))
      return false;

    FileResource::OpenWithLink open_with_link;
    open_with_link.app_id = iter.key();
    open_with_link.open_url = GURL(string_value);
    result->push_back(open_with_link);
  }

  return true;
}

// Drive v2 API JSON names.

// Definition order follows the order of documentation in
// https://developers.google.com/drive/v2/reference/

// Common
const char kKind[] = "kind";
const char kId[] = "id";
const char kETag[] = "etag";
const char kItems[] = "items";
const char kLargestChangeId[] = "largestChangeId";
const char kNextPageToken[] = "nextPageToken";

// About Resource
// https://developers.google.com/drive/v2/reference/about
const char kAboutKind[] = "drive#about";
const char kQuotaBytesTotal[] = "quotaBytesTotal";
const char kQuotaBytesUsedAggregate[] = "quotaBytesUsedAggregate";
const char kRootFolderId[] = "rootFolderId";

// App Icon
// https://developers.google.com/drive/v2/reference/apps
const char kCategory[] = "category";
const char kSize[] = "size";
const char kIconUrl[] = "iconUrl";

// Apps Resource
// https://developers.google.com/drive/v2/reference/apps
const char kAppKind[] = "drive#app";
const char kName[] = "name";
const char kObjectType[] = "objectType";
const char kProductId[] = "productId";
const char kSupportsCreate[] = "supportsCreate";
const char kRemovable[] = "removable";
const char kPrimaryMimeTypes[] = "primaryMimeTypes";
const char kSecondaryMimeTypes[] = "secondaryMimeTypes";
const char kPrimaryFileExtensions[] = "primaryFileExtensions";
const char kSecondaryFileExtensions[] = "secondaryFileExtensions";
const char kIcons[] = "icons";
const char kCreateUrl[] = "createUrl";

// Apps List
// https://developers.google.com/drive/v2/reference/apps/list
const char kAppListKind[] = "drive#appList";

// Parent Resource
// https://developers.google.com/drive/v2/reference/parents
const char kParentReferenceKind[] = "drive#parentReference";

// File Resource
// https://developers.google.com/drive/v2/reference/files
const char kFileKind[] = "drive#file";
const char kTitle[] = "title";
const char kMimeType[] = "mimeType";
const char kCreatedDate[] = "createdDate";
const char kModificationDate[] = "modificationDate";
const char kModifiedDate[] = "modifiedDate";
const char kModifiedByMeDate[] = "modifiedByMeDate";
const char kLastViewedByMeDate[] = "lastViewedByMeDate";
const char kSharedWithMeDate[] = "sharedWithMeDate";
const char kMd5Checksum[] = "md5Checksum";
const char kFileSize[] = "fileSize";
const char kAlternateLink[] = "alternateLink";
const char kParents[] = "parents";
const char kOpenWithLinks[] = "openWithLinks";
const char kLabels[] = "labels";
const char kImageMediaMetadata[] = "imageMediaMetadata";
const char kShared[] = "shared";
// These 5 flags are defined under |labels|.
const char kLabelTrashed[] = "trashed";
const char kLabelStarred[] = "starred";
// These 3 flags are defined under |imageMediaMetadata|.
const char kImageMediaMetadataWidth[] = "width";
const char kImageMediaMetadataHeight[] = "height";
const char kImageMediaMetadataRotation[] = "rotation";
// URL to the share dialog UI, which is provided only in v2internal.
const char kShareLink[] = "shareLink";

const char kDriveFolderMimeType[] = "application/vnd.google-apps.folder";

// Team Drive
const char kTeamDriveKind[] = "drive#teamDrive";
const char kTeamDriveListKind[] = "drive#teamDriveList";
const char kCapabilities[] = "capabilities";

// Team Drive capabilities.
const char kCanAddChildren[] = "canAddChildren";
const char kCanComment[] = "canComment";
const char kCanCopy[] = "canCopy";
const char kCanDeleteTeamDrive[] = "canDeleteTeamDrive";
const char kCanDownload[] = "canDownload";
const char kCanEdit[] = "canEdit";
const char kCanListChildren[] = "canListChildren";
const char kCanManageMembers[] = "canManageMembers";
const char kCanReadRevisions[] = "canReadRevisions";
const char kCanRemoveChildren[] = "canRemoveChildren";
const char kCanRename[] = "canRename";
const char kCanRenameTeamDrive[] = "canRenameTeamDrive";
const char kCanShare[] = "canShare";

// Files List
// https://developers.google.com/drive/v2/reference/files/list
const char kFileListKind[] = "drive#fileList";
const char kNextLink[] = "nextLink";

// Change Resource
// https://developers.google.com/drive/v2/reference/changes
const char kChangeKind[] = "drive#change";
const char kType[] = "type";
const char kFileId[] = "fileId";
const char kDeleted[] = "deleted";
const char kFile[] = "file";
const char kTeamDrive[] = "teamDrive";
const char kTeamDriveId[] = "teamDriveId";

// Changes List
// https://developers.google.com/drive/v2/reference/changes/list
const char kChangeListKind[] = "drive#changeList";

// Maps category name to enum ChangeType.
struct ChangeTypeMap {
  ChangeResource::ChangeType type;
  const char* type_name;
};

constexpr ChangeTypeMap kChangeTypeMap[] = {
  { ChangeResource::FILE, "file" },
  { ChangeResource::TEAM_DRIVE, "teamDrive" },
};

// Maps category name to enum IconCategory.
struct AppIconCategoryMap {
  DriveAppIcon::IconCategory category;
  const char* category_name;
};

constexpr AppIconCategoryMap kAppIconCategoryMap[] = {
  { DriveAppIcon::DOCUMENT, "document" },
  { DriveAppIcon::APPLICATION, "application" },
  { DriveAppIcon::SHARED_DOCUMENT, "documentShared" },
};

// Checks if the JSON is expected kind.  In Drive API, JSON data structure has
// |kind| property which denotes the type of the structure (e.g. "drive#file").
bool IsResourceKindExpected(const base::Value& value,
                            const std::string& expected_kind) {
  const base::DictionaryValue* as_dict = NULL;
  std::string kind;
  return value.GetAsDictionary(&as_dict) &&
      as_dict->HasKey(kKind) &&
      as_dict->GetString(kKind, &kind) &&
      kind == expected_kind;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AboutResource implementation

AboutResource::AboutResource()
    : largest_change_id_(0),
      quota_bytes_total_(0),
      quota_bytes_used_aggregate_(0) {}

AboutResource::~AboutResource() {}

// static
std::unique_ptr<AboutResource> AboutResource::CreateFrom(
    const base::Value& value) {
  std::unique_ptr<AboutResource> resource(new AboutResource());
  if (!IsResourceKindExpected(value, kAboutKind) || !resource->Parse(value)) {
    LOG(ERROR) << "Unable to create: Invalid About resource JSON!";
    return std::unique_ptr<AboutResource>();
  }
  return resource;
}

// static
void AboutResource::RegisterJSONConverter(
    base::JSONValueConverter<AboutResource>* converter) {
  converter->RegisterCustomField<int64_t>(kLargestChangeId,
                                          &AboutResource::largest_change_id_,
                                          &base::StringToInt64);
  converter->RegisterCustomField<int64_t>(kQuotaBytesTotal,
                                          &AboutResource::quota_bytes_total_,
                                          &base::StringToInt64);
  converter->RegisterCustomField<int64_t>(
      kQuotaBytesUsedAggregate, &AboutResource::quota_bytes_used_aggregate_,
      &base::StringToInt64);
  converter->RegisterStringField(kRootFolderId,
                                 &AboutResource::root_folder_id_);
}

bool AboutResource::Parse(const base::Value& value) {
  base::JSONValueConverter<AboutResource> converter;
  if (!converter.Convert(value, this)) {
    LOG(ERROR) << "Unable to parse: Invalid About resource JSON!";
    return false;
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// DriveAppIcon implementation

DriveAppIcon::DriveAppIcon() : category_(UNKNOWN), icon_side_length_(0) {}

DriveAppIcon::~DriveAppIcon() {}

// static
void DriveAppIcon::RegisterJSONConverter(
    base::JSONValueConverter<DriveAppIcon>* converter) {
  converter->RegisterCustomField<IconCategory>(
      kCategory,
      &DriveAppIcon::category_,
      &DriveAppIcon::GetIconCategory);
  converter->RegisterIntField(kSize, &DriveAppIcon::icon_side_length_);
  converter->RegisterCustomField<GURL>(kIconUrl,
                                       &DriveAppIcon::icon_url_,
                                       GetGURLFromString);
}

// static
std::unique_ptr<DriveAppIcon> DriveAppIcon::CreateFrom(
    const base::Value& value) {
  std::unique_ptr<DriveAppIcon> resource(new DriveAppIcon());
  if (!resource->Parse(value)) {
    LOG(ERROR) << "Unable to create: Invalid DriveAppIcon JSON!";
    return std::unique_ptr<DriveAppIcon>();
  }
  return resource;
}

bool DriveAppIcon::Parse(const base::Value& value) {
  base::JSONValueConverter<DriveAppIcon> converter;
  if (!converter.Convert(value, this)) {
    LOG(ERROR) << "Unable to parse: Invalid DriveAppIcon";
    return false;
  }
  return true;
}

// static
bool DriveAppIcon::GetIconCategory(const base::StringPiece& category,
                                   DriveAppIcon::IconCategory* result) {
  for (size_t i = 0; i < arraysize(kAppIconCategoryMap); i++) {
    if (category == kAppIconCategoryMap[i].category_name) {
      *result = kAppIconCategoryMap[i].category;
      return true;
    }
  }
  DVLOG(1) << "Unknown icon category " << category;
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// AppResource implementation

AppResource::AppResource()
    : supports_create_(false),
      removable_(false) {
}

AppResource::~AppResource() {}

// static
void AppResource::RegisterJSONConverter(
    base::JSONValueConverter<AppResource>* converter) {
  converter->RegisterStringField(kId, &AppResource::application_id_);
  converter->RegisterStringField(kName, &AppResource::name_);
  converter->RegisterStringField(kObjectType, &AppResource::object_type_);
  converter->RegisterStringField(kProductId, &AppResource::product_id_);
  converter->RegisterBoolField(kSupportsCreate, &AppResource::supports_create_);
  converter->RegisterBoolField(kRemovable, &AppResource::removable_);
  converter->RegisterRepeatedString(kPrimaryMimeTypes,
                                    &AppResource::primary_mimetypes_);
  converter->RegisterRepeatedString(kSecondaryMimeTypes,
                                    &AppResource::secondary_mimetypes_);
  converter->RegisterRepeatedString(kPrimaryFileExtensions,
                                    &AppResource::primary_file_extensions_);
  converter->RegisterRepeatedString(kSecondaryFileExtensions,
                                    &AppResource::secondary_file_extensions_);
  converter->RegisterRepeatedMessage(kIcons, &AppResource::icons_);
  converter->RegisterCustomField<GURL>(kCreateUrl,
                                       &AppResource::create_url_,
                                       GetGURLFromString);
}

// static
std::unique_ptr<AppResource> AppResource::CreateFrom(const base::Value& value) {
  std::unique_ptr<AppResource> resource(new AppResource());
  if (!IsResourceKindExpected(value, kAppKind) || !resource->Parse(value)) {
    LOG(ERROR) << "Unable to create: Invalid AppResource JSON!";
    return std::unique_ptr<AppResource>();
  }
  return resource;
}

bool AppResource::Parse(const base::Value& value) {
  base::JSONValueConverter<AppResource> converter;
  if (!converter.Convert(value, this)) {
    LOG(ERROR) << "Unable to parse: Invalid AppResource";
    return false;
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// AppList implementation

AppList::AppList() {}

AppList::~AppList() {}

// static
void AppList::RegisterJSONConverter(
    base::JSONValueConverter<AppList>* converter) {
  converter->RegisterStringField(kETag, &AppList::etag_);
  converter->RegisterRepeatedMessage<AppResource>(kItems,
                                                   &AppList::items_);
}

// static
std::unique_ptr<AppList> AppList::CreateFrom(const base::Value& value) {
  std::unique_ptr<AppList> resource(new AppList());
  if (!IsResourceKindExpected(value, kAppListKind) || !resource->Parse(value)) {
    LOG(ERROR) << "Unable to create: Invalid AppList JSON!";
    return std::unique_ptr<AppList>();
  }
  return resource;
}

bool AppList::Parse(const base::Value& value) {
  base::JSONValueConverter<AppList> converter;
  if (!converter.Convert(value, this)) {
    LOG(ERROR) << "Unable to parse: Invalid AppList";
    return false;
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// TeamDriveCapabilities implementation

TeamDriveCapabilities::TeamDriveCapabilities()
    : can_add_children_(false),
      can_comment_(false),
      can_copy_(false),
      can_delete_team_drive_(false),
      can_download_(false),
      can_edit_(false),
      can_list_children_(false),
      can_manage_members_(false),
      can_read_revisions_(false),
      can_remove_children_(false),
      can_rename_(false),
      can_rename_team_drive_(false),
      can_share_(false) {
}

TeamDriveCapabilities::TeamDriveCapabilities(const TeamDriveCapabilities& src) =
    default;

TeamDriveCapabilities::~TeamDriveCapabilities(){}

// static
void TeamDriveCapabilities::RegisterJSONConverter(
    base::JSONValueConverter<TeamDriveCapabilities>* converter) {
  converter->RegisterBoolField(kCanAddChildren,
                               &TeamDriveCapabilities::can_add_children_);
  converter->RegisterBoolField(kCanComment,
                               &TeamDriveCapabilities::can_comment_);
  converter->RegisterBoolField(kCanCopy, &TeamDriveCapabilities::can_copy_);
  converter->RegisterBoolField(kCanDeleteTeamDrive,
                               &TeamDriveCapabilities::can_delete_team_drive_);
  converter->RegisterBoolField(kCanDownload,
                               &TeamDriveCapabilities::can_download_);
  converter->RegisterBoolField(kCanEdit, &TeamDriveCapabilities::can_edit_);
  converter->RegisterBoolField(kCanListChildren,
                               &TeamDriveCapabilities::can_list_children_);
  converter->RegisterBoolField(kCanManageMembers,
                               &TeamDriveCapabilities::can_manage_members_);
  converter->RegisterBoolField(kCanReadRevisions,
                               &TeamDriveCapabilities::can_read_revisions_);
  converter->RegisterBoolField(kCanRemoveChildren,
                               &TeamDriveCapabilities::can_remove_children_);
  converter->RegisterBoolField(kCanRename, &TeamDriveCapabilities::can_rename_);
  converter->RegisterBoolField(kCanRenameTeamDrive,
                               &TeamDriveCapabilities::can_rename_team_drive_);
  converter->RegisterBoolField(kCanShare, &TeamDriveCapabilities::can_share_);
}

////////////////////////////////////////////////////////////////////////////////
// TeamDriveResource implementation

TeamDriveResource::TeamDriveResource() {}

TeamDriveResource::~TeamDriveResource() {}

// static
std::unique_ptr<TeamDriveResource> TeamDriveResource::CreateFrom(
    const base::Value& value) {
  std::unique_ptr<TeamDriveResource> resource(new TeamDriveResource());
  if (!IsResourceKindExpected(value, kTeamDriveKind) ||
      !resource->Parse(value)) {
    LOG(ERROR) << "Unable to create: Invalid Team Drive resource JSON!";
    return std::unique_ptr<TeamDriveResource>();
  }
  return resource;
}

// static
void TeamDriveResource::RegisterJSONConverter(
    base::JSONValueConverter<TeamDriveResource>* converter) {
  converter->RegisterStringField(kId, &TeamDriveResource::id_);
  converter->RegisterStringField(kName, &TeamDriveResource::name_);
  converter->RegisterNestedField(kCapabilities,
                                 &TeamDriveResource::capabilities_);
}

bool TeamDriveResource::Parse(const base::Value& value) {
  base::JSONValueConverter<TeamDriveResource> converter;
  if (!converter.Convert(value, this)) {
    LOG(ERROR) << "Unable to parse: Invalid Team Drive resource JSON!";
    return false;
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// TeamDriveList implementation

TeamDriveList::TeamDriveList() {}

TeamDriveList::~TeamDriveList() {}

// static
void TeamDriveList::RegisterJSONConverter(
    base::JSONValueConverter<TeamDriveList>* converter) {
  converter->RegisterStringField(kNextPageToken,
                                 &TeamDriveList::next_page_token_);
  converter->RegisterRepeatedMessage<TeamDriveResource>(kItems,
                                                        &TeamDriveList::items_);
}

// static
bool TeamDriveList::HasTeamDriveListKind(const base::Value& value) {
  return IsResourceKindExpected(value, kTeamDriveListKind);
}

// static
std::unique_ptr<TeamDriveList> TeamDriveList::CreateFrom(
    const base::Value& value) {
  std::unique_ptr<TeamDriveList> resource(new TeamDriveList());
  if (!HasTeamDriveListKind(value) || !resource->Parse(value)) {
    LOG(ERROR) << "Unable to create: Invalid TeamDriveList JSON!";
    return std::unique_ptr<TeamDriveList>();
  }
  return resource;
}

bool TeamDriveList::Parse(const base::Value& value) {
  base::JSONValueConverter<TeamDriveList> converter;
  if (!converter.Convert(value, this)) {
    LOG(ERROR) << "Unable to parse: Invalid TeamDriveList";
    return false;
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// ParentReference implementation

ParentReference::ParentReference() {}

ParentReference::~ParentReference() {}

// static
void ParentReference::RegisterJSONConverter(
    base::JSONValueConverter<ParentReference>* converter) {
  converter->RegisterStringField(kId, &ParentReference::file_id_);
}

// static
std::unique_ptr<ParentReference> ParentReference::CreateFrom(
    const base::Value& value) {
  std::unique_ptr<ParentReference> reference(new ParentReference());
  if (!IsResourceKindExpected(value, kParentReferenceKind) ||
      !reference->Parse(value)) {
    LOG(ERROR) << "Unable to create: Invalid ParentRefernce JSON!";
    return std::unique_ptr<ParentReference>();
  }
  return reference;
}

bool ParentReference::Parse(const base::Value& value) {
  base::JSONValueConverter<ParentReference> converter;
  if (!converter.Convert(value, this)) {
    LOG(ERROR) << "Unable to parse: Invalid ParentReference";
    return false;
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// FileResource implementation

FileResource::FileResource() : shared_(false), file_size_(kUnsetFileSize) {}

FileResource::FileResource(const FileResource& other) = default;

FileResource::~FileResource() {}

// static
void FileResource::RegisterJSONConverter(
    base::JSONValueConverter<FileResource>* converter) {
  converter->RegisterStringField(kId, &FileResource::file_id_);
  converter->RegisterStringField(kETag, &FileResource::etag_);
  converter->RegisterStringField(kTitle, &FileResource::title_);
  converter->RegisterStringField(kMimeType, &FileResource::mime_type_);
  converter->RegisterNestedField(kLabels, &FileResource::labels_);
  converter->RegisterNestedField(kImageMediaMetadata,
                                 &FileResource::image_media_metadata_);
  converter->RegisterCustomField<base::Time>(
      kCreatedDate,
      &FileResource::created_date_,
      &util::GetTimeFromString);
  converter->RegisterCustomField<base::Time>(
      kModifiedDate,
      &FileResource::modified_date_,
      &util::GetTimeFromString);
  converter->RegisterCustomField<base::Time>(
      kModifiedByMeDate, &FileResource::modified_by_me_date_,
      &util::GetTimeFromString);
  converter->RegisterCustomField<base::Time>(
      kLastViewedByMeDate,
      &FileResource::last_viewed_by_me_date_,
      &util::GetTimeFromString);
  converter->RegisterCustomField<base::Time>(
      kSharedWithMeDate,
      &FileResource::shared_with_me_date_,
      &util::GetTimeFromString);
  converter->RegisterBoolField(kShared, &FileResource::shared_);
  converter->RegisterStringField(kMd5Checksum, &FileResource::md5_checksum_);
  converter->RegisterCustomField<int64_t>(kFileSize, &FileResource::file_size_,
                                          &base::StringToInt64);
  converter->RegisterCustomField<GURL>(kAlternateLink,
                                       &FileResource::alternate_link_,
                                       GetGURLFromString);
  converter->RegisterCustomField<GURL>(kShareLink,
                                       &FileResource::share_link_,
                                       GetGURLFromString);
  converter->RegisterCustomValueField<std::vector<ParentReference> >(
      kParents,
      &FileResource::parents_,
      GetParentsFromValue);
  converter->RegisterCustomValueField<std::vector<OpenWithLink> >(
      kOpenWithLinks,
      &FileResource::open_with_links_,
      GetOpenWithLinksFromDictionaryValue);
}

// static
std::unique_ptr<FileResource> FileResource::CreateFrom(
    const base::Value& value) {
  std::unique_ptr<FileResource> resource(new FileResource());
  if (!IsResourceKindExpected(value, kFileKind) || !resource->Parse(value)) {
    LOG(ERROR) << "Unable to create: Invalid FileResource JSON!";
    return std::unique_ptr<FileResource>();
  }
  return resource;
}

bool FileResource::IsDirectory() const {
  return mime_type_ == kDriveFolderMimeType;
}

bool FileResource::IsHostedDocument() const {
  // Hosted documents don't have fileSize field set:
  // https://developers.google.com/drive/v2/reference/files
  return !IsDirectory() && file_size_ == kUnsetFileSize;
}

bool FileResource::Parse(const base::Value& value) {
  base::JSONValueConverter<FileResource> converter;
  if (!converter.Convert(value, this)) {
    LOG(ERROR) << "Unable to parse: Invalid FileResource";
    return false;
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// FileList implementation

FileList::FileList() {}

FileList::~FileList() {}

// static
void FileList::RegisterJSONConverter(
    base::JSONValueConverter<FileList>* converter) {
  converter->RegisterCustomField<GURL>(kNextLink,
                                       &FileList::next_link_,
                                       GetGURLFromString);
  converter->RegisterRepeatedMessage<FileResource>(kItems,
                                                   &FileList::items_);
}

// static
bool FileList::HasFileListKind(const base::Value& value) {
  return IsResourceKindExpected(value, kFileListKind);
}

// static
std::unique_ptr<FileList> FileList::CreateFrom(const base::Value& value) {
  std::unique_ptr<FileList> resource(new FileList());
  if (!HasFileListKind(value) || !resource->Parse(value)) {
    LOG(ERROR) << "Unable to create: Invalid FileList JSON!";
    return std::unique_ptr<FileList>();
  }
  return resource;
}

bool FileList::Parse(const base::Value& value) {
  base::JSONValueConverter<FileList> converter;
  if (!converter.Convert(value, this)) {
    LOG(ERROR) << "Unable to parse: Invalid FileList";
    return false;
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// ChangeResource implementation

ChangeResource::ChangeResource()
    : change_id_(0), type_(UNKNOWN), deleted_(false) {}

ChangeResource::~ChangeResource() {}

// static
void ChangeResource::RegisterJSONConverter(
    base::JSONValueConverter<ChangeResource>* converter) {
  converter->RegisterCustomField<int64_t>(kId, &ChangeResource::change_id_,
                                          &base::StringToInt64);
  converter->RegisterCustomField<ChangeType>(kType, &ChangeResource::type_,
                                             &ChangeResource::GetType);
  converter->RegisterStringField(kFileId, &ChangeResource::file_id_);
  converter->RegisterBoolField(kDeleted, &ChangeResource::deleted_);
  converter->RegisterCustomValueField(kFile, &ChangeResource::file_,
                                      &CreateFileResourceFromValue);
  converter->RegisterCustomField<base::Time>(
      kModificationDate, &ChangeResource::modification_date_,
      &util::GetTimeFromString);
  converter->RegisterStringField(kTeamDriveId, &ChangeResource::team_drive_id_);
  converter->RegisterCustomValueField(kTeamDrive, &ChangeResource::team_drive_,
                                      &CreateTeamDriveResourceFromValue);
}

// static
std::unique_ptr<ChangeResource> ChangeResource::CreateFrom(
    const base::Value& value) {
  std::unique_ptr<ChangeResource> resource(new ChangeResource());
  if (!IsResourceKindExpected(value, kChangeKind) || !resource->Parse(value)) {
    LOG(ERROR) << "Unable to create: Invalid ChangeResource JSON!";
    return std::unique_ptr<ChangeResource>();
  }
  return resource;
}

bool ChangeResource::Parse(const base::Value& value) {
  base::JSONValueConverter<ChangeResource> converter;
  if (!converter.Convert(value, this)) {
    LOG(ERROR) << "Unable to parse: Invalid ChangeResource";
    return false;
  }
  return true;
}

// static
bool ChangeResource::GetType(const base::StringPiece& type_name,
                             ChangeResource::ChangeType* result) {
  for (size_t i = 0; i < arraysize(kChangeTypeMap); i++) {
    if (type_name == kChangeTypeMap[i].type_name) {
      *result = kChangeTypeMap[i].type;
      return true;
    }
  }
  DVLOG(1) << "Unknown change type" << type_name;
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// ChangeList implementation

ChangeList::ChangeList() : largest_change_id_(0) {}

ChangeList::~ChangeList() {}

// static
void ChangeList::RegisterJSONConverter(
    base::JSONValueConverter<ChangeList>* converter) {
  converter->RegisterCustomField<GURL>(kNextLink,
                                       &ChangeList::next_link_,
                                       GetGURLFromString);
  converter->RegisterCustomField<int64_t>(
      kLargestChangeId, &ChangeList::largest_change_id_, &base::StringToInt64);
  converter->RegisterRepeatedMessage<ChangeResource>(kItems,
                                                     &ChangeList::items_);
}

// static
bool ChangeList::HasChangeListKind(const base::Value& value) {
  return IsResourceKindExpected(value, kChangeListKind);
}

// static
std::unique_ptr<ChangeList> ChangeList::CreateFrom(const base::Value& value) {
  std::unique_ptr<ChangeList> resource(new ChangeList());
  if (!HasChangeListKind(value) || !resource->Parse(value)) {
    LOG(ERROR) << "Unable to create: Invalid ChangeList JSON!";
    return std::unique_ptr<ChangeList>();
  }
  return resource;
}

bool ChangeList::Parse(const base::Value& value) {
  base::JSONValueConverter<ChangeList> converter;
  if (!converter.Convert(value, this)) {
    LOG(ERROR) << "Unable to parse: Invalid ChangeList";
    return false;
  }
  return true;
}


////////////////////////////////////////////////////////////////////////////////
// FileLabels implementation

FileLabels::FileLabels()
    : trashed_(false),
      starred_(false) {}

FileLabels::~FileLabels() {}

// static
void FileLabels::RegisterJSONConverter(
    base::JSONValueConverter<FileLabels>* converter) {
  converter->RegisterBoolField(kLabelTrashed, &FileLabels::trashed_);
  converter->RegisterBoolField(kLabelStarred, &FileLabels::starred_);
}

// static
std::unique_ptr<FileLabels> FileLabels::CreateFrom(const base::Value& value) {
  std::unique_ptr<FileLabels> resource(new FileLabels());
  if (!resource->Parse(value)) {
    LOG(ERROR) << "Unable to create: Invalid FileLabels JSON!";
    return std::unique_ptr<FileLabels>();
  }
  return resource;
}

bool FileLabels::Parse(const base::Value& value) {
  base::JSONValueConverter<FileLabels> converter;
  if (!converter.Convert(value, this)) {
    LOG(ERROR) << "Unable to parse: Invalid FileLabels.";
    return false;
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// ImageMediaMetadata implementation

ImageMediaMetadata::ImageMediaMetadata()
    : width_(-1),
      height_(-1),
      rotation_(-1) {}

ImageMediaMetadata::~ImageMediaMetadata() {}

// static
void ImageMediaMetadata::RegisterJSONConverter(
    base::JSONValueConverter<ImageMediaMetadata>* converter) {
  converter->RegisterIntField(kImageMediaMetadataWidth,
                              &ImageMediaMetadata::width_);
  converter->RegisterIntField(kImageMediaMetadataHeight,
                              &ImageMediaMetadata::height_);
  converter->RegisterIntField(kImageMediaMetadataRotation,
                              &ImageMediaMetadata::rotation_);
}

// static
std::unique_ptr<ImageMediaMetadata> ImageMediaMetadata::CreateFrom(
    const base::Value& value) {
  std::unique_ptr<ImageMediaMetadata> resource(new ImageMediaMetadata());
  if (!resource->Parse(value)) {
    LOG(ERROR) << "Unable to create: Invalid ImageMediaMetadata JSON!";
    return std::unique_ptr<ImageMediaMetadata>();
  }
  return resource;
}

bool ImageMediaMetadata::Parse(const base::Value& value) {
  base::JSONValueConverter<ImageMediaMetadata> converter;
  if (!converter.Convert(value, this)) {
    LOG(ERROR) << "Unable to parse: Invalid ImageMediaMetadata.";
    return false;
  }
  return true;
}

}  // namespace google_apis
