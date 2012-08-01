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
  const std::string& application_id() const { return application_id_; }

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

  std::string application_id_;
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

// FileResource reporesents a file or folder metadata in Drive.
// https://developers.google.com/drive/v2/reference/files
class FileResource {
 public:
  ~FileResource();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<FileResource>* converter);
  static scoped_ptr<FileResource> CreateFrom(const base::Value& value);

  // Returns true if this is a directory.
  // Note: "folder" is used elsewhere in this file to match Drive API reference,
  // but outside this file we use "directory" to match HTML5 filesystem API.
  bool IsDirectory() const;

  // Returns file ID.  This is unique in all files in Google Drive.
  const std::string& file_id() const { return file_id_; }

  // Returns ETag for this file.
  const std::string& etag() const { return etag_; }

  // Returns MIME type of this file.
  const std::string& mime_type() const { return mime_type_; }

  // Returns the title of this file.
  const std::string& title() const { return title_; }

  // Returns modification time by the user.
  const base::Time& modified_by_me_date() const { return modified_by_me_date_; }

  // Returns the download URL.
  const GURL& download_url() const { return download_url_; }

  // Returns the extension part of the filename.
  const std::string& file_extension() const { return file_extension_; }

  // Returns MD5 checksum of this file.
  const std::string& md5_checksum() const { return md5_checksum_; }

  // Returns the size of this file in bytes.
  int64 file_size() const { return file_size_; }

 private:
  friend class base::internal::RepeatedMessageConverter<FileResource>;
  friend class FileList;
  FileResource();

  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  std::string file_id_;
  std::string etag_;
  std::string mime_type_;
  std::string title_;
  base::Time modified_by_me_date_;
  GURL download_url_;
  std::string file_extension_;
  std::string md5_checksum_;
  int64 file_size_;

  DISALLOW_COPY_AND_ASSIGN(FileResource);
};

// FileList represents a collection of files and folders.
// https://developers.google.com/drive/v2/reference/files/list
class FileList {
 public:
  ~FileList();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<FileList>* converter);
  static scoped_ptr<FileList> CreateFrom(const base::Value& value);

  // Returns the ETag of the list.
  const std::string& etag() const { return etag_; }

  // Returns the page token for the next page of files, if the list is large
  // to fit in one response.  If this is empty, there is no more file lists.
  const std::string& next_page_token() const { return next_page_token_; }

  // Returns a link to the next page of files.  The URL includes the next page
  // token.
  const GURL& next_link() const { return next_link_; }

  // Returns a set of files in this list.
  const ScopedVector<FileResource>& items() const { return items_; }

 private:
  friend class DriveAPIParserTest;
  FRIEND_TEST_ALL_PREFIXES(DriveAPIParserTest, FileListParser);
  FileList();

  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  std::string etag_;
  std::string next_page_token_;
  GURL next_link_;
  ScopedVector<FileResource> items_;

  DISALLOW_COPY_AND_ASSIGN(FileList);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_API_PARSER_H_
