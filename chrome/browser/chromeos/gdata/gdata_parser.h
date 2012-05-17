// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_PARSER_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_PARSER_H_
#pragma once

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/string_piece.h"
#include "base/time.h"
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

// Defines link (URL) of an entity (document, file, feed...). Each entity could
// have more than one link representing it.
class Link {
 public:
  enum LinkType {
    UNKNOWN,
    SELF,
    NEXT,
    PARENT,
    ALTERNATE,
    EDIT,
    EDIT_MEDIA,
    ALT_EDIT_MEDIA,
    ALT_POST,
    FEED,
    POST,
    BATCH,
    RESUMABLE_EDIT_MEDIA,
    RESUMABLE_CREATE_MEDIA,
    TABLES_FEED,
    WORKSHEET_FEED,
    THUMBNAIL,
    EMBED,
    PRODUCT,
    ICON,
  };
  Link();

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

  // Link MIME type.
  const std::string& mime_type() const { return mime_type_; }

 private:
  friend class DocumentEntry;
  // Converts value of link.rel into LinkType. Outputs to |result| and
  // returns true when |rel| has a valid value. Otherwise does nothing
  // and returns false.
  static bool GetLinkType(const base::StringPiece& rel, LinkType* result);

  LinkType type_;
  GURL href_;
  string16 title_;
  std::string mime_type_;

  DISALLOW_COPY_AND_ASSIGN(Link);
};

// Feed links define links (URLs) to special list of entries (i.e. list of
// previous document revisions).
class FeedLink {
 public:
  enum FeedLinkType {
    UNKNOWN,
    ACL,
    REVISIONS,
  };
  FeedLink();

  // Registers the mapping between JSON field names and the members in
  // this class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<FeedLink>* converter);

  static FeedLink* CreateFromXml(XmlReader* xml_reader);

  // URL of the feed.
  FeedLinkType type() const { return type_; }

  // MIME type of the feed.
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
    UNKNOWN,
    ITEM,
    KIND,
    LABEL,
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
  // Converts catory scheme into CategoryType enum. For example,
  // http://schemas.google.com/g/2005#kind => Category::KIND
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

  // Returns true when time string (in format yyyy-mm-ddThh:mm:ss.dddZ), is
  // successfully parsed and output as |time|.
  static bool GetTimeFromString(
      const base::StringPiece& raw_value, base::Time* time);

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
  enum EntryKind {
    UNKNOWN       = 0x000000,
    // Special entries.
    ITEM          = 0x001001,
    SITE          = 0x001002,
    // Hosted Google document.
    DOCUMENT      = 0x002101,
    SPREADSHEET   = 0x002102,
    PRESENTATION  = 0x002103,
    DRAWING       = 0x002104,
    TABLE         = 0x002105,
    // Hosted external application document.
    EXTERNAL_APP  = 0x002201,
    // Folders, collections.
    FOLDER        = 0x004001,
    // Regular files.
    FILE          = 0x008001,
    PDF           = 0x008002,
  };
  virtual ~DocumentEntry();

  // Creates document entry from parsed JSON Value.  You should call
  // this instead of instantiating JSONValueConverter by yourself
  // because this method does some post-process for some fields.  See
  // FillRemainingFields comment and implementation for the details.
  static DocumentEntry* CreateFrom(const base::Value* value);

  // Creates document entry from parsed XML.
  static DocumentEntry* CreateFromXml(XmlReader* xml_reader);

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
  EntryKind kind() const { return kind_; }

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
  bool is_hosted_document() const { return (kind_ & 0x002000) == 0x002000; }
  // True if document entry hosted by Google Documents.
  bool is_google_document() const { return (kind_ & 0x002100) == 0x002100; }
  // True if document entry is hosted by an external application.
  bool is_external_document() const { return (kind_ & 0x002200) == 0x002200; }
  // True if document entry is a folder (collection).
  bool is_folder() const { return (kind_ & 0x004000) != 0; }
  // True if document entry is regular file.
  bool is_file() const { return (kind_ & 0x008000) != 0; }
  // True if document entry can't be mapped to the file system.
  bool is_special() const {
    return !is_file() && !is_folder() && !is_hosted_document();
  }

 private:
  friend class base::internal::RepeatedMessageConverter<DocumentEntry>;
  friend class DocumentFeed;
  friend class ResumeUploadOperation;

  DocumentEntry();

  // Fills the remaining fields where JSONValueConverter cannot catch.
  void FillRemainingFields();

  // Converts categories.term into EntryKind enum.
  static EntryKind GetEntryKindFromTerm(const std::string& term);
  // Converts |kind| into its text identifier equivalent.
  static const char* GetEntryKindDescription(EntryKind kind);

  std::string resource_id_;
  std::string id_;
  EntryKind kind_;
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

  // Registers the mapping between JSON field names and the members in
  // this class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<DocumentFeed>* converter);

  // Returns true and passes|url| of the next feed if the current entry list
  // does not completed this feed.
  bool GetNextFeedURL(GURL* url);

  // List of document entries.
  const ScopedVector<DocumentEntry>& entries() const { return entries_; }

  // Start index of the document entry list.
  int start_index() const { return start_index_; }

  // Number of items per feed of the document entry list.
  int items_per_page() const { return items_per_page_; }

  // The largest changestamp. Next time the documents should be fetched
  // from this changestamp.
  int largest_changestamp() const { return largest_changestamp_; }

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
  int largest_changestamp_;

  DISALLOW_COPY_AND_ASSIGN(DocumentFeed);
};

// Metadata representing installed Google Drive application.
class InstalledApp {
 public:
  InstalledApp();
  virtual ~InstalledApp();

  // WebApp name.
  const string16& app_name() const { return app_name_; }

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

  string16 app_name_;
  string16 object_type_;
  bool supports_create_;
  ScopedVector<std::string> primary_mimetypes_;
  ScopedVector<std::string> secondary_mimetypes_;
  ScopedVector<std::string> primary_extensions_;
  ScopedVector<std::string> secondary_extensions_;
  ScopedVector<Link> links_;
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

  int largest_changestamp() const {
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
  int largest_changestamp_;
  ScopedVector<InstalledApp> installed_apps_;

  DISALLOW_COPY_AND_ASSIGN(AccountMetadataFeed);
};


}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_PARSER_H_
