// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_WAPI_PARSER_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_WAPI_PARSER_H_

#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/string_piece.h"
#include "base/time.h"
#include "chrome/browser/chromeos/gdata/drive.pb.h"
#include "googleurl/src/gurl.h"

class FilePath;
class Profile;
class XmlReader;

namespace base {
class Value;
class DictionaryValue;
template <class StructType>
class JSONValueConverter;

namespace internal {
template <class NestedType>
class RepeatedMessageConverter;
}  // namespace internal

}  // namespace base

// Defines data elements of Google Documents API as described in
// http://code.google.com/apis/documents/.
namespace gdata {

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
  };
  Link();
  ~Link();

  // Registers the mapping between JSON field names and the members in
  // this class.
  static void RegisterJSONConverter(base::JSONValueConverter<Link>* converter);

  // Creates document entry from parsed XML.
  static Link* CreateFromXml(XmlReader* xml_reader);

  // Type of the link.
  LinkType type() const { return type_; }

  // URL of the link.
  const GURL& href() const { return href_; }

  // Title of the link.
  const string16& title() const { return title_; }

  // For OPEN_WITH links, this contains the application ID. For all other link
  // types, it is the empty string.
  const std::string& app_id() const { return app_id_; }

  // Link MIME type.
  const std::string& mime_type() const { return mime_type_; }

 private:
  friend class DocumentEntry;
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
  string16 title_;
  std::string app_id_;
  std::string mime_type_;

  DISALLOW_COPY_AND_ASSIGN(Link);
};

// Feed links define links (URLs) to special list of entries (i.e. list of
// previous document revisions).
class FeedLink {
 public:
  enum FeedLinkType {
    FEED_LINK_UNKNOWN,
    FEED_LINK_ACL,
    FEED_LINK_REVISIONS,
  };
  FeedLink();

  // Registers the mapping between JSON field names and the members in
  // this class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<FeedLink>* converter);

  static FeedLink* CreateFromXml(XmlReader* xml_reader);

  // MIME type of the feed.
  FeedLinkType type() const { return type_; }

  // URL of the feed.
  const GURL& href() const { return href_; }

 private:
  friend class DocumentEntry;
  // Converts value of gd$feedLink.rel into FeedLinkType enum.
  // Outputs to |result| and returns true when |rel| has a valid
  // value.  Otherwise does nothing and returns false.
  static bool GetFeedLinkType(
      const base::StringPiece& rel, FeedLinkType* result);

  FeedLinkType type_;
  GURL href_;

  DISALLOW_COPY_AND_ASSIGN(FeedLink);
};

// Author represents an author of an entity.
class Author {
 public:
  Author();

  // Registers the mapping between JSON field names and the members in
  // this class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<Author>* converter);

  static Author* CreateFromXml(XmlReader* xml_reader);

  // Getters.
  const string16& name() const { return name_; }
  const std::string& email() const { return email_; }

 private:
  friend class DocumentEntry;

  string16 name_;
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

  static Category* CreateFromXml(XmlReader* xml_reader);

  // Category label.
  const string16& label() const { return label_; }

  // Category type.
  CategoryType type() const { return type_; }

  // Category term.
  const std::string& term() const { return term_; }

 private:
  friend class DocumentEntry;
  // Converts category scheme into CategoryType enum. For example,
  // http://schemas.google.com/g/2005#kind => Category::CATEGORY_KIND
  // Returns false and does not change |result| when |scheme| has an
  // unrecognizable value.
  static bool GetCategoryTypeFromScheme(
      const base::StringPiece& scheme, CategoryType* result);

  string16 label_;
  CategoryType type_;
  std::string term_;

  DISALLOW_COPY_AND_ASSIGN(Category);
};

// Content details of a document: mime-type, url, and so on.
class Content {
 public:
  Content();

  // Registers the mapping between JSON field names and the members in
  // this class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<Content>* converter);

  static Content* CreateFromXml(XmlReader* xml_reader);

  const GURL& url() const { return url_; }
  const std::string& mime_type() const { return mime_type_; }

 private:
  friend class DocumentEntry;

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

// Base class for feed entries.
class FeedEntry {
 public:
  FeedEntry();
  virtual ~FeedEntry();

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

  // List of entry categories.
  const ScopedVector<Category>& categories() const { return categories_; }

  // Registers the mapping between JSON field names and the members in
  // this class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<FeedEntry>* converter);

 protected:
  std::string etag_;
  ScopedVector<Author> authors_;
  ScopedVector<Link> links_;
  ScopedVector<Category> categories_;
  base::Time updated_time_;

  DISALLOW_COPY_AND_ASSIGN(FeedEntry);
};

// Document feed entry.
class DocumentEntry : public FeedEntry {
 public:
  virtual ~DocumentEntry();

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
  static DocumentEntry* ExtractAndParse(const base::Value& value);

  // Creates document entry from parsed JSON Value.  You should call
  // this instead of instantiating JSONValueConverter by yourself
  // because this method does some post-process for some fields.  See
  // FillRemainingFields comment and implementation for the details.
  static DocumentEntry* CreateFrom(const base::Value& value);

  // Creates document entry from parsed XML.
  static DocumentEntry* CreateFromXml(XmlReader* xml_reader);

  // Creates document entry from FileResource.
  // TODO(kochi): This should go away soon. http://crbug.com/142293
  static DocumentEntry* CreateFromFileResource(const FileResource& file);

  // Creates document entry from ChangeResource.
  // Todo(Kochi): This should go away soon. http://crbug.com/142293
  static DocumentEntry* CreateFromChangeResource(const ChangeResource& change);

  // Returns name of entry node.
  static std::string GetEntryNodeName();

  // Registers the mapping between JSON field names and the members in
  // this class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<DocumentEntry>* converter);

  // Helper function for parsing bool fields based on presence of
  // their value nodes.
  static bool HasFieldPresent(const base::Value* value, bool* result);

  // Returns true if |file| has one of the hosted document extensions.
  static bool HasHostedDocumentExtension(const FilePath& file);

  // Document entry resource id.
  const std::string& resource_id() const { return resource_id_; }

  // Document entry id.
  const std::string& id() const { return id_; }

  // Document entry kind.
  DriveEntryKind kind() const { return kind_; }

  // Document entry title.
  const string16& title() const { return title_; }

  // Document entry published time.
  base::Time published_time() const { return published_time_; }

  // List of document feed labels.
  const std::vector<string16>& labels() const { return labels_; }

  // Document entry content URL.
  const GURL& content_url() const { return content_.url(); }

  // Document entry MIME type.
  const std::string& content_mime_type() const { return content_.mime_type(); }

  // List of document feed links.
  const ScopedVector<FeedLink>& feed_links() const { return feed_links_; }

  // Document feed file name (exists only for kinds FILE and PDF).
  const string16& filename() const { return filename_; }

  // Document feed suggested file name (exists only for kinds FILE and PDF).
  const string16& suggested_filename() const { return suggested_filename_; }

  // Document feed file content MD5 (exists only for kinds FILE and PDF).
  const std::string& file_md5() const { return file_md5_; }

  // Document feed file size (exists only for kinds FILE and PDF).
  int64 file_size() const { return file_size_; }

  // True if the file or directory is deleted (applicable to change feeds only).
  bool deleted() const { return deleted_ || removed_; }

  // Text version of document entry kind. Returns an empty string for
  // unknown entry kind.
  std::string GetEntryKindText() const;

  // Returns preferred file extension for hosted documents. If entry is not
  // a hosted document, this call returns an empty string.
  std::string GetHostedDocumentExtension() const;

  // True if document entry is remotely hosted.
  bool is_hosted_document() const {
    return ClassifyEntryKind(kind_) & KIND_OF_HOSTED_DOCUMENT;
  }
  // True if document entry hosted by Google Documents.
  bool is_google_document() const {
    return ClassifyEntryKind(kind_) & KIND_OF_GOOGLE_DOCUMENT;
  }
  // True if document entry is hosted by an external application.
  bool is_external_document() const {
    return ClassifyEntryKind(kind_) & KIND_OF_EXTERNAL_DOCUMENT;
  }
  // True if document entry is a folder (collection).
  bool is_folder() const { return ClassifyEntryKind(kind_) & KIND_OF_FOLDER; }
  // True if document entry is regular file.
  bool is_file() const { return ClassifyEntryKind(kind_) & KIND_OF_FILE; }
  // True if document entry can't be mapped to the file system.
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

 private:
  friend class base::internal::RepeatedMessageConverter<DocumentEntry>;
  friend class DocumentFeed;
  friend class ResumeUploadOperation;

  DocumentEntry();

  // Fills the remaining fields where JSONValueConverter cannot catch.
  void FillRemainingFields();

  // Converts categories.term into DriveEntryKind enum.
  static DriveEntryKind GetEntryKindFromTerm(const std::string& term);
  // Converts |kind| into its text identifier equivalent.
  static const char* GetEntryKindDescription(DriveEntryKind kind);

  std::string resource_id_;
  std::string id_;
  DriveEntryKind kind_;
  string16 title_;
  base::Time published_time_;
  std::vector<string16> labels_;
  Content content_;
  ScopedVector<FeedLink> feed_links_;
  // Optional fields for files only.
  string16 filename_;
  string16 suggested_filename_;
  std::string file_md5_;
  int64 file_size_;
  bool deleted_;
  bool removed_;

  DISALLOW_COPY_AND_ASSIGN(DocumentEntry);
};

// Document feed represents a list of entries. The feed is paginated and
// the rest of the feed can be fetched by retrieving the remaining parts of the
// feed from URLs provided by GetNextFeedURL() method.
class DocumentFeed : public FeedEntry {
 public:
  virtual ~DocumentFeed();

  // Extracts "feed" dictionary from the JSON value, and parse the contents,
  // using CreateFrom(). Returns NULL on failure. The input JSON data, coming
  // from the gdata server, looks like:
  //
  // {
  //   "encoding": "UTF-8",
  //   "feed": { ... },   // This function will extract this and parse.
  //   "version": "1.0"
  // }
  static scoped_ptr<DocumentFeed> ExtractAndParse(const base::Value& value);

  // Creates feed from parsed JSON Value.  You should call this
  // instead of instantiating JSONValueConverter by yourself because
  // this method does some post-process for some fields.  See
  // FillRemainingFields comment and implementation in DocumentEntry
  // class for the details.
  static scoped_ptr<DocumentFeed> CreateFrom(const base::Value& value);
  // Variant of CreateFrom() above, creates feed from parsed ChangeList.
  // TODO(kochi): This should go away soon. http://crbug.com/142293
  static scoped_ptr<DocumentFeed> CreateFromChangeList(
      const ChangeList& changelist);

  // Registers the mapping between JSON field names and the members in
  // this class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<DocumentFeed>* converter);

  // Returns true and passes|url| of the next feed if the current entry list
  // does not completed this feed.
  bool GetNextFeedURL(GURL* url);

  // List of document entries.
  const ScopedVector<DocumentEntry>& entries() const { return entries_; }

  // Releases entries_ into |entries|. This is a transfer of ownership, so the
  // caller is responsible for deleting the elements of |entries|.
  void ReleaseEntries(std::vector<DocumentEntry*>* entries);

  // Start index of the document entry list.
  int start_index() const { return start_index_; }

  // Number of items per feed of the document entry list.
  int items_per_page() const { return items_per_page_; }

  // The largest changestamp. Next time the documents should be fetched
  // from this changestamp.
  int64 largest_changestamp() const { return largest_changestamp_; }

  // Document entry list title.
  const std::string& title() { return title_; }

 private:
  DocumentFeed();

  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  ScopedVector<DocumentEntry> entries_;
  int start_index_;
  int items_per_page_;
  std::string title_;
  int64 largest_changestamp_;

  DISALLOW_COPY_AND_ASSIGN(DocumentFeed);
};

// Metadata representing installed Google Drive application.
class InstalledApp {
 public:
  typedef std::vector<std::pair<int, GURL> > IconList;

  InstalledApp();
  virtual ~InstalledApp();

  // WebApp name.
  const string16& app_name() const { return app_name_; }

  // Drive app id
  const std::string& app_id() const { return app_id_; }

  // Object (file) type name that is generated by this WebApp.
  const string16& object_type() const { return object_type_; }

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

 private:
  // Extracts "$t" value from the dictionary |value| and returns it in |result|.
  // If the string value can't be found, it returns false.
  static bool GetValueString(const base::Value* value,
                             std::string* result);

  std::string app_id_;
  string16 app_name_;
  string16 object_type_;
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
class AccountMetadataFeed {
 public:
  virtual ~AccountMetadataFeed();

  // Creates feed from parsed JSON Value.  You should call this
  // instead of instantiating JSONValueConverter by yourself because
  // this method does some post-process for some fields.  See
  // FillRemainingFields comment and implementation in DocumentEntry
  // class for the details.
  static scoped_ptr<AccountMetadataFeed> CreateFrom(const base::Value& value);

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

  // Registers the mapping between JSON field names and the members in
  // this class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<AccountMetadataFeed>* converter);

 private:
  AccountMetadataFeed();

  // Parses and initializes data members from content of |value|.
  // Return false if parsing fails.
  bool Parse(const base::Value& value);

  int64 quota_bytes_total_;
  int64 quota_bytes_used_;
  int64 largest_changestamp_;
  ScopedVector<InstalledApp> installed_apps_;

  DISALLOW_COPY_AND_ASSIGN(AccountMetadataFeed);
};


}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_WAPI_PARSER_H_
