// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_API_PARSER_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_API_PARSER_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/string_piece.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"

namespace base {
class Value;
template <class StructType>
class JSONValueConverter;

namespace internal {
template <class NestedType>
class RepeatedMessageConverter;
}  // namespace internal
}  // namespace base

// TODO(kochi): Rename to namespace drive. http://crbug.com/136371
namespace gdata {

// About resource represents the account information about the current user.
// https://developers.google.com/drive/v2/reference/about
class AboutResource {
 public:
  ~AboutResource();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<AboutResource>* converter);

  // Creates about resource from parsed JSON.
  static scoped_ptr<AboutResource> CreateFrom(const base::Value& value);

  // Returns root folder ID.
  const std::string& root_folder_id() const { return root_folder_id_; }
  // Returns total number of quta bytes.
  int64 quota_bytes_total() const { return quota_bytes_total_; }
  // Returns the number of quota bytes used.
  int64 quota_bytes_used() const { return quota_bytes_used_; }
  // Returns the largest change ID number.
  int64 largest_change_id() const { return largest_change_id_; }

 private:
  friend class DriveAPIParserTest;
  FRIEND_TEST_ALL_PREFIXES(DriveAPIParserTest, AboutResourceParser);
  AboutResource();

  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  std::string root_folder_id_;
  int64 quota_bytes_total_;
  int64 quota_bytes_used_;
  int64 largest_change_id_;

  DISALLOW_COPY_AND_ASSIGN(AboutResource);
};

// DriveAppIcon represents an icon for Drive Application.
// https://developers.google.com/drive/v2/reference/apps/list
class DriveAppIcon {
 public:
  enum IconCategory {
    UNKNOWN,         // Uninitialized state
    DOCUMENT,        // Document icon for various MIME types
    APPLICATION,     // Application icon for various MIME types
    SHARED_DOCUMENT, // Icon for documents that are shared from other users.
  };

  ~DriveAppIcon();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<DriveAppIcon>* converter);

  // Creates drive app icon instance from parsed JSON.
  static scoped_ptr<DriveAppIcon> CreateFrom(const base::Value& value);

  // Category of the icon.
  IconCategory category() const { return category_; }

  // Size in pixels of one side of the icon (icons are always square).
  const int icon_side_length() const { return icon_side_length_; }

  // Returns URL for this icon.
  const GURL& icon_url() const { return icon_url_; }

 private:
  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  // Extracts the icon category from the given string. Returns false and does
  // not change |result| when |scheme| has an unrecognizable value.
  static bool GetIconCategory(const base::StringPiece& category,
                              IconCategory* result);

  friend class base::internal::RepeatedMessageConverter<DriveAppIcon>;
  friend class AppResource;
  DriveAppIcon();

  IconCategory category_;
  int icon_side_length_;
  GURL icon_url_;

  DISALLOW_COPY_AND_ASSIGN(DriveAppIcon);
};

// AppResource represents a Drive Application.
// https://developers.google.com/drive/v2/reference/apps/list
class AppResource {
 public:
  ~AppResource();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<AppResource>* converter);

  // Creates app resource from parsed JSON.
  static scoped_ptr<AppResource> CreateFrom(const base::Value& value);

  // Returns application ID, which is 12-digit decimals (e.g. "123456780123").
  const std::string& id() const { return id_; }

  // Returns application name.
  const std::string& name() const { return name_; }

  // Returns the name of the type of object this application creates.
  // This can be any string.  TODO(kochi): figure out how to use this value.
  const std::string& object_type() const { return object_type_; }

  // Returns whether this application suuports creating new objects.
  bool supports_create() const { return supports_create_; }

  // Returns whether this application supports importing Google Docs.
  bool supports_import() const { return supports_import_; }

  // Returns whether this application is installed.
  bool is_installed() const { return installed_; }

  // Returns whether this application is authorized to access data on the
  // user's Drive.
  bool is_authorized() const { return authorized_; }

  // Returns the product URL, e.g. at Chrmoe Web Store.
  const GURL& product_url() const { return product_url_; }

  // List of primary mime types supported by this WebApp. Primary status should
  // trigger this WebApp becoming the default handler of file instances that
  // have these mime types.
  const ScopedVector<std::string>& primary_mimetypes() const {
    return primary_mimetypes_;
  }

  // List of secondary mime types supported by this WebApp. Secondary status
  // should make this WebApp show up in "Open with..." pop-up menu of the
  // default action menu for file with matching mime types.
  const ScopedVector<std::string>& secondary_mimetypes() const {
    return secondary_mimetypes_;
  }

  // List of primary file extensions supported by this WebApp. Primary status
  // should trigger this WebApp becoming the default handler of file instances
  // that match these extensions.
  const ScopedVector<std::string>& primary_file_extensions() const {
    return primary_file_extensions_;
  }

  // List of secondary file extensions supported by this WebApp. Secondary
  // status should make this WebApp show up in "Open with..." pop-up menu of the
  // default action menu for file with matching extensions.
  const ScopedVector<std::string>& secondary_file_extensions() const {
    return secondary_file_extensions_;
  }

  // Returns Icons for this application.  An application can have multiple
  // icons for different purpose (application, document, shared document)
  // in several sizes.
  const ScopedVector<DriveAppIcon>& icons() const {
    return icons_;
  }

 private:
  friend class base::internal::RepeatedMessageConverter<AppResource>;
  friend class AppList;
  AppResource();

  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  std::string id_;
  std::string name_;
  std::string object_type_;
  bool supports_create_;
  bool supports_import_;
  bool installed_;
  bool authorized_;
  GURL product_url_;
  ScopedVector<std::string> primary_mimetypes_;
  ScopedVector<std::string> secondary_mimetypes_;
  ScopedVector<std::string> primary_file_extensions_;
  ScopedVector<std::string> secondary_file_extensions_;
  ScopedVector<DriveAppIcon> icons_;

  DISALLOW_COPY_AND_ASSIGN(AppResource);
};

// AppList represents a list of Drive Applications.
// https://developers.google.com/drive/v2/reference/apps/list
class AppList {
 public:
  ~AppList();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<AppList>* converter);

  // Creates app list from parsed JSON.
  static scoped_ptr<AppList> CreateFrom(const base::Value& value);

  // ETag for this resource.
  const std::string& etag() const { return etag_; }

  // Returns a vector of applications.
  const ScopedVector<AppResource>& items() const { return items_; }

 private:
  friend class DriveAPIParserTest;
  FRIEND_TEST_ALL_PREFIXES(DriveAPIParserTest, AppListParser);
  AppList();

  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  std::string etag_;
  ScopedVector<AppResource> items_;

  DISALLOW_COPY_AND_ASSIGN(AppList);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_API_PARSER_H_
