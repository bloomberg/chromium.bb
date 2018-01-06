// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_DRIVE_DRIVE_API_PARSER_H_
#define GOOGLE_APIS_DRIVE_DRIVE_API_PARSER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace base {
class Value;
template <class StructType>
class JSONValueConverter;

namespace internal {
template <class NestedType>
class RepeatedMessageConverter;
}  // namespace internal
}  // namespace base

namespace google_apis {

// About resource represents the account information about the current user.
// https://developers.google.com/drive/v2/reference/about
class AboutResource {
 public:
  AboutResource();
  ~AboutResource();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<AboutResource>* converter);

  // Creates about resource from parsed JSON.
  static std::unique_ptr<AboutResource> CreateFrom(const base::Value& value);

  // Returns the largest change ID number.
  int64_t largest_change_id() const { return largest_change_id_; }
  // Returns total number of quota bytes.
  int64_t quota_bytes_total() const { return quota_bytes_total_; }
  // Returns the number of quota bytes used.
  int64_t quota_bytes_used_aggregate() const {
    return quota_bytes_used_aggregate_;
  }
  // Returns root folder ID.
  const std::string& root_folder_id() const { return root_folder_id_; }

  void set_largest_change_id(int64_t largest_change_id) {
    largest_change_id_ = largest_change_id;
  }
  void set_quota_bytes_total(int64_t quota_bytes_total) {
    quota_bytes_total_ = quota_bytes_total;
  }
  void set_quota_bytes_used_aggregate(int64_t quota_bytes_used_aggregate) {
    quota_bytes_used_aggregate_ = quota_bytes_used_aggregate;
  }
  void set_root_folder_id(const std::string& root_folder_id) {
    root_folder_id_ = root_folder_id;
  }

 private:
  friend class DriveAPIParserTest;
  FRIEND_TEST_ALL_PREFIXES(DriveAPIParserTest, AboutResourceParser);

  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  int64_t largest_change_id_;
  int64_t quota_bytes_total_;
  int64_t quota_bytes_used_aggregate_;
  std::string root_folder_id_;

  // This class is copyable on purpose.
};

// DriveAppIcon represents an icon for Drive Application.
// https://developers.google.com/drive/v2/reference/apps
class DriveAppIcon {
 public:
  enum IconCategory {
    UNKNOWN,          // Uninitialized state.
    DOCUMENT,         // Icon for a file associated with the app.
    APPLICATION,      // Icon for the application.
    SHARED_DOCUMENT,  // Icon for a shared file associated with the app.
  };

  DriveAppIcon();
  ~DriveAppIcon();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<DriveAppIcon>* converter);

  // Creates drive app icon instance from parsed JSON.
  static std::unique_ptr<DriveAppIcon> CreateFrom(const base::Value& value);

  // Category of the icon.
  IconCategory category() const { return category_; }

  // Size in pixels of one side of the icon (icons are always square).
  int icon_side_length() const { return icon_side_length_; }

  // Returns URL for this icon.
  const GURL& icon_url() const { return icon_url_; }

  void set_category(IconCategory category) {
    category_ = category;
  }
  void set_icon_side_length(int icon_side_length) {
    icon_side_length_ = icon_side_length;
  }
  void set_icon_url(const GURL& icon_url) {
    icon_url_ = icon_url;
  }

 private:
  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  // Extracts the icon category from the given string. Returns false and does
  // not change |result| when |scheme| has an unrecognizable value.
  static bool GetIconCategory(base::StringPiece category, IconCategory* result);

  friend class base::internal::RepeatedMessageConverter<DriveAppIcon>;
  friend class AppResource;

  IconCategory category_;
  int icon_side_length_;
  GURL icon_url_;

  DISALLOW_COPY_AND_ASSIGN(DriveAppIcon);
};

// AppResource represents a Drive Application.
// https://developers.google.com/drive/v2/reference/apps
class AppResource {
 public:
  ~AppResource();
  AppResource();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<AppResource>* converter);

  // Creates app resource from parsed JSON.
  static std::unique_ptr<AppResource> CreateFrom(const base::Value& value);

  // Returns application ID, which is 12-digit decimals (e.g. "123456780123").
  const std::string& application_id() const { return application_id_; }

  // Returns application name.
  const std::string& name() const { return name_; }

  // Returns the name of the type of object this application creates.
  // This is used for displaying in "Create" menu item for this app.
  // If empty, application name is used instead.
  const std::string& object_type() const { return object_type_; }

  // Returns the product ID.
  const std::string& product_id() const { return product_id_; }

  // Returns whether this application supports creating new objects.
  bool supports_create() const { return supports_create_; }

  // Returns whether this application is removable by apps.delete API.
  bool is_removable() const { return removable_; }

  // Returns the create URL, i.e., the URL for opening a new file by the app.
  const GURL& create_url() const { return create_url_; }

  // List of primary mime types supported by this WebApp. Primary status should
  // trigger this WebApp becoming the default handler of file instances that
  // have these mime types.
  const std::vector<std::unique_ptr<std::string>>& primary_mimetypes() const {
    return primary_mimetypes_;
  }

  // List of secondary mime types supported by this WebApp. Secondary status
  // should make this WebApp show up in "Open with..." pop-up menu of the
  // default action menu for file with matching mime types.
  const std::vector<std::unique_ptr<std::string>>& secondary_mimetypes() const {
    return secondary_mimetypes_;
  }

  // List of primary file extensions supported by this WebApp. Primary status
  // should trigger this WebApp becoming the default handler of file instances
  // that match these extensions.
  const std::vector<std::unique_ptr<std::string>>& primary_file_extensions()
      const {
    return primary_file_extensions_;
  }

  // List of secondary file extensions supported by this WebApp. Secondary
  // status should make this WebApp show up in "Open with..." pop-up menu of the
  // default action menu for file with matching extensions.
  const std::vector<std::unique_ptr<std::string>>& secondary_file_extensions()
      const {
    return secondary_file_extensions_;
  }

  // Returns Icons for this application.  An application can have multiple
  // icons for different purpose (application, document, shared document)
  // in several sizes.
  const std::vector<std::unique_ptr<DriveAppIcon>>& icons() const {
    return icons_;
  }

  void set_application_id(const std::string& application_id) {
    application_id_ = application_id;
  }
  void set_name(const std::string& name) { name_ = name; }
  void set_object_type(const std::string& object_type) {
    object_type_ = object_type;
  }
  void set_product_id(const std::string& id) { product_id_ = id; }
  void set_supports_create(bool supports_create) {
    supports_create_ = supports_create;
  }
  void set_removable(bool removable) { removable_ = removable; }
  void set_primary_mimetypes(
      std::vector<std::unique_ptr<std::string>> primary_mimetypes) {
    primary_mimetypes_ = std::move(primary_mimetypes);
  }
  void set_secondary_mimetypes(
      std::vector<std::unique_ptr<std::string>> secondary_mimetypes) {
    secondary_mimetypes_ = std::move(secondary_mimetypes);
  }
  void set_primary_file_extensions(
      std::vector<std::unique_ptr<std::string>> primary_file_extensions) {
    primary_file_extensions_ = std::move(primary_file_extensions);
  }
  void set_secondary_file_extensions(
      std::vector<std::unique_ptr<std::string>> secondary_file_extensions) {
    secondary_file_extensions_ = std::move(secondary_file_extensions);
  }
  void set_icons(std::vector<std::unique_ptr<DriveAppIcon>> icons) {
    icons_ = std::move(icons);
  }
  void set_create_url(const GURL& url) {
    create_url_ = url;
  }

 private:
  friend class base::internal::RepeatedMessageConverter<AppResource>;
  friend class AppList;

  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  std::string application_id_;
  std::string name_;
  std::string object_type_;
  std::string product_id_;
  bool supports_create_;
  bool removable_;
  GURL create_url_;
  std::vector<std::unique_ptr<std::string>> primary_mimetypes_;
  std::vector<std::unique_ptr<std::string>> secondary_mimetypes_;
  std::vector<std::unique_ptr<std::string>> primary_file_extensions_;
  std::vector<std::unique_ptr<std::string>> secondary_file_extensions_;
  std::vector<std::unique_ptr<DriveAppIcon>> icons_;

  DISALLOW_COPY_AND_ASSIGN(AppResource);
};

// AppList represents a list of Drive Applications.
// https://developers.google.com/drive/v2/reference/apps/list
class AppList {
 public:
  AppList();
  ~AppList();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<AppList>* converter);

  // Creates app list from parsed JSON.
  static std::unique_ptr<AppList> CreateFrom(const base::Value& value);

  // ETag for this resource.
  const std::string& etag() const { return etag_; }

  // Returns a vector of applications.
  const std::vector<std::unique_ptr<AppResource>>& items() const {
    return items_;
  }

  void set_etag(const std::string& etag) {
    etag_ = etag;
  }
  void set_items(std::vector<std::unique_ptr<AppResource>> items) {
    items_ = std::move(items);
  }

 private:
  friend class DriveAPIParserTest;
  FRIEND_TEST_ALL_PREFIXES(DriveAPIParserTest, AppListParser);

  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  std::string etag_;
  std::vector<std::unique_ptr<AppResource>> items_;

  DISALLOW_COPY_AND_ASSIGN(AppList);
};

// Capabilities of a Team Drive indicate the permissions granted to the user
// for the Team Drive and items within the Team Drive.
class TeamDriveCapabilities {
 public:
  TeamDriveCapabilities();
  TeamDriveCapabilities(const TeamDriveCapabilities& src);
  ~TeamDriveCapabilities();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<TeamDriveCapabilities>* converter);

  // Creates Team Drive resource from parsed JSON.
  static std::unique_ptr<TeamDriveCapabilities>
      CreateFrom(const base::Value& value);

  // Whether the current user can add children to folders in this Team Drive.
  bool can_add_children() const { return can_add_children_; }
  // Whether the current user can comment on files in this Team Drive.
  bool can_comment() const { return can_comment_; }
  // Whether files in this Team Drive can be copied by the current user.
  bool can_copy() const { return can_copy_; }
  // Whether this Team Drive can be deleted by the current user.
  bool can_delete_team_drive() const { return can_delete_team_drive_; }
  // Whether files in this Team Drive can be edited by the current user.
  bool can_download() const { return can_download_; }
  // Whether files in this Team Drive can be edited by current user.
  bool can_edit() const { return can_edit_; }
  // Whether the current user can list the children of folders in this Team
  // Drive.
  bool can_list_children() const { return can_list_children_; }
  // Whether the current user can add members to this Team Drive or remove them
  // or change their role.
  bool can_manage_members() const { return can_manage_members_; }
  // Whether the current user has read access to the Revisions resource of files
  // in this Team Drive.
  bool can_read_revisions() const { return can_read_revisions_; }
  // Whether the current user can remove children from folders in this Team
  // Drive.
  bool can_remove_children() const { return can_remove_children_; }
  // Whether files or folders in this Team Drive can be renamed by the current
  // user.
  bool can_rename() const { return can_rename_; }
  // Whether this Team Drive can be renamed by the current user.
  bool can_rename_team_drive() const { return can_rename_team_drive_; }
  // Whether files or folders in this Team Drive can be shared by the current
  // user.
  bool can_share() const { return can_share_; }

 private:
  bool can_add_children_;
  bool can_comment_;
  bool can_copy_;
  bool can_delete_team_drive_;
  bool can_download_;
  bool can_edit_;
  bool can_list_children_;
  bool can_manage_members_;
  bool can_read_revisions_;
  bool can_remove_children_;
  bool can_rename_;
  bool can_rename_team_drive_;
  bool can_share_;
};

// Team Drive resource represents the metadata about Team Drive itself, such as
// the name.
class TeamDriveResource {
 public:
  TeamDriveResource();
  ~TeamDriveResource();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<TeamDriveResource>* converter);

  // Creates Team Drive resource from parsed JSON.
  static std::unique_ptr<TeamDriveResource>
      CreateFrom(const base::Value& value);

  // The ID of this Team Drive. The ID is the same as the top-level folder for
  // this Team Drive.
  const std::string& id() const { return id_; }
  void set_id(const std::string& id) { id_ = id; }
  // The name of this Team Drive.
  const std::string& name() const { return name_; }
  void set_name(const std::string& name) { name_ = name; }
  // Capabilities the current user has on this Team Drive.
  const TeamDriveCapabilities& capabilities() const { return capabilities_; }

 private:
  friend class DriveAPIParserTest;
  FRIEND_TEST_ALL_PREFIXES(DriveAPIParserTest, TeamDriveResourceParser);

  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  std::string id_;
  std::string name_;
  TeamDriveCapabilities capabilities_;
};

// TeamDriveList represents a collection of Team Drives.
// https://developers.google.com/drive/v2/reference/teamdrives/list
class TeamDriveList {
 public:
  TeamDriveList();
  ~TeamDriveList();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<TeamDriveList>* converter);

  // Returns true if the |value| has kind field for TeamDriveList.
  static bool HasTeamDriveListKind(const base::Value& value);

  // Creates file list from parsed JSON.
  static std::unique_ptr<TeamDriveList> CreateFrom(const base::Value& value);

  // Returns a page token for the next page of Team Drives.
  const std::string& next_page_token() const { return next_page_token_; }

  void set_next_page_token(const std::string& next_page_token) {
    this->next_page_token_ = next_page_token;
  }

  // Returns a set of Team Drives in this list.
  const std::vector<std::unique_ptr<TeamDriveResource>>& items() const {
    return items_;
  }

  std::vector<std::unique_ptr<TeamDriveResource>>* mutable_items() {
    return &items_;
  }

 private:
  friend class DriveAPIParserTest;
  FRIEND_TEST_ALL_PREFIXES(DriveAPIParserTest, TeamDriveListParser);

  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  std::string next_page_token_;
  std::vector<std::unique_ptr<TeamDriveResource>> items_;

  DISALLOW_COPY_AND_ASSIGN(TeamDriveList);
};

// ParentReference represents a directory.
// https://developers.google.com/drive/v2/reference/parents
class ParentReference {
 public:
  ParentReference();
  ~ParentReference();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<ParentReference>* converter);

  // Creates parent reference from parsed JSON.
  static std::unique_ptr<ParentReference> CreateFrom(const base::Value& value);

  // Returns the file id of the reference.
  const std::string& file_id() const { return file_id_; }

  void set_file_id(const std::string& file_id) { file_id_ = file_id; }

 private:
  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  std::string file_id_;
};

// FileLabels represents labels for file or folder.
// https://developers.google.com/drive/v2/reference/files
class FileLabels {
 public:
  FileLabels();
  ~FileLabels();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<FileLabels>* converter);

  // Creates about resource from parsed JSON.
  static std::unique_ptr<FileLabels> CreateFrom(const base::Value& value);

  // Whether this file has been trashed.
  bool is_trashed() const { return trashed_; }
  // Whether this file is starred by the user.
  bool is_starred() const { return starred_; }

  void set_trashed(bool trashed) { trashed_ = trashed; }
  void set_starred(bool starred) { starred_ = starred; }

 private:
  friend class FileResource;

  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  bool trashed_;
  bool starred_;
};

// ImageMediaMetadata represents image metadata for a file.
// https://developers.google.com/drive/v2/reference/files
class ImageMediaMetadata {
 public:
  ImageMediaMetadata();
  ~ImageMediaMetadata();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<ImageMediaMetadata>* converter);

  // Creates about resource from parsed JSON.
  static std::unique_ptr<ImageMediaMetadata> CreateFrom(
      const base::Value& value);

  // Width of the image in pixels.
  int width() const { return width_; }
  // Height of the image in pixels.
  int height() const { return height_; }
  // Rotation of the image in clockwise degrees.
  int rotation() const { return rotation_; }

  void set_width(int width) { width_ = width; }
  void set_height(int height) { height_ = height; }
  void set_rotation(int rotation) { rotation_ = rotation; }

 private:
  friend class FileResource;

  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  int width_;
  int height_;
  int rotation_;
};

// FileResource represents a file or folder metadata in Drive.
// https://developers.google.com/drive/v2/reference/files
class FileResource {
 public:
  // Link to open a file resource on a web app with |app_id|.
  struct OpenWithLink {
    std::string app_id;
    GURL open_url;
  };

  FileResource();
  FileResource(const FileResource& other);
  ~FileResource();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<FileResource>* converter);

  // Creates file resource from parsed JSON.
  static std::unique_ptr<FileResource> CreateFrom(const base::Value& value);

  // Returns true if this is a directory.
  // Note: "folder" is used elsewhere in this file to match Drive API reference,
  // but outside this file we use "directory" to match HTML5 filesystem API.
  bool IsDirectory() const;

  // Returns true if this is a hosted document.
  // A hosted document is a document in one of Google Docs formats (Documents,
  // Spreadsheets, Slides, ...) whose content is not exposed via the API. It is
  // available only as |alternate_link()| to the document hosted on the server.
  bool IsHostedDocument() const;

  // Returns file ID.  This is unique in all files in Google Drive.
  const std::string& file_id() const { return file_id_; }

  // Returns ETag for this file.
  const std::string& etag() const { return etag_; }

  // Returns the title of this file.
  const std::string& title() const { return title_; }

  // Returns MIME type of this file.
  const std::string& mime_type() const { return mime_type_; }

  // Returns labels for this file.
  const FileLabels& labels() const { return labels_; }

  // Returns image media metadata for this file.
  const ImageMediaMetadata& image_media_metadata() const {
    return image_media_metadata_;
  }

  // Returns created time of this file.
  const base::Time& created_date() const { return created_date_; }

  // Returns modified time of this file.
  const base::Time& modified_date() const { return modified_date_; }

  // Returns last modified time by the user.
  const base::Time& modified_by_me_date() const { return modified_by_me_date_; }

  // Returns last access time by the user.
  const base::Time& last_viewed_by_me_date() const {
    return last_viewed_by_me_date_;
  }

  // Returns time when the file was shared with the user.
  const base::Time& shared_with_me_date() const {
    return shared_with_me_date_;
  }

  // Returns the 'shared' attribute of the file.
  bool shared() const { return shared_; }

  // Returns MD5 checksum of this file.
  const std::string& md5_checksum() const { return md5_checksum_; }

  // Returns the size of this file in bytes.
  int64_t file_size() const { return file_size_; }

  // Return the link to open the file in Google editor or viewer.
  // E.g. Google Document, Google Spreadsheet.
  const GURL& alternate_link() const { return alternate_link_; }

  // Returns URL to the share dialog UI.
  const GURL& share_link() const { return share_link_; }

  // Returns parent references (directories) of this file.
  const std::vector<ParentReference>& parents() const { return parents_; }

  // Returns the list of links to open the resource with a web app.
  const std::vector<OpenWithLink>& open_with_links() const {
    return open_with_links_;
  }

  void set_file_id(const std::string& file_id) {
    file_id_ = file_id;
  }
  void set_etag(const std::string& etag) {
    etag_ = etag;
  }
  void set_title(const std::string& title) {
    title_ = title;
  }
  void set_mime_type(const std::string& mime_type) {
    mime_type_ = mime_type;
  }
  FileLabels* mutable_labels() {
    return &labels_;
  }
  ImageMediaMetadata* mutable_image_media_metadata() {
    return &image_media_metadata_;
  }
  void set_created_date(const base::Time& created_date) {
    created_date_ = created_date;
  }
  void set_modified_date(const base::Time& modified_date) {
    modified_date_ = modified_date;
  }
  void set_modified_by_me_date(const base::Time& modified_by_me_date) {
    modified_by_me_date_ = modified_by_me_date;
  }
  void set_last_viewed_by_me_date(const base::Time& last_viewed_by_me_date) {
    last_viewed_by_me_date_ = last_viewed_by_me_date;
  }
  void set_shared_with_me_date(const base::Time& shared_with_me_date) {
    shared_with_me_date_ = shared_with_me_date;
  }
  void set_shared(bool shared) {
    shared_ = shared;
  }
  void set_md5_checksum(const std::string& md5_checksum) {
    md5_checksum_ = md5_checksum;
  }
  void set_file_size(int64_t file_size) { file_size_ = file_size; }
  void set_alternate_link(const GURL& alternate_link) {
    alternate_link_ = alternate_link;
  }
  void set_share_link(const GURL& share_link) {
    share_link_ = share_link;
  }
  std::vector<ParentReference>* mutable_parents() { return &parents_; }
  std::vector<OpenWithLink>* mutable_open_with_links() {
    return &open_with_links_;
  }

 private:
  friend class base::internal::RepeatedMessageConverter<FileResource>;
  friend class ChangeResource;
  friend class FileList;

  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  std::string file_id_;
  std::string etag_;
  std::string title_;
  std::string mime_type_;
  FileLabels labels_;
  ImageMediaMetadata image_media_metadata_;
  base::Time created_date_;
  base::Time modified_date_;
  base::Time modified_by_me_date_;
  base::Time last_viewed_by_me_date_;
  base::Time shared_with_me_date_;
  bool shared_;
  std::string md5_checksum_;
  int64_t file_size_;
  GURL alternate_link_;
  GURL share_link_;
  std::vector<ParentReference> parents_;
  std::vector<OpenWithLink> open_with_links_;
};

// FileList represents a collection of files and folders.
// https://developers.google.com/drive/v2/reference/files/list
class FileList {
 public:
  FileList();
  ~FileList();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<FileList>* converter);

  // Returns true if the |value| has kind field for FileList.
  static bool HasFileListKind(const base::Value& value);

  // Creates file list from parsed JSON.
  static std::unique_ptr<FileList> CreateFrom(const base::Value& value);

  // Returns a link to the next page of files.  The URL includes the next page
  // token.
  const GURL& next_link() const { return next_link_; }

  // Returns a set of files in this list.
  const std::vector<std::unique_ptr<FileResource>>& items() const {
    return items_;
  }
  std::vector<std::unique_ptr<FileResource>>* mutable_items() {
    return &items_;
  }

  void set_next_link(const GURL& next_link) {
    next_link_ = next_link;
  }

 private:
  friend class DriveAPIParserTest;
  FRIEND_TEST_ALL_PREFIXES(DriveAPIParserTest, FileListParser);

  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  GURL next_link_;
  std::vector<std::unique_ptr<FileResource>> items_;

  DISALLOW_COPY_AND_ASSIGN(FileList);
};

// ChangeResource represents a change in a file.
// https://developers.google.com/drive/v2/reference/changes
class ChangeResource {
 public:
  enum ChangeType {
    UNKNOWN,  // Uninitialized state.
    FILE,
    TEAM_DRIVE,
  };
  ChangeResource();
  ~ChangeResource();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<ChangeResource>* converter);

  // Creates change resource from parsed JSON.
  static std::unique_ptr<ChangeResource> CreateFrom(const base::Value& value);

  // Returns change ID for this change.  This is a monotonically increasing
  // number.
  int64_t change_id() const { return change_id_; }

  // Returns whether this is a change of a file or a team drive.
  ChangeType type() const { return type_; }

  // Returns a string file ID for corresponding file of the change.
  // Valid only when type == FILE.
  const std::string& file_id() const {
    DCHECK_EQ(FILE, type_);
    return file_id_;
  }

  // Returns true if this file is deleted in the change.
  bool is_deleted() const { return deleted_; }

  // Returns FileResource of the file which the change refers to.
  // Valid only when type == FILE.
  const FileResource* file() const {
    DCHECK_EQ(FILE, type_);
    return file_.get();
  }
  FileResource* mutable_file() {
    DCHECK_EQ(FILE, type_);
    return file_.get();
  }

  // Returns TeamDriveResource which the change refers to.
  // Valid only when type == TEAM_DRIVE.
  const TeamDriveResource* team_drive() const {
    DCHECK_EQ(TEAM_DRIVE, type_);
    return team_drive_.get();
  }
  TeamDriveResource* mutable_team_drive() {
    DCHECK_EQ(TEAM_DRIVE, type_);
    return team_drive_.get();
  }

  // Returns the ID of the Team Drive. Valid only when type == TEAM_DRIVE.
  const std::string& team_drive_id() const {
    DCHECK_EQ(TEAM_DRIVE, type_);
    return team_drive_id_;
  }

  // Returns the time of this modification.
  const base::Time& modification_date() const { return modification_date_; }

  void set_change_id(int64_t change_id) { change_id_ = change_id; }
  void set_type(ChangeType type) { type_ = type; }
  void set_file_id(const std::string& file_id) {
    file_id_ = file_id;
  }
  void set_deleted(bool deleted) {
    deleted_ = deleted;
  }
  void set_file(std::unique_ptr<FileResource> file) { file_ = std::move(file); }
  void set_team_drive(std::unique_ptr<TeamDriveResource> team_drive) {
    team_drive_ = std::move(team_drive);
  }
  void set_team_drive_id(const std::string& team_drive_id) {
    team_drive_id_ = team_drive_id;
  }
  void set_modification_date(const base::Time& modification_date) {
    modification_date_ = modification_date;
  }

 private:
  friend class base::internal::RepeatedMessageConverter<ChangeResource>;
  friend class ChangeList;

  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  // Extracts the change type from the given string. Returns false and does
  // not change |result| when |type_name| has an unrecognizable value.
  static bool GetType(base::StringPiece type_name,
                      ChangeResource::ChangeType* result);

  int64_t change_id_;
  ChangeType type_;
  std::string file_id_;
  bool deleted_;
  std::unique_ptr<FileResource> file_;
  base::Time modification_date_;
  std::string team_drive_id_;
  std::unique_ptr<TeamDriveResource> team_drive_;

  DISALLOW_COPY_AND_ASSIGN(ChangeResource);
};

// ChangeList represents a set of changes in the drive.
// https://developers.google.com/drive/v2/reference/changes/list
class ChangeList {
 public:
  ChangeList();
  ~ChangeList();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<ChangeList>* converter);

  // Returns true if the |value| has kind field for ChangeList.
  static bool HasChangeListKind(const base::Value& value);

  // Creates change list from parsed JSON.
  static std::unique_ptr<ChangeList> CreateFrom(const base::Value& value);

  // Returns a link to the next page of files.  The URL includes the next page
  // token.
  const GURL& next_link() const { return next_link_; }

  // Returns the largest change ID number.
  int64_t largest_change_id() const { return largest_change_id_; }

  // Returns a set of changes in this list.
  const std::vector<std::unique_ptr<ChangeResource>>& items() const {
    return items_;
  }
  std::vector<std::unique_ptr<ChangeResource>>* mutable_items() {
    return &items_;
  }

  void set_next_link(const GURL& next_link) {
    next_link_ = next_link;
  }
  void set_largest_change_id(int64_t largest_change_id) {
    largest_change_id_ = largest_change_id;
  }

 private:
  friend class DriveAPIParserTest;
  FRIEND_TEST_ALL_PREFIXES(DriveAPIParserTest, ChangeListParser);

  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  GURL next_link_;
  int64_t largest_change_id_;
  std::vector<std::unique_ptr<ChangeResource>> items_;

  DISALLOW_COPY_AND_ASSIGN(ChangeList);
};

}  // namespace google_apis

#endif  // GOOGLE_APIS_DRIVE_DRIVE_API_PARSER_H_
