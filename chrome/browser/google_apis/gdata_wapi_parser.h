// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_GDATA_WAPI_PARSER_H_
#define CHROME_BROWSER_GOOGLE_APIS_GDATA_WAPI_PARSER_H_

#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string_piece.h"
#include "base/time.h"
#include "chrome/browser/google_apis/drive_entry_kinds.h"
#include "googleurl/src/gurl.h"

namespace base {
class FilePath;
class DictionaryValue;
class Value;

template <class StructType>
class JSONValueConverter;

namespace internal {
template <class NestedType>
class RepeatedMessageConverter;
}  // namespace internal

}  // namespace base

// Defines data elements of Google Documents API as described in
// http://code.google.com/apis/documents/.
namespace google_apis {

// TODO(kochi): These forward declarations will be unnecessary once
// http://crbug.com/142293 is resolved.
class ChangeList;
class ChangeResource;
class FileList;
class FileResource;

// Defines link (URL) of an entity (document, file, feed...). Each entity could
// have more than one link representing it.
class Link {
 public:
  enum LinkType {
    LINK_UNKNOWN,
    LINK_SELF,
    LINK_NEXT,
    LINK_PARENT,
    LINK_ALTERNATE,
    LINK_EDIT,
    LINK_EDIT_MEDIA,
    LINK_ALT_EDIT_MEDIA,
    LINK_ALT_POST,
    LINK_FEED,
    LINK_POST,
    LINK_BATCH,
    LINK_RESUMABLE_EDIT_MEDIA,
    LINK_RESUMABLE_CREATE_MEDIA,
    LINK_TABLES_FEED,
    LINK_WORKSHEET_FEED,
    LINK_THUMBNAIL,
    LINK_EMBED,
    LINK_PRODUCT,
    LINK_ICON,
    LINK_OPEN_WITH,
    LINK_SHARE,
  };
  Link();
  ~Link();

  // Registers the mapping between JSON field names and the members in
  // this class.
  static void RegisterJSONConverter(base::JSONValueConverter<Link>* converter);

  // Type of the link.
  LinkType type() const { return type_; }

  // URL of the link.
  const GURL& href() const { return href_; }

  // Title of the link.
  const std::string& title() const { return title_; }

  // For OPEN_WITH links, this contains the application ID. For all other link
  // types, it is the empty string.
  const std::string& app_id() const { return app_id_; }

  // Link MIME type.
  const std::string& mime_type() const { return mime_type_; }

  void set_type(LinkType type) { type_ = type; }
  void set_href(const GURL& href) { href_ = href; }
  void set_title(const std::string& title) { title_ = title; }
  void set_app_id(const std::string& app_id) { app_id_ = app_id; }
  void set_mime_type(const std::string& mime_type) { mime_type_ = mime_type; }

 private:
  friend class ResourceEntry;
  // Converts value of link.rel into LinkType. Outputs to |type| and returns
  // true when |rel| has a valid value. Otherwise does nothing and returns
  // false.
  static bool GetLinkType(const base::StringPiece& rel, LinkType* type);

  // Converts value of link.rel to application ID, if there is one embedded in
  // the link.rel field. Outputs to |app_id| and returns true when |rel| has a
  // valid value. Otherwise does nothing and returns false.
  static bool GetAppID(const base::StringPiece& rel, std::string* app_id);

  LinkType type_;
  GURL href_;
  std::string title_;
  std::string app_id_;
  std::string mime_type_;

  DISALLOW_COPY_AND_ASSIGN(Link);
};

// Feed links define links (URLs) to special list of entries (i.e. list of
// previous document revisions).
class ResourceLink {
 public:
  enum ResourceLinkType {
    FEED_LINK_UNKNOWN,
    FEED_LINK_ACL,
    FEED_LINK_REVISIONS,
  };
  ResourceLink();

  // Registers the mapping between JSON field names and the members in
  // this class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<ResourceLink>* converter);

  // MIME type of the feed.
  ResourceLinkType type() const { return type_; }

  // URL of the feed.
  const GURL& href() const { return href_; }

  void set_type(ResourceLinkType type) { type_ = type; }
  void set_href(const GURL& href) { href_ = href; }

 private:
  friend class ResourceEntry;
  // Converts value of gd$feedLink.rel into ResourceLinkType enum.
  // Outputs to |result| and returns true when |rel| has a valid
  // value.  Otherwise does nothing and returns false.
  static bool GetFeedLinkType(
      const base::StringPiece& rel, ResourceLinkType* result);

  ResourceLinkType type_;
  GURL href_;

  DISALLOW_COPY_AND_ASSIGN(ResourceLink);
};

// Author represents an author of an entity.
class Author {
 public:
  Author();

  // Registers the mapping between JSON field names and the members in
  // this class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<Author>* converter);

  // Getters.
  const std::string& name() const { return name_; }
  const std::string& email() const { return email_; }

  void set_name(const std::string& name) { name_ = name; }
  void set_email(const std::string& email) { email_ = email; }

 private:
  friend class ResourceEntry;

  std::string name_;
  std::string email_;

  DISALLOW_COPY_AND_ASSIGN(Author);
};

// Entry category.
class Category {
 public:
  enum CategoryType {
    CATEGORY_UNKNOWN,
    CATEGORY_ITEM,
    CATEGORY_KIND,
    CATEGORY_LABEL,
  };

  Category();

  // Registers the mapping between JSON field names and the members in
  // this class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<Category>* converter);

  // Category label.
  const std::string& label() const { return label_; }

  // Category type.
  CategoryType type() const { return type_; }

  // Category term.
  const std::string& term() const { return term_; }

  void set_label(const std::string& label) { label_ = label; }
  void set_type(CategoryType type) { type_ = type; }
  void set_term(const std::string& term) { term_ = term; }

 private:
  friend class ResourceEntry;
  // Converts category scheme into CategoryType enum. For example,
  // http://schemas.google.com/g/2005#kind => Category::CATEGORY_KIND
  // Returns false and does not change |result| when |scheme| has an
  // unrecognizable value.
  static bool GetCategoryTypeFromScheme(
      const base::StringPiece& scheme, CategoryType* result);

  std::string label_;
  CategoryType type_;
  std::string term_;

  DISALLOW_COPY_AND_ASSIGN(Category);
};

// Content details of a resource: mime-type, url, and so on.
class Content {
 public:
  Content();

  // Registers the mapping between JSON field names and the members in
  // this class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<Content>* converter);

  // The URL to download the file content.
  // Note that the url can expire, so we'll fetch the latest resource
  // entry before starting a download to get the download URL. See also
  // DriveFileSystem::OnGetFileFromCache for details.
  const GURL& url() const { return url_; }
  const std::string& mime_type() const { return mime_type_; }

  void set_url(const GURL& url) { url_ = url; }
  void set_mime_type(const std::string& mime_type) { mime_type_ = mime_type; }

 private:
  friend class ResourceEntry;

  GURL url_;
  std::string mime_type_;
};

// This stores a representation of an application icon as registered with the
// installed applications section of the account metadata feed. There can be
// multiple icons registered for each application, differing in size, category
// and MIME type.
class AppIcon {
 public:
  enum IconCategory {
    ICON_UNKNOWN,          // Uninitialized state
    ICON_DOCUMENT,         // Document icon for various MIME types
    ICON_APPLICATION,      // Application icon for various MIME types
    ICON_SHARED_DOCUMENT,  // Icon for documents that are shared from other
                           // users.
  };

  AppIcon();
  ~AppIcon();

  // Registers the mapping between JSON field names and the members in
  // this class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<AppIcon>* converter);

  // Category of the icon.
  IconCategory category() const { return category_; }

  // Size in pixels of one side of the icon (icons are always square).
  int icon_side_length() const { return icon_side_length_; }

  // Get a list of links available for this AppIcon.
  const ScopedVector<Link>& links() const { return links_; }

  // Get the icon URL from the internal list of links.  Returns the first
  // icon URL found in the list.
  GURL GetIconURL() const;

  void set_category(IconCategory category) { category_ = category; }
  void set_icon_side_length(int icon_side_length) {
    icon_side_length_ = icon_side_length;
  }
  void set_links(ScopedVector<Link>* links) { links_.swap(*links); }

 private:
  // Extracts the icon category from the given string. Returns false and does
  // not change |result| when |scheme| has an unrecognizable value.
  static bool GetIconCategory(const base::StringPiece& category,
                              IconCategory* result);

  IconCategory category_;
  int icon_side_length_;
  ScopedVector<Link> links_;

  DISALLOW_COPY_AND_ASSIGN(AppIcon);
};

// Base class for feed entries. This class defines fields commonly used by
// various feeds.
class CommonMetadata {
 public:
  CommonMetadata();
  virtual ~CommonMetadata();

  // Returns a link of a given |type| for this entry. If not found, it returns
  // NULL.
  const Link* GetLinkByType(Link::LinkType type) const;

  // Entry update time.
  base::Time updated_time() const { return updated_time_; }

  // Entry ETag.
  const std::string& etag() const { return etag_; }

  // List of entry authors.
  const ScopedVector<Author>& authors() const { return authors_; }

  // List of entry links.
  const ScopedVector<Link>& links() const { return links_; }
  ScopedVector<Link>* mutable_links() { return &links_; }

  // List of entry categories.
  const ScopedVector<Category>& categories() const { return categories_; }

  void set_etag(const std::string& etag) { etag_ = etag; }
  void set_authors(ScopedVector<Author>* authors) {
    authors_.swap(*authors);
  }
  void set_links(ScopedVector<Link>* links) {
    links_.swap(*links);
  }
  void set_categories(ScopedVector<Category>* categories) {
    categories_.swap(*categories);
  }
  void set_updated_time(const base::Time& updated_time) {
    updated_time_ = updated_time;
  }

 protected:
  // Registers the mapping between JSON field names and the members in
  // this class.
  template<typename CommonMetadataDescendant>
  static void RegisterJSONConverter(
      base::JSONValueConverter<CommonMetadataDescendant>* converter);

  std::string etag_;
  ScopedVector<Author> authors_;
  ScopedVector<Link> links_;
  ScopedVector<Category> categories_;
  base::Time updated_time_;

  DISALLOW_COPY_AND_ASSIGN(CommonMetadata);
};

// This class represents a resource entry. A resource is a generic term which
// refers to a file and a directory.
class ResourceEntry : public CommonMetadata {
 public:
  ResourceEntry();
  virtual ~ResourceEntry();

  // Extracts "entry" dictionary from the JSON value, and parse the contents,
  // using CreateFrom(). Returns NULL on failure. The input JSON data, coming
  // from the gdata server, looks like:
  //
  // {
  //   "encoding": "UTF-8",
  //   "entry": { ... },   // This function will extract this and parse.
  //   "version": "1.0"
  // }
  //
  // The caller should delete the returned object.
  static scoped_ptr<ResourceEntry> ExtractAndParse(const base::Value& value);

  // Creates resource entry from parsed JSON Value.  You should call
  // this instead of instantiating JSONValueConverter by yourself
  // because this method does some post-process for some fields.  See
  // FillRemainingFields comment and implementation for the details.
  static scoped_ptr<ResourceEntry> CreateFrom(const base::Value& value);

  // Creates resource entry from FileResource.
  // TODO(kochi): This should go away soon. http://crbug.com/142293
  static scoped_ptr<ResourceEntry> CreateFromFileResource(
      const FileResource& file);

  // Creates resource entry from ChangeResource.
  // Todo(Kochi): This should go away soon. http://crbug.com/142293
  static scoped_ptr<ResourceEntry> CreateFromChangeResource(
      const ChangeResource& change);

  // Returns name of entry node.
  static std::string GetEntryNodeName();

  // Registers the mapping between JSON field names and the members in
  // this class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<ResourceEntry>* converter);

  // Sets true to |result| if the field exists.
  // Always returns true even when the field does not exist.
  static bool HasFieldPresent(const base::Value* value, bool* result);

  // Parses |value| as int64 and sets it to |result|. If the field does not
  // exist, sets 0 to |result| as default value.
  // Returns true if |value| is NULL or it is parsed as int64 successfully.
  static bool ParseChangestamp(const base::Value* value, int64* result);

  // Returns true if |file| has one of the hosted document extensions.
  static bool HasHostedDocumentExtension(const base::FilePath& file);

  // The resource ID is used to identify a resource, which looks like:
  // file:d41d8cd98f00b204e9800998ecf8
  const std::string& resource_id() const { return resource_id_; }

  // This is a URL looks like:
  // https://docs.google.com/feeds/id/file%3Ad41d8cd98f00b204e9800998ecf8.
  // The URL is currently not used.
  const std::string& id() const { return id_; }

  DriveEntryKind kind() const { return kind_; }
  const std::string& title() const { return title_; }
  base::Time published_time() const { return published_time_; }
  base::Time last_viewed_time() const { return last_viewed_time_; }
  const std::vector<std::string>& labels() const { return labels_; }

  // The URL to download a file content.
  // Search for 'download_url' in gdata_wapi_operations.h for details.
  const GURL& download_url() const { return content_.url(); }

  const std::string& content_mime_type() const { return content_.mime_type(); }

  // The resource links contain extra links for revisions and access control,
  // etc.  Note that links() contain more basic links like edit URL,
  // alternative URL, etc.
  const ScopedVector<ResourceLink>& resource_links() const {
    return resource_links_;
  }

  // File name (exists only for kinds FILE and PDF).
  const std::string& filename() const { return filename_; }

  // Suggested file name (exists only for kinds FILE and PDF).
  const std::string& suggested_filename() const { return suggested_filename_; }

  // File content MD5 (exists only for kinds FILE and PDF).
  const std::string& file_md5() const { return file_md5_; }

  // File size (exists only for kinds FILE and PDF).
  int64 file_size() const { return file_size_; }

  // True if the file or directory is deleted (applicable to change list only).
  bool deleted() const { return deleted_ || removed_; }

  // Changestamp (exists only for change query results).
  // If not exists, defaults to 0.
  int64 changestamp() const { return changestamp_; }

  // Text version of resource entry kind. Returns an empty string for
  // unknown entry kind.
  std::string GetEntryKindText() const;

  // Returns preferred file extension for hosted documents. If entry is not
  // a hosted document, this call returns an empty string.
  std::string GetHostedDocumentExtension() const;

  // True if resource entry is remotely hosted.
  bool is_hosted_document() const {
    return (ClassifyEntryKind(kind_) & KIND_OF_HOSTED_DOCUMENT) > 0;
  }
  // True if resource entry hosted by Google Documents.
  bool is_google_document() const {
    return (ClassifyEntryKind(kind_) & KIND_OF_GOOGLE_DOCUMENT) > 0;
  }
  // True if resource entry is hosted by an external application.
  bool is_external_document() const {
    return (ClassifyEntryKind(kind_) & KIND_OF_EXTERNAL_DOCUMENT) > 0;
  }
  // True if resource entry is a folder (collection).
  bool is_folder() const {
    return (ClassifyEntryKind(kind_) & KIND_OF_FOLDER) > 0;
  }
  // True if resource entry is regular file.
  bool is_file() const {
    return (ClassifyEntryKind(kind_) & KIND_OF_FILE) > 0;
  }
  // True if resource entry can't be mapped to the file system.
  bool is_special() const {
    return !is_file() && !is_folder() && !is_hosted_document();
  }

  // The following constructs are exposed for unit tests.

  // Classes of EntryKind. Used for ClassifyEntryKind().
  enum EntryKindClass {
    KIND_OF_NONE = 0,
    KIND_OF_HOSTED_DOCUMENT = 1,
    KIND_OF_GOOGLE_DOCUMENT = 1 << 1,
    KIND_OF_EXTERNAL_DOCUMENT = 1 << 2,
    KIND_OF_FOLDER = 1 << 3,
    KIND_OF_FILE = 1 << 4,
  };

  // Classifies the EntryKind. The returned value is a bitmask of
  // EntryKindClass. For example, DOCUMENT is classified as
  // KIND_OF_HOSTED_DOCUMENT and KIND_OF_GOOGLE_DOCUMENT, hence the returned
  // value is KIND_OF_HOSTED_DOCUMENT | KIND_OF_GOOGLE_DOCUMENT.
  static int ClassifyEntryKind(DriveEntryKind kind);

  void set_resource_id(const std::string& resource_id) {
    resource_id_ = resource_id;
  }
  void set_id(const std::string& id) { id_ = id; }
  void set_kind(DriveEntryKind kind) { kind_ = kind; }
  void set_title(const std::string& title) { title_ = title; }
  void set_published_time(const base::Time& published_time) {
    published_time_ = published_time;
  }
  void set_last_viewed_time(const base::Time& last_viewed_time) {
    last_viewed_time_ = last_viewed_time;
  }
  void set_labels(const std::vector<std::string>& labels) {
    labels_ = labels;
  }
  void set_content(const Content& content) {
    content_ = content;
  }
  void set_resource_links(ScopedVector<ResourceLink>* resource_links) {
    resource_links_.swap(*resource_links);
  }
  void set_filename(const std::string& filename) { filename_ = filename; }
  void set_suggested_filename(const std::string& suggested_filename) {
    suggested_filename_ = suggested_filename;
  }
  void set_file_md5(const std::string& file_md5) { file_md5_ = file_md5; }
  void set_file_size(int64 file_size) { file_size_ = file_size; }
  void set_deleted(bool deleted) { deleted_ = deleted; }
  void set_removed(bool removed) { removed_ = removed; }
  void set_changestamp(int64 changestamp) { changestamp_ = changestamp; }

 private:
  friend class base::internal::RepeatedMessageConverter<ResourceEntry>;
  friend class ResourceList;
  friend class ResumeUploadOperation;

  // Fills the remaining fields where JSONValueConverter cannot catch.
  void FillRemainingFields();

  // Converts categories.term into DriveEntryKind enum.
  static DriveEntryKind GetEntryKindFromTerm(const std::string& term);
  // Converts |kind| into its text identifier equivalent.
  static const char* GetEntryKindDescription(DriveEntryKind kind);

  std::string resource_id_;
  std::string id_;
  DriveEntryKind kind_;
  std::string title_;
  base::Time published_time_;
  // Last viewed value may be unreliable. See: crbug.com/152628.
  base::Time last_viewed_time_;
  std::vector<std::string> labels_;
  Content content_;
  ScopedVector<ResourceLink> resource_links_;
  // Optional fields for files only.
  std::string filename_;
  std::string suggested_filename_;
  std::string file_md5_;
  int64 file_size_;
  bool deleted_;
  bool removed_;
  int64 changestamp_;

  DISALLOW_COPY_AND_ASSIGN(ResourceEntry);
};

// This class represents a list of resource entries with some extra metadata
// such as the root upload URL. The feed is paginated and the rest of the
// feed can be fetched by retrieving the remaining parts of the feed from
// URLs provided by GetNextFeedURL() method.
class ResourceList : public CommonMetadata {
 public:
  ResourceList();
  virtual ~ResourceList();

  // Extracts "feed" dictionary from the JSON value, and parse the contents,
  // using CreateFrom(). Returns NULL on failure. The input JSON data, coming
  // from the gdata server, looks like:
  //
  // {
  //   "encoding": "UTF-8",
  //   "feed": { ... },   // This function will extract this and parse.
  //   "version": "1.0"
  // }
  static scoped_ptr<ResourceList> ExtractAndParse(const base::Value& value);

  // Creates feed from parsed JSON Value.  You should call this
  // instead of instantiating JSONValueConverter by yourself because
  // this method does some post-process for some fields.  See
  // FillRemainingFields comment and implementation in ResourceEntry
  // class for the details.
  static scoped_ptr<ResourceList> CreateFrom(const base::Value& value);
  // Variant of CreateFrom() above, creates feed from parsed ChangeList.
  // TODO(kochi): This should go away soon. http://crbug.com/142293
  static scoped_ptr<ResourceList> CreateFromChangeList(
      const ChangeList& changelist);

  // Registers the mapping between JSON field names and the members in
  // this class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<ResourceList>* converter);

  // Returns true and passes|url| of the next feed if the current entry list
  // does not completed this feed.
  bool GetNextFeedURL(GURL* url) const;

  // List of resource entries.
  const ScopedVector<ResourceEntry>& entries() const { return entries_; }
  ScopedVector<ResourceEntry>* mutable_entries() { return &entries_; }

  // Releases entries_ into |entries|. This is a transfer of ownership, so the
  // caller is responsible for deleting the elements of |entries|.
  void ReleaseEntries(std::vector<ResourceEntry*>* entries);

  // Start index of the resource entry list.
  int start_index() const { return start_index_; }

  // Number of items per feed of the resource entry list.
  int items_per_page() const { return items_per_page_; }

  // The largest changestamp. Next time the resource list should be fetched
  // from this changestamp.
  int64 largest_changestamp() const { return largest_changestamp_; }

  // Resource entry list title.
  const std::string& title() { return title_; }

  void set_entries(ScopedVector<ResourceEntry>* entries) {
    entries_.swap(*entries);
  }
  void set_start_index(int start_index) {
    start_index_ = start_index;
  }
  void set_items_per_page(int items_per_page) {
    items_per_page_ = items_per_page;
  }
  void set_title(const std::string& title) {
    title_ = title;
  }
  void set_largest_changestamp(int64 largest_changestamp) {
    largest_changestamp_ = largest_changestamp;
  }

 private:
  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  ScopedVector<ResourceEntry> entries_;
  int start_index_;
  int items_per_page_;
  std::string title_;
  int64 largest_changestamp_;

  DISALLOW_COPY_AND_ASSIGN(ResourceList);
};

// Metadata representing installed Google Drive application.
class InstalledApp {
 public:
  typedef std::vector<std::pair<int, GURL> > IconList;

  InstalledApp();
  virtual ~InstalledApp();

  // WebApp name.
  const std::string& app_name() const { return app_name_; }

  // Drive app id
  const std::string& app_id() const { return app_id_; }

  // Object (file) type name that is generated by this WebApp.
  const std::string& object_type() const { return object_type_; }

  // True if WebApp supports creation of new file instances.
  bool supports_create() const { return supports_create_; }

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
  const ScopedVector<std::string>& primary_extensions() const {
    return primary_extensions_;
  }

  // List of secondary file extensions supported by this WebApp. Secondary
  // status should make this WebApp show up in "Open with..." pop-up menu of the
  // default action menu for file with matching extensions.
  const ScopedVector<std::string>& secondary_extensions() const {
    return secondary_extensions_;
  }

  // List of entry links.
  const ScopedVector<Link>& links() const { return links_; }

  // Returns a list of icons associated with this installed application.
  const ScopedVector<AppIcon>& app_icons() const {
    return app_icons_;
  }

  // Convenience function for getting the icon URLs for a particular |category|
  // of icon. Icons are returned in a sorted list, from smallest to largest.
  IconList GetIconsForCategory(AppIcon::IconCategory category) const;

  // Retrieves product URL from the link collection.
  GURL GetProductUrl() const;

  // Registers the mapping between JSON field names and the members in
  // this class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<InstalledApp>* converter);

  void set_app_id(const std::string& app_id) { app_id_ = app_id; }
  void set_app_name(const std::string& app_name) { app_name_ = app_name; }
  void set_object_type(const std::string& object_type) {
    object_type_ = object_type;
  }
  void set_supports_create(bool supports_create) {
    supports_create_ = supports_create;
  }
  void set_primary_mimetypes(
      ScopedVector<std::string>* primary_mimetypes) {
    primary_mimetypes_.swap(*primary_mimetypes);
  }
  void set_secondary_mimetypes(
      ScopedVector<std::string>* secondary_mimetypes) {
    secondary_mimetypes_.swap(*secondary_mimetypes);
  }
  void set_primary_extensions(
      ScopedVector<std::string>* primary_extensions) {
    primary_extensions_.swap(*primary_extensions);
  }
  void set_secondary_extensions(
      ScopedVector<std::string>* secondary_extensions) {
    secondary_extensions_.swap(*secondary_extensions);
  }
  void set_links(ScopedVector<Link>* links) {
    links_.swap(*links);
  }
  void set_app_icons(ScopedVector<AppIcon>* app_icons) {
    app_icons_.swap(*app_icons);
  }

 private:
  // Extracts "$t" value from the dictionary |value| and returns it in |result|.
  // If the string value can't be found, it returns false.
  static bool GetValueString(const base::Value* value,
                             std::string* result);

  std::string app_id_;
  std::string app_name_;
  std::string object_type_;
  bool supports_create_;
  ScopedVector<std::string> primary_mimetypes_;
  ScopedVector<std::string> secondary_mimetypes_;
  ScopedVector<std::string> primary_extensions_;
  ScopedVector<std::string> secondary_extensions_;
  ScopedVector<Link> links_;
  ScopedVector<AppIcon> app_icons_;
};

// Account metadata feed represents the metadata object attached to the user's
// account.
class AccountMetadata {
 public:
  AccountMetadata();
  virtual ~AccountMetadata();

  // Creates feed from parsed JSON Value.  You should call this
  // instead of instantiating JSONValueConverter by yourself because
  // this method does some post-process for some fields.  See
  // FillRemainingFields comment and implementation in ResourceEntry
  // class for the details.
  static scoped_ptr<AccountMetadata> CreateFrom(const base::Value& value);

  int64 quota_bytes_total() const {
    return quota_bytes_total_;
  }

  int64 quota_bytes_used() const {
    return quota_bytes_used_;
  }

  int64 largest_changestamp() const {
    return largest_changestamp_;
  }

  const ScopedVector<InstalledApp>& installed_apps() const {
    return installed_apps_;
  }

  void set_quota_bytes_total(int64 quota_bytes_total) {
    quota_bytes_total_ = quota_bytes_total;
  }
  void set_quota_bytes_used(int64 quota_bytes_used) {
    quota_bytes_used_ = quota_bytes_used;
  }
  void set_largest_changestamp(int64 largest_changestamp) {
    largest_changestamp_ = largest_changestamp;
  }
  void set_installed_apps(ScopedVector<InstalledApp>* installed_apps) {
    installed_apps_.swap(*installed_apps);
  }

  // Registers the mapping between JSON field names and the members in
  // this class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<AccountMetadata>* converter);

 private:
  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  int64 quota_bytes_total_;
  int64 quota_bytes_used_;
  int64 largest_changestamp_;
  ScopedVector<InstalledApp> installed_apps_;

  DISALLOW_COPY_AND_ASSIGN(AccountMetadata);
};


}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_GDATA_WAPI_PARSER_H_
