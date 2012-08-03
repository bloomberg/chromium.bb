// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/drive_api_parser.h"

#include <algorithm>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/json/json_value_converter.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"

using base::Value;
using base::DictionaryValue;
using base::ListValue;

namespace {

// Converts |url_string| to |result|.  Always returns true to be used
// for JSONValueConverter::RegisterCustomField method.
// TODO(mukai): make it return false in case of invalid |url_string|.
bool GetGURLFromString(const base::StringPiece& url_string, GURL* result) {
  *result = GURL(url_string.as_string());
  return true;
}

// Drive v2 API JSON names.

// Common
const char kKind[] = "kind";
const char kId[] = "id";
const char kETag[] = "etag";
const char kItems[] = "items";
const char kLargestChangeId[] = "largestChangeId";

// About Resource:
const char kAboutKind[] = "drive#about";
const char kRootFolderId[] = "rootFolderId";
const char kQuotaBytesTotal[] = "quotaBytesTotal";
const char kQuotaBytesUsed[] = "quotaBytesUsed";

// App Icon:
const char kCategory[] = "category";
const char kSize[] = "size";
const char kIconUrl[] = "iconUrl";

// Apps Resource:
const char kAppKind[] = "drive#app";
const char kName[] = "name";
const char kObjectType[] = "objectType";
const char kSupportsCreate[] = "supportsCreate";
const char kSupportsImport[] = "supportsImport";
const char kInstalled[] = "installed";
const char kAuthorized[] = "authorized";
const char kProductUrl[] = "productUrl";
const char kPrimaryMimeTypes[] = "primaryMimeTypes";
const char kSecondaryMimeTypes[] = "secondaryMimeTypes";
const char kPrimaryFileExtensions[] = "primaryFileExtensions";
const char kSecondaryFileExtensions[] = "secondaryFileExtensions";
const char kIcons[] = "icons";

// Apps List:
const char kAppListKind[] = "drive#appList";

// Parent Resource:
const char kParentReferenceKind[] = "drive#parentReference";
const char kIsRoot[] = "isRoot";

// File Resource:
const char kFileKind[] = "drive#file";
const char kMimeType[] = "mimeType";
const char kTitle[] = "title";
const char kModifiedByMeDate[] = "modifiedByMeDate";
const char kParents[] = "parents";
const char kDownloadUrl[] = "downloadUrl";
const char kFileExtension[] = "fileExtension";
const char kMd5Checksum[] = "md5Checksum";
const char kFileSize[] = "fileSize";

const char kDriveFolderMimeType[] = "application/vnd.google-apps.folder";

// Files List:
const char kFileListKind[] = "drive#fileList";
const char kNextPageToken[] = "nextPageToken";
const char kNextLink[] = "nextLink";

// Change Resource:
const char kChangeKind[] = "drive#change";
const char kFileId[] = "fileId";
const char kFile[] = "file";

// Changes List:
const char kChangeListKind[] = "drive#changeList";

// Maps category name to enum IconCategory.
struct AppIconCategoryMap {
  gdata::DriveAppIcon::IconCategory category;
  const char* category_name;
};

const AppIconCategoryMap kAppIconCategoryMap[] = {
  { gdata::DriveAppIcon::DOCUMENT, "document" },
  { gdata::DriveAppIcon::APPLICATION, "application" },
  { gdata::DriveAppIcon::SHARED_DOCUMENT, "documentShared" },
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

// TODO(kochi): Rename to namespace drive. http://crbug.com/136371
namespace gdata {

////////////////////////////////////////////////////////////////////////////////
// AboutResource implementation

AboutResource::AboutResource()
    : quota_bytes_total_(0),
      quota_bytes_used_(0),
      largest_change_id_(0) {}

AboutResource::~AboutResource() {}

// static
scoped_ptr<AboutResource> AboutResource::CreateFrom(const base::Value& value) {
  scoped_ptr<AboutResource> resource(new AboutResource());
  if (!IsResourceKindExpected(value, kAboutKind) || !resource->Parse(value)) {
    LOG(ERROR) << "Unable to create: Invalid About resource JSON!";
    return scoped_ptr<AboutResource>(NULL);
  }
  return resource.Pass();
}

// static
void AboutResource::RegisterJSONConverter(
    base::JSONValueConverter<AboutResource>* converter) {
  converter->RegisterStringField(kRootFolderId,
                                 &AboutResource::root_folder_id_);
  converter->RegisterCustomField<int64>(kQuotaBytesTotal,
                                        &AboutResource::quota_bytes_total_,
                                        &base::StringToInt64);
  converter->RegisterCustomField<int64>(kQuotaBytesUsed,
                                        &AboutResource::quota_bytes_used_,
                                        &base::StringToInt64);
  converter->RegisterCustomField<int64>(kLargestChangeId,
                                        &AboutResource::largest_change_id_,
                                        &base::StringToInt64);
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

DriveAppIcon::DriveAppIcon() {}

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
scoped_ptr<DriveAppIcon> DriveAppIcon::CreateFrom(const base::Value& value) {
  scoped_ptr<DriveAppIcon> resource(new DriveAppIcon());
  if (!resource->Parse(value)) {
    LOG(ERROR) << "Unable to create: Invalid DriveAppIcon JSON!";
    return scoped_ptr<DriveAppIcon>(NULL);
  }
  return resource.Pass();
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

AppResource::AppResource() {}

AppResource::~AppResource() {}

// static
void AppResource::RegisterJSONConverter(
    base::JSONValueConverter<AppResource>* converter) {
  converter->RegisterStringField(kId, &AppResource::application_id_);
  converter->RegisterStringField(kName, &AppResource::name_);
  converter->RegisterStringField(kObjectType, &AppResource::object_type_);
  converter->RegisterBoolField(kSupportsCreate, &AppResource::supports_create_);
  converter->RegisterBoolField(kSupportsImport, &AppResource::supports_import_);
  converter->RegisterBoolField(kInstalled, &AppResource::installed_);
  converter->RegisterBoolField(kAuthorized, &AppResource::authorized_);
  converter->RegisterCustomField<GURL>(kProductUrl,
                                       &AppResource::product_url_,
                                       GetGURLFromString);
  converter->RegisterRepeatedString(kPrimaryMimeTypes,
                                    &AppResource::primary_mimetypes_);
  converter->RegisterRepeatedString(kSecondaryMimeTypes,
                                    &AppResource::secondary_mimetypes_);
  converter->RegisterRepeatedString(kPrimaryFileExtensions,
                                    &AppResource::primary_file_extensions_);
  converter->RegisterRepeatedString(kSecondaryFileExtensions,
                                    &AppResource::secondary_file_extensions_);
  converter->RegisterRepeatedMessage(kIcons, &AppResource::icons_);
}

// static
scoped_ptr<AppResource> AppResource::CreateFrom(const base::Value& value) {
  scoped_ptr<AppResource> resource(new AppResource());
  if (!IsResourceKindExpected(value, kAppKind) || !resource->Parse(value)) {
    LOG(ERROR) << "Unable to create: Invalid AppResource JSON!";
    return scoped_ptr<AppResource>(NULL);
  }
  return resource.Pass();
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
scoped_ptr<AppList> AppList::CreateFrom(const base::Value& value) {
  scoped_ptr<AppList> resource(new AppList());
  if (!IsResourceKindExpected(value, kAppListKind) || !resource->Parse(value)) {
    LOG(ERROR) << "Unable to create: Invalid AppList JSON!";
    return scoped_ptr<AppList>(NULL);
  }
  return resource.Pass();
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
// ParentReference implementation

ParentReference::ParentReference() : is_root_(false) {}

ParentReference::~ParentReference() {}

// static
void ParentReference::RegisterJSONConverter(
    base::JSONValueConverter<ParentReference>* converter) {
  converter->RegisterStringField(kId, &ParentReference::file_id_);
  converter->RegisterBoolField(kIsRoot, &ParentReference::is_root_);
}

// static
scoped_ptr<ParentReference>
ParentReference::CreateFrom(const base::Value& value) {
  scoped_ptr<ParentReference> reference(new ParentReference());
  if (!IsResourceKindExpected(value, kParentReferenceKind) ||
      !reference->Parse(value)) {
    LOG(ERROR) << "Unable to create: Invalid ParentRefernce JSON!";
    return scoped_ptr<ParentReference>(NULL);
  }
  return reference.Pass();
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

FileResource::FileResource() : file_size_(0) {}

FileResource::~FileResource() {}

// static
void FileResource::RegisterJSONConverter(
    base::JSONValueConverter<FileResource>* converter) {
  converter->RegisterStringField(kId, &FileResource::file_id_);
  converter->RegisterStringField(kETag, &FileResource::etag_);
  converter->RegisterStringField(kMimeType, &FileResource::mime_type_);
  converter->RegisterStringField(kTitle, &FileResource::title_);
  converter->RegisterCustomField<base::Time>(
      kModifiedByMeDate,
      &FileResource::modified_by_me_date_,
      &gdata::util::GetTimeFromString);
  converter->RegisterRepeatedMessage<ParentReference>(kParents,
                                                      &FileResource::parents_);
  converter->RegisterCustomField<GURL>(kDownloadUrl,
                                       &FileResource::download_url_,
                                       GetGURLFromString);
  converter->RegisterStringField(kFileExtension,
                                 &FileResource::file_extension_);
  converter->RegisterStringField(kMd5Checksum, &FileResource::md5_checksum_);
  converter->RegisterCustomField<int64>(kFileSize,
                                        &FileResource::file_size_,
                                        &base::StringToInt64);
}

// static
scoped_ptr<FileResource> FileResource::CreateFrom(const base::Value& value) {
  scoped_ptr<FileResource> resource(new FileResource());
  if (!IsResourceKindExpected(value, kFileKind) || !resource->Parse(value)) {
    LOG(ERROR) << "Unable to create: Invalid FileResource JSON!";
    return scoped_ptr<FileResource>(NULL);
  }
  return resource.Pass();
}

bool FileResource::IsDirectory() const {
  return mime_type_ == kDriveFolderMimeType;
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
  converter->RegisterStringField(kETag, &FileList::etag_);
  converter->RegisterStringField(kNextPageToken, &FileList::next_page_token_);
  converter->RegisterCustomField<GURL>(kNextLink,
                                       &FileList::next_link_,
                                       GetGURLFromString);
  converter->RegisterRepeatedMessage<FileResource>(kItems,
                                                   &FileList::items_);
}

// static
scoped_ptr<FileList> FileList::CreateFrom(const base::Value& value) {
  scoped_ptr<FileList> resource(new FileList());
  if (!IsResourceKindExpected(value, kFileListKind) ||
      !resource->Parse(value)) {
    LOG(ERROR) << "Unable to create: Invalid FileList JSON!";
    return scoped_ptr<FileList>(NULL);
  }
  return resource.Pass();
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

ChangeResource::ChangeResource() : change_id_(0), deleted_(false) {}

ChangeResource::~ChangeResource() {}

// static
void ChangeResource::RegisterJSONConverter(
    base::JSONValueConverter<ChangeResource>* converter) {
  converter->RegisterCustomField<int64>(kId,
                                        &ChangeResource::change_id_,
                                        &base::StringToInt64);
  converter->RegisterStringField(kFileId, &ChangeResource::file_id_);
  converter->RegisterNestedField(kFile, &ChangeResource::file_);
}

// static
scoped_ptr<ChangeResource>
ChangeResource::CreateFrom(const base::Value& value) {
  scoped_ptr<ChangeResource> resource(new ChangeResource());
  if (!IsResourceKindExpected(value, kChangeKind) || !resource->Parse(value)) {
    LOG(ERROR) << "Unable to create: Invalid ChangeResource JSON!";
    return scoped_ptr<ChangeResource>(NULL);
  }
  return resource.Pass();
}

bool ChangeResource::Parse(const base::Value& value) {
  base::JSONValueConverter<ChangeResource> converter;
  if (!converter.Convert(value, this)) {
    LOG(ERROR) << "Unable to parse: Invalid ChangeResource";
    return false;
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// ChangeList implementation

ChangeList::ChangeList() : largest_change_id_(0) {}

ChangeList::~ChangeList() {}

// static
void ChangeList::RegisterJSONConverter(
    base::JSONValueConverter<ChangeList>* converter) {
  converter->RegisterStringField(kETag, &ChangeList::etag_);
  converter->RegisterStringField(kNextPageToken, &ChangeList::next_page_token_);
  converter->RegisterCustomField<GURL>(kNextLink,
                                       &ChangeList::next_link_,
                                       GetGURLFromString);
  converter->RegisterCustomField<int64>(kLargestChangeId,
                                        &ChangeList::largest_change_id_,
                                        &base::StringToInt64);
  converter->RegisterRepeatedMessage<ChangeResource>(kItems,
                                                   &ChangeList::items_);
}

// static
scoped_ptr<ChangeList> ChangeList::CreateFrom(const base::Value& value) {
  scoped_ptr<ChangeList> resource(new ChangeList());
  if (!IsResourceKindExpected(value, kChangeListKind) ||
      !resource->Parse(value)) {
    LOG(ERROR) << "Unable to create: Invalid ChangeList JSON!";
    return scoped_ptr<ChangeList>(NULL);
  }
  return resource.Pass();
}

bool ChangeList::Parse(const base::Value& value) {
  base::JSONValueConverter<ChangeList> converter;
  if (!converter.Convert(value, this)) {
    LOG(ERROR) << "Unable to parse: Invalid ChangeList";
    return false;
  }
  return true;
}

}  // namespace gdata
