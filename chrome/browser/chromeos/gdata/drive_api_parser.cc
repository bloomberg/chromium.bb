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

// About Resource:
const char kAboutKind[] = "drive#about";
const char kRootFolderId[] = "rootFolderId";
const char kQuotaBytesTotal[] = "quotaBytesTotal";
const char kQuotaBytesUsed[] = "quotaBytesUsed";
const char kLargestChangeId[] = "largestChangeId";

// App Icon:
const char kCategory[] = "category";
const char kSize[] = "size";
const char kIconUrl[] = "iconUrl";

// Apps Resource:
const char kAppKind[] = "drive#app";
const char kId[] = "id";
const char kETag[] = "etag";
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
const char kItems[] = "items";


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
  converter->RegisterStringField(kId, &AppResource::id_);
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

}  // namespace gdata
