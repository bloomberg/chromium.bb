// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_parser.h"

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/json/json_value_converter.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "third_party/libxml/chromium/libxml_utils.h"

using base::Value;
using base::DictionaryValue;
using base::ListValue;

namespace gdata {

namespace {

// Term values for kSchemeKind category:
const char kSchemeKind[] = "http://schemas.google.com/g/2005#kind";
const char kTermPrefix[] = "http://schemas.google.com/docs/2007#";
const char kFileTerm[] = "file";
const char kFolderTerm[] = "folder";
const char kItemTerm[] = "item";
const char kPdfTerm[] = "pdf";
const char kDocumentTerm[] = "document";
const char kSpreadSheetTerm[] = "spreadsheet";
const char kPresentationTerm[] = "presentation";

const char kSchemeLabels[] = "http://schemas.google.com/g/2005/labels";

// Node names.
const char kAuthorNode[] = "author";
const char kCategoryNode[] = "category";
const char kContentNode[] = "content";
const char kEditedNode[] = "edited";
const char kEmailNode[] = "email";
const char kEntryNode[] = "entry";
const char kFeedLinkNode[] = "feedLink";
const char kFilenameNode[] = "filename";
const char kIDNode[] = "id";
const char kLastModifiedByNode[] = "lastModifiedBy";
const char kLinkNode[] = "link";
const char kMd5ChecksumNode[] = "md5Checksum";
const char kModifiedByMeDateNode[] = "modifiedByMeDate";
const char kNameNode[] = "name";
const char kPublishedNode[] = "published";
const char kQuotaBytesUsedNode[] = "quotaBytesUsed";
const char kResourceIdNode[] = "resourceId";
const char kSizeNode[] = "size";
const char kSuggestedFilenameNode[] = "suggestedFilename";
const char kTitleNode[] = "title";
const char kUpdatedNode[] = "updated";
const char kWritersCanInviteNode[] = "writersCanInvite";

// Field names.
const char kAuthorField[] = "author";
const char kCategoryField[] = "category";
const char kContentField[] = "content";
const char kDeletedField[] = "gd$deleted";
const char kETagField[] = "gd$etag";
const char kEmailField[] = "email.$t";
const char kEntryField[] = "entry";
const char kFeedField[] = "feed";
const char kFeedLinkField[] = "gd$feedLink";
const char kFileNameField[] = "docs$filename.$t";
const char kHrefField[] = "href";
const char kIDField[] = "id.$t";
const char kInstalledAppField[] = "docs$installedApp";
const char kInstalledAppNameField[] = "docs$installedAppName";
const char kInstalledAppObjectTypeField[] = "docs$installedAppObjectType";
const char kInstalledAppPrimaryFileExtensionField[] =
    "docs$installedAppPrimaryFileExtension";
const char kInstalledAppPrimaryMimeTypeField[] =
    "docs$installedAppPrimaryMimeType";
const char kInstalledAppSecondaryFileExtensionField[] =
    "docs$installedAppSecondaryFileExtension";
const char kInstalledAppSecondaryMimeTypeField[] =
    "docs$installedAppSecondaryMimeType";
const char kInstalledAppSupportsCreateField[] =
    "docs$installedAppSupportsCreate";
const char kItemsPerPageField[] = "openSearch$itemsPerPage.$t";
const char kLabelField[] = "label";
const char kLargestChangestampField[] = "docs$largestChangestamp.value";
const char kLinkField[] = "link";
const char kMD5Field[] = "docs$md5Checksum.$t";
const char kNameField[] = "name.$t";
const char kPublishedField[] = "published.$t";
const char kQuotaBytesTotalField[] = "gd$quotaBytesTotal.$t";
const char kQuotaBytesUsedField[] = "gd$quotaBytesUsed.$t";
const char kRelField[] = "rel";
const char kRemovedField[] = "gd$removed";
const char kResourceIdField[] = "gd$resourceId.$t";
const char kSchemeField[] = "scheme";
const char kSizeField[] = "docs$size.$t";
const char kSrcField[] = "src";
const char kStartIndexField[] = "openSearch$startIndex.$t";
const char kSuggestedFileNameField[] = "docs$suggestedFilename.$t";
const char kTField[] = "$t";
const char kTermField[] = "term";
const char kTitleField[] = "title";
const char kTitleTField[] = "title.$t";
const char kTypeField[] = "type";
const char kUpdatedField[] = "updated.$t";

// Attribute names.
// Attributes are not namespace-blind as node names in XmlReader.
const char kETagAttr[] = "gd:etag";
const char kEmailAttr[] = "email";
const char kHrefAttr[] = "href";
const char kLabelAttr[] = "label";
const char kNameAttr[] = "name";
const char kRelAttr[] = "rel";
const char kSchemeAttr[] = "scheme";
const char kSrcAttr[] = "src";
const char kTermAttr[] = "term";
const char kTypeAttr[] = "type";
const char kValueAttr[] = "value";

struct EntryKindMap {
  DocumentEntry::EntryKind kind;
  const char* entry;
  const char* extension;
};

const EntryKindMap kEntryKindMap[] = {
    { DocumentEntry::ITEM,         "item",         NULL},
    { DocumentEntry::DOCUMENT,     "document",     ".gdoc"},
    { DocumentEntry::SPREADSHEET,  "spreadsheet",  ".gsheet"},
    { DocumentEntry::PRESENTATION, "presentation", ".gslides" },
    { DocumentEntry::DRAWING,      "drawing",      ".gdraw"},
    { DocumentEntry::TABLE,        "table",        ".gtable"},
    { DocumentEntry::SITE,         "site",         NULL},
    { DocumentEntry::FOLDER,       "folder",       NULL},
    { DocumentEntry::FILE,         "file",         NULL},
    { DocumentEntry::PDF,          "pdf",          NULL},
};

struct LinkTypeMap {
  Link::LinkType type;
  const char* rel;
};

const LinkTypeMap kLinkTypeMap[] = {
    { Link::SELF,
      "self" },
    { Link::NEXT,
      "next" },
    { Link::PARENT,
      "http://schemas.google.com/docs/2007#parent" },
    { Link::ALTERNATE,
      "alternate"},
    { Link::EDIT,
      "edit" },
    { Link::EDIT_MEDIA,
      "edit-media" },
    { Link::ALT_EDIT_MEDIA,
      "http://schemas.google.com/docs/2007#alt-edit-media" },
    { Link::ALT_POST,
      "http://schemas.google.com/docs/2007#alt-post" },
    { Link::FEED,
      "http://schemas.google.com/g/2005#feed"},
    { Link::POST,
      "http://schemas.google.com/g/2005#post"},
    { Link::BATCH,
      "http://schemas.google.com/g/2005#batch"},
    { Link::THUMBNAIL,
      "http://schemas.google.com/docs/2007/thumbnail"},
    { Link::RESUMABLE_EDIT_MEDIA,
      "http://schemas.google.com/g/2005#resumable-edit-media"},
    { Link::RESUMABLE_CREATE_MEDIA,
      "http://schemas.google.com/g/2005#resumable-create-media"},
    { Link::TABLES_FEED,
      "http://schemas.google.com/spreadsheets/2006#tablesfeed"},
    { Link::WORKSHEET_FEED,
      "http://schemas.google.com/spreadsheets/2006#worksheetsfeed"},
    { Link::EMBED,
      "http://schemas.google.com/docs/2007#embed"},
    { Link::PRODUCT,
      "http://schemas.google.com/docs/2007#product"},
    { Link::ICON,
      "http://schemas.google.com/docs/2007#icon"},
};

struct FeedLinkTypeMap {
  FeedLink::FeedLinkType type;
  const char* rel;
};

const FeedLinkTypeMap kFeedLinkTypeMap[] = {
    { FeedLink::ACL,
      "http://schemas.google.com/acl/2007#accessControlList" },
    { FeedLink::REVISIONS,
      "http://schemas.google.com/docs/2007/revisions" },
};

struct CategoryTypeMap {
  Category::CategoryType type;
  const char* scheme;
};

const CategoryTypeMap kCategoryTypeMap[] = {
    { Category::KIND,
      "http://schemas.google.com/g/2005#kind" },
    { Category::LABEL,
      "http://schemas.google.com/g/2005/labels" },
};

// Converts |url_string| to |result|.  Always returns true to be used
// for JSONValueConverter::RegisterCustomField method.
// TODO(mukai): make it return false in case of invalid |url_string|.
bool GetGURLFromString(const base::StringPiece& url_string, GURL* result) {
  *result = GURL(url_string.as_string());
  return true;
}

// Converts boolean string values like "true" into bool.
bool GetBoolFromString(const base::StringPiece& value, bool* result) {
  *result = (value == "true");
  return true;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Author implementation

Author::Author() {
}

// static
void Author::RegisterJSONConverter(
    base::JSONValueConverter<Author>* converter) {
  converter->RegisterStringField(kNameField, &Author::name_);
  converter->RegisterStringField(kEmailField, &Author::email_);
}

Author* Author::CreateFromXml(XmlReader* xml_reader) {
  if (xml_reader->NodeName() != kAuthorNode)
    return NULL;

  if (!xml_reader->Read())
    return NULL;

  const int depth = xml_reader->Depth();
  Author* author = new Author();
  bool skip_read = false;
  do {
    skip_read = false;
    DVLOG(1) << "Parsing author node " << xml_reader->NodeName()
            << ", depth = " << depth;
    if (xml_reader->NodeName() == kNameNode) {
     std::string name;
     if (xml_reader->ReadElementContent(&name))
       author->name_ = UTF8ToUTF16(name);
     skip_read = true;
    } else if (xml_reader->NodeName() == kEmailNode) {
     xml_reader->ReadElementContent(&author->email_);
     skip_read = true;
    }
  } while (depth == xml_reader->Depth() && (skip_read || xml_reader->Next()));
  return author;
}

////////////////////////////////////////////////////////////////////////////////
// Link implementation

Link::Link() : type_(Link::UNKNOWN) {
}

// static.
bool Link::GetLinkType(const base::StringPiece& rel, Link::LinkType* result) {
  for (size_t i = 0; i < arraysize(kLinkTypeMap); i++) {
    if (rel == kLinkTypeMap[i].rel) {
      *result = kLinkTypeMap[i].type;
      return true;
    }
  }
  // Let unknown link types through, just report it; if the link type is needed
  // in the future, add it into LinkType and kLinkTypeMap.
  DVLOG(1) << "Ignoring unknown link type for rel " << rel;
  *result = UNKNOWN;
  return true;
}

// static
void Link::RegisterJSONConverter(base::JSONValueConverter<Link>* converter) {
  converter->RegisterCustomField<Link::LinkType>(
      kRelField, &Link::type_, &Link::GetLinkType);
  converter->RegisterCustomField(kHrefField, &Link::href_, &GetGURLFromString);
  converter->RegisterStringField(kTitleField, &Link::title_);
  converter->RegisterStringField(kTypeField, &Link::mime_type_);
}

// static.
Link* Link::CreateFromXml(XmlReader* xml_reader) {
  if (xml_reader->NodeName() != kLinkNode)
    return NULL;

  Link* link = new Link();
  xml_reader->NodeAttribute(kTypeAttr, &link->mime_type_);

  std::string href;
  if (xml_reader->NodeAttribute(kHrefAttr, &href))
      link->href_ = GURL(href);

  std::string rel;
  if (xml_reader->NodeAttribute(kRelAttr, &rel))
    GetLinkType(rel, &link->type_);

  return link;
}

////////////////////////////////////////////////////////////////////////////////
// FeedLink implementation

FeedLink::FeedLink() : type_(FeedLink::UNKNOWN) {
}

// static.
bool FeedLink::GetFeedLinkType(
    const base::StringPiece& rel, FeedLink::FeedLinkType* result) {
  for (size_t i = 0; i < arraysize(kFeedLinkTypeMap); i++) {
    if (rel == kFeedLinkTypeMap[i].rel) {
      *result = kFeedLinkTypeMap[i].type;
      return true;
    }
  }
  DVLOG(1) << "Unknown feed link type for rel " << rel;
  return false;
}

// static
void FeedLink::RegisterJSONConverter(
    base::JSONValueConverter<FeedLink>* converter) {
  converter->RegisterCustomField<FeedLink::FeedLinkType>(
      kRelField, &FeedLink::type_, &FeedLink::GetFeedLinkType);
  converter->RegisterCustomField(
      kHrefField, &FeedLink::href_, &GetGURLFromString);
}

// static
FeedLink* FeedLink::CreateFromXml(XmlReader* xml_reader) {
  if (xml_reader->NodeName() != kFeedLinkNode)
    return NULL;

  FeedLink* link = new FeedLink();
  std::string href;
  if (xml_reader->NodeAttribute(kHrefAttr, &href))
    link->href_ = GURL(href);

  std::string rel;
  if (xml_reader->NodeAttribute(kRelAttr, &rel))
    GetFeedLinkType(rel, &link->type_);

  return link;
}

////////////////////////////////////////////////////////////////////////////////
// Category implementation

Category::Category() : type_(UNKNOWN) {
}

// Converts category.scheme into CategoryType enum.
bool Category::GetCategoryTypeFromScheme(
    const base::StringPiece& scheme, Category::CategoryType* result) {
  for (size_t i = 0; i < arraysize(kCategoryTypeMap); i++) {
    if (scheme == kCategoryTypeMap[i].scheme) {
      *result = kCategoryTypeMap[i].type;
      return true;
    }
  }
  DVLOG(1) << "Unknown feed link type for scheme " << scheme;
  return false;
}

// static
void Category::RegisterJSONConverter(
    base::JSONValueConverter<Category>* converter) {
  converter->RegisterStringField(kLabelField, &Category::label_);
  converter->RegisterCustomField<Category::CategoryType>(
      kSchemeField, &Category::type_, &Category::GetCategoryTypeFromScheme);
  converter->RegisterStringField(kTermField, &Category::term_);
}

// static
Category* Category::CreateFromXml(XmlReader* xml_reader) {
  if (xml_reader->NodeName() != kCategoryNode)
    return NULL;

  Category* category = new Category();
  xml_reader->NodeAttribute(kTermAttr, &category->term_);

  std::string scheme;
  if (xml_reader->NodeAttribute(kSchemeAttr, &scheme))
    GetCategoryTypeFromScheme(scheme, &category->type_);

  std::string label;
  if (xml_reader->NodeAttribute(kLabelAttr, &label))
    category->label_ = UTF8ToUTF16(label);

  return category;
}

const Link* FeedEntry::GetLinkByType(Link::LinkType type) const {
  for (size_t i = 0; i < links_.size(); ++i) {
    if (links_[i]->type() == type)
      return links_[i];
  }
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// Content implementation

Content::Content() {
}

// static
void Content::RegisterJSONConverter(
    base::JSONValueConverter<Content>* converter) {
  converter->RegisterCustomField(kSrcField, &Content::url_, &GetGURLFromString);
  converter->RegisterStringField(kTypeField, &Content::mime_type_);
}

Content* Content::CreateFromXml(XmlReader* xml_reader) {
  if (xml_reader->NodeName() != kContentNode)
    return NULL;

  Content* content = new Content();
  std::string src;
  if (xml_reader->NodeAttribute(kSrcAttr, &src))
    content->url_ = GURL(src);

  xml_reader->NodeAttribute(kTypeAttr, &content->mime_type_);
  return content;
}

////////////////////////////////////////////////////////////////////////////////
// FeedEntry implementation

FeedEntry::FeedEntry() {
}

FeedEntry::~FeedEntry() {
}

// static
void FeedEntry::RegisterJSONConverter(
    base::JSONValueConverter<FeedEntry>* converter) {
  converter->RegisterStringField(kETagField, &FeedEntry::etag_);
  converter->RegisterRepeatedMessage(kAuthorField, &FeedEntry::authors_);
  converter->RegisterRepeatedMessage(kLinkField, &FeedEntry::links_);
  converter->RegisterRepeatedMessage(kCategoryField, &FeedEntry::categories_);
  converter->RegisterCustomField<base::Time>(
      kUpdatedField,
      &FeedEntry::updated_time_,
      &FeedEntry::GetTimeFromString);
}

// static
bool FeedEntry::GetTimeFromString(const base::StringPiece& raw_value,
                                   base::Time* time) {
  const char kTimeParsingDelimiters[] = "-:.TZ";
  std::vector<base::StringPiece> parts;
  if (Tokenize(raw_value, kTimeParsingDelimiters, &parts) != 7)
    return false;

  base::Time::Exploded exploded;
  if (!base::StringToInt(parts[0], &exploded.year) ||
      !base::StringToInt(parts[1], &exploded.month) ||
      !base::StringToInt(parts[2], &exploded.day_of_month) ||
      !base::StringToInt(parts[3], &exploded.hour) ||
      !base::StringToInt(parts[4], &exploded.minute) ||
      !base::StringToInt(parts[5], &exploded.second) ||
      !base::StringToInt(parts[6], &exploded.millisecond)) {
    return false;
  }

  exploded.day_of_week = 0;
  if (!exploded.HasValidValues())
    return false;

  *time = base::Time::FromLocalExploded(exploded);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// DocumentEntry implementation

DocumentEntry::DocumentEntry()
    : kind_(DocumentEntry::UNKNOWN),
      file_size_(0),
      deleted_(false),
      removed_(false) {
}

DocumentEntry::~DocumentEntry() {
}

bool DocumentEntry::HasFieldPresent(const base::Value* value,
                                    bool* result) {
  *result = (value != NULL);
  return true;
}

// static
void DocumentEntry::RegisterJSONConverter(
    base::JSONValueConverter<DocumentEntry>* converter) {
  // inheritant the parent registrations.
  FeedEntry::RegisterJSONConverter(
      reinterpret_cast<base::JSONValueConverter<FeedEntry>*>(converter));
  converter->RegisterStringField(
      kResourceIdField, &DocumentEntry::resource_id_);
  converter->RegisterStringField(kIDField, &DocumentEntry::id_);
  converter->RegisterStringField(kTitleTField, &DocumentEntry::title_);
  converter->RegisterCustomField<base::Time>(
      kPublishedField, &DocumentEntry::published_time_,
      &FeedEntry::GetTimeFromString);
  converter->RegisterRepeatedMessage(
      kFeedLinkField, &DocumentEntry::feed_links_);
  converter->RegisterNestedField(kContentField, &DocumentEntry::content_);

  // File properties.  If the document type is not a normal file, then
  // that's no problem because those feed must not have these fields
  // themselves, which does not report errors.
  converter->RegisterStringField(kFileNameField, &DocumentEntry::filename_);
  converter->RegisterStringField(kMD5Field, &DocumentEntry::file_md5_);
  converter->RegisterCustomField<int64>(
      kSizeField, &DocumentEntry::file_size_, &base::StringToInt64);
  converter->RegisterStringField(
      kSuggestedFileNameField, &DocumentEntry::suggested_filename_);
  // Deleted are treated as 'trashed' items on web client side. Removed files
  // are gone for good. We treat both cases as 'deleted' for this client.
  converter->RegisterCustomValueField<bool>(
      kDeletedField, &DocumentEntry::deleted_, &DocumentEntry::HasFieldPresent);
  converter->RegisterCustomValueField<bool>(
      kRemovedField, &DocumentEntry::removed_, &DocumentEntry::HasFieldPresent);
}

std::string DocumentEntry::GetHostedDocumentExtension() const {
  for (size_t i = 0; i < arraysize(kEntryKindMap); i++) {
    if (kEntryKindMap[i].kind == kind_) {
      if (kEntryKindMap[i].extension)
        return std::string(kEntryKindMap[i].extension);
      else
        return std::string();
    }
  }
  return std::string();
}

// static
bool DocumentEntry::HasHostedDocumentExtension(const FilePath& file) {
  FilePath::StringType file_extension = file.Extension();
  for (size_t i = 0; i < arraysize(kEntryKindMap); ++i) {
    const char* document_extension = kEntryKindMap[i].extension;
    if (document_extension && file_extension == document_extension)
      return true;
  }
  return false;
}

// static
DocumentEntry::EntryKind DocumentEntry::GetEntryKindFromTerm(
    const std::string& term) {
  if (!StartsWithASCII(term, kTermPrefix, false)) {
    DVLOG(1) << "Unexpected term prefix term " << term;
    return DocumentEntry::UNKNOWN;
  }

  std::string type = term.substr(strlen(kTermPrefix));
  for (size_t i = 0; i < arraysize(kEntryKindMap); i++) {
    if (type == kEntryKindMap[i].entry)
      return kEntryKindMap[i].kind;
  }
  DVLOG(1) << "Unknown entry type for term " << term << ", type " << type;
  return DocumentEntry::UNKNOWN;
}

void DocumentEntry::FillRemainingFields() {
  // Set |kind_| and |labels_| based on the |categories_| in the class.
  // JSONValueConverter does not have the ability to catch an element in a list
  // based on a predicate.  Thus we need to iterate over |categories_| and
  // find the elements to set these fields as a post-process.
  for (size_t i = 0; i < categories_.size(); ++i) {
    const Category* category = categories_[i];
    if (category->type() == Category::KIND)
      kind_ = GetEntryKindFromTerm(category->term());
    else if (category->type() == Category::LABEL)
      labels_.push_back(category->label());
  }
}

// static
DocumentEntry* DocumentEntry::CreateFrom(const base::Value* value) {
  base::JSONValueConverter<DocumentEntry> converter;
  scoped_ptr<DocumentEntry> entry(new DocumentEntry());
  if (!converter.Convert(*value, entry.get())) {
    DVLOG(1) << "Invalid document entry!";
    return NULL;
  }

  entry->FillRemainingFields();
  return entry.release();
}

// static.
DocumentEntry* DocumentEntry::CreateFromXml(XmlReader* xml_reader) {
  if (xml_reader->NodeName() != kEntryNode)
    return NULL;

  DocumentEntry* entry = new DocumentEntry();
  xml_reader->NodeAttribute(kETagAttr, &entry->etag_);

  if (!xml_reader->Read())
    return entry;

  bool skip_read = false;
  do {
    DVLOG(1) << "Parsing node " << xml_reader->NodeName();
    skip_read = false;

    if (xml_reader->NodeName() == kAuthorNode) {
      scoped_ptr<Author> author(Author::CreateFromXml(xml_reader));
      if (author.get())
        entry->authors_.push_back(author.release());
    }

    if (xml_reader->NodeName() == kContentNode) {
      scoped_ptr<Content> content(Content::CreateFromXml(xml_reader));
      if (content.get())
        entry->content_ = *content.get();
    } else if (xml_reader->NodeName() == kLinkNode) {
      scoped_ptr<Link> link(Link::CreateFromXml(xml_reader));
      if (link.get())
        entry->links_.push_back(link.release());
    } else if (xml_reader->NodeName() == kFeedLinkNode) {
      scoped_ptr<FeedLink> link(FeedLink::CreateFromXml(xml_reader));
      if (link.get())
        entry->feed_links_.push_back(link.release());
    } else if (xml_reader->NodeName() == kCategoryNode) {
      scoped_ptr<Category> category(Category::CreateFromXml(xml_reader));
      if (category.get())
        entry->categories_.push_back(category.release());
    } else if (xml_reader->NodeName() == kUpdatedNode) {
      std::string time;
      if (xml_reader->ReadElementContent(&time))
        GetTimeFromString(time, &entry->updated_time_);
      skip_read = true;
    } else if (xml_reader->NodeName() == kPublishedNode) {
      std::string time;
      if (xml_reader->ReadElementContent(&time))
        GetTimeFromString(time, &entry->published_time_);
      skip_read = true;
    } else if (xml_reader->NodeName() == kIDNode) {
      xml_reader->ReadElementContent(&entry->id_);
      skip_read = true;
    } else if (xml_reader->NodeName() == kResourceIdNode) {
      xml_reader->ReadElementContent(&entry->resource_id_);
      skip_read = true;
    } else if (xml_reader->NodeName() == kTitleNode) {
      std::string title;
      if (xml_reader->ReadElementContent(&title))
        entry->title_ = UTF8ToUTF16(title);
      skip_read = true;
    } else if (xml_reader->NodeName() == kFilenameNode) {
      std::string file_name;
      if (xml_reader->ReadElementContent(&file_name))
        entry->filename_ = UTF8ToUTF16(file_name);
      skip_read = true;
    } else if (xml_reader->NodeName() == kSuggestedFilenameNode) {
      std::string suggested_filename;
      if (xml_reader->ReadElementContent(&suggested_filename))
        entry->suggested_filename_ = UTF8ToUTF16(suggested_filename);
      skip_read = true;
    } else if (xml_reader->NodeName() == kMd5ChecksumNode) {
      xml_reader->ReadElementContent(&entry->file_md5_);
      skip_read = true;
    } else if (xml_reader->NodeName() == kSizeNode) {
      std::string size;
      if (xml_reader->ReadElementContent(&size))
        base::StringToInt64(size, &entry->file_size_);
      skip_read = true;
    } else {
      DVLOG(1) << "Unknown node " << xml_reader->NodeName();
    }
  } while (skip_read || xml_reader->Next());

  entry->FillRemainingFields();
  return entry;
}

// static
std::string DocumentEntry::GetEntryNodeName() {
  return kEntryNode;
}

////////////////////////////////////////////////////////////////////////////////
// DocumentFeed implementation

DocumentFeed::DocumentFeed()
    : start_index_(0),
      items_per_page_(0),
      largest_changestamp_(0) {
}

DocumentFeed::~DocumentFeed() {
}

// static
void DocumentFeed::RegisterJSONConverter(
    base::JSONValueConverter<DocumentFeed>* converter) {
  // inheritance
  FeedEntry::RegisterJSONConverter(
      reinterpret_cast<base::JSONValueConverter<FeedEntry>*>(converter));
  // TODO(zelidrag): Once we figure out where these will be used, we should
  // check for valid start_index_ and items_per_page_ values.
  converter->RegisterCustomField<int>(
      kStartIndexField, &DocumentFeed::start_index_, &base::StringToInt);
  converter->RegisterCustomField<int>(
      kItemsPerPageField, &DocumentFeed::items_per_page_, &base::StringToInt);
  converter->RegisterStringField(kTitleTField, &DocumentFeed::title_);
  converter->RegisterRepeatedMessage(kEntryField, &DocumentFeed::entries_);
  converter->RegisterCustomField<int>(
     kLargestChangestampField, &DocumentFeed::largest_changestamp_,
     &base::StringToInt);
}

bool DocumentFeed::Parse(const base::Value& value) {
  base::JSONValueConverter<DocumentFeed> converter;
  if (!converter.Convert(value, this)) {
    DVLOG(1) << "Invalid document feed!";
    return false;
  }

  ScopedVector<DocumentEntry>::iterator iter = entries_.begin();
  while (iter != entries_.end()) {
    DocumentEntry* entry = (*iter);
    entry->FillRemainingFields();
    ++iter;
  }
  return true;
}

// static
scoped_ptr<DocumentFeed> DocumentFeed::ExtractAndParse(
    const base::Value& value) {
  const base::DictionaryValue* as_dict = NULL;
  base::DictionaryValue* feed_dict = NULL;
  if (value.GetAsDictionary(&as_dict) &&
      as_dict->GetDictionary(kFeedField, &feed_dict)) {
    return DocumentFeed::CreateFrom(*feed_dict);
  }
  return scoped_ptr<DocumentFeed>(NULL);
}

// static
scoped_ptr<DocumentFeed> DocumentFeed::CreateFrom(const base::Value& value) {
  scoped_ptr<DocumentFeed> feed(new DocumentFeed());
  if (!feed->Parse(value)) {
    DVLOG(1) << "Invalid document feed!";
    return scoped_ptr<DocumentFeed>(NULL);
  }

  return feed.Pass();
}

bool DocumentFeed::GetNextFeedURL(GURL* url) {
  DCHECK(url);
  for (size_t i = 0; i < links_.size(); ++i) {
    if (links_[i]->type() == Link::NEXT) {
      *url = links_[i]->href();
      return true;
    }
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// InstalledApp implementation

InstalledApp::InstalledApp() : supports_create_(false) {
}

InstalledApp::~InstalledApp() {
}

GURL InstalledApp::GetProductUrl() const {
  for (ScopedVector<Link>::const_iterator it = links_->begin();
       it != links_.end(); ++it) {
    const Link* link = *it;
    if (link->type() == Link::PRODUCT)
      return link->href();
  }
  return GURL();
}

// static
bool InstalledApp::GetValueString(const base::Value* value,
                                  std::string* result) {
  const base::DictionaryValue* dict = NULL;
  if (!value->GetAsDictionary(&dict))
    return false;

  if (!dict->GetString(kTField, result))
    return false;

  return true;
}

// static
void InstalledApp::RegisterJSONConverter(
    base::JSONValueConverter<InstalledApp>* converter) {
  converter->RegisterStringField(kInstalledAppNameField,
                                 &InstalledApp::app_name_);
  converter->RegisterStringField(kInstalledAppObjectTypeField,
                                 &InstalledApp::object_type_);
  converter->RegisterCustomField<bool>(kInstalledAppSupportsCreateField,
                                       &InstalledApp::supports_create_,
                                       &GetBoolFromString);
  converter->RegisterRepeatedCustomValue(kInstalledAppPrimaryMimeTypeField,
                                         &InstalledApp::primary_mimetypes_,
                                         &GetValueString);
  converter->RegisterRepeatedCustomValue(kInstalledAppSecondaryMimeTypeField,
                                         &InstalledApp::secondary_mimetypes_,
                                         &GetValueString);
  converter->RegisterRepeatedCustomValue(kInstalledAppPrimaryFileExtensionField,
                                         &InstalledApp::primary_extensions_,
                                         &GetValueString);
  converter->RegisterRepeatedCustomValue(
      kInstalledAppSecondaryFileExtensionField,
      &InstalledApp::secondary_extensions_,
      &GetValueString);
  converter->RegisterRepeatedMessage(kLinkField, &InstalledApp::links_);
}

////////////////////////////////////////////////////////////////////////////////
// AccountMetadataFeed implementation

AccountMetadataFeed::AccountMetadataFeed()
    : quota_bytes_total_(0),
      quota_bytes_used_(0),
      largest_changestamp_(0) {
}

AccountMetadataFeed::~AccountMetadataFeed() {
}

// static
void AccountMetadataFeed::RegisterJSONConverter(
    base::JSONValueConverter<AccountMetadataFeed>* converter) {
  converter->RegisterCustomField<int64>(
      kQuotaBytesTotalField,
      &AccountMetadataFeed::quota_bytes_total_,
      &base::StringToInt64);
  converter->RegisterCustomField<int64>(
      kQuotaBytesUsedField,
      &AccountMetadataFeed::quota_bytes_used_,
      &base::StringToInt64);
  converter->RegisterCustomField<int>(
      kLargestChangestampField,
      &AccountMetadataFeed::largest_changestamp_,
      &base::StringToInt);
  converter->RegisterRepeatedMessage(kInstalledAppField,
                                     &AccountMetadataFeed::installed_apps_);
}

// static
scoped_ptr<AccountMetadataFeed> AccountMetadataFeed::CreateFrom(
    const base::Value& value) {
  scoped_ptr<AccountMetadataFeed> feed(new AccountMetadataFeed());
  const base::DictionaryValue* dictionary = NULL;
  base::Value* entry = NULL;
  if (!value.GetAsDictionary(&dictionary) ||
      !dictionary->Get(kEntryField, &entry) ||
      !feed->Parse(*entry)) {
    LOG(ERROR) << "Unable to create: Invalid account metadata feed!";
    return scoped_ptr<AccountMetadataFeed>(NULL);
  }

  return feed.Pass();
}

bool AccountMetadataFeed::Parse(const base::Value& value) {
  base::JSONValueConverter<AccountMetadataFeed> converter;
  if (!converter.Convert(value, this)) {
    LOG(ERROR) << "Unable to parse: Invalid account metadata feed!";
    return false;
  }
  return true;
}

}  // namespace gdata
