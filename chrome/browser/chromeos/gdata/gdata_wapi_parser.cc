// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_wapi_parser.h"

#include <algorithm>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/json/json_value_converter.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/gdata/drive_api_parser.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
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
const char kInstalledAppIdField[] = "docs$installedAppId";
const char kInstalledAppIconField[] = "docs$installedAppIcon";
const char kInstalledAppIconCategoryField[] = "docs$installedAppIconCategory";
const char kInstalledAppIconSizeField[] = "docs$installedAppIconSize";
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

// Link Prefixes
const char kOpenWithPrefix[] = "http://schemas.google.com/docs/2007#open-with-";
const size_t kOpenWithPrefixSize = arraysize(kOpenWithPrefix) - 1;

struct EntryKindMap {
  DriveEntryKind kind;
  const char* entry;
  const char* extension;
};

const EntryKindMap kEntryKindMap[] = {
    { ENTRY_KIND_UNKNOWN,      "unknown",      NULL},
    { ENTRY_KIND_ITEM,         "item",         NULL},
    { ENTRY_KIND_DOCUMENT,     "document",     ".gdoc"},
    { ENTRY_KIND_SPREADSHEET,  "spreadsheet",  ".gsheet"},
    { ENTRY_KIND_PRESENTATION, "presentation", ".gslides" },
    { ENTRY_KIND_DRAWING,      "drawing",      ".gdraw"},
    { ENTRY_KIND_TABLE,        "table",        ".gtable"},
    { ENTRY_KIND_EXTERNAL_APP, "externalapp",  ".glink"},
    { ENTRY_KIND_SITE,         "site",         NULL},
    { ENTRY_KIND_FOLDER,       "folder",       NULL},
    { ENTRY_KIND_FILE,         "file",         NULL},
    { ENTRY_KIND_PDF,          "pdf",          NULL},
};
COMPILE_ASSERT(arraysize(kEntryKindMap) == ENTRY_KIND_MAX_VALUE,
               EntryKindMap_and_DriveEntryKind_are_not_in_sync);

struct LinkTypeMap {
  Link::LinkType type;
  const char* rel;
};

const LinkTypeMap kLinkTypeMap[] = {
    { Link::LINK_SELF,
      "self" },
    { Link::LINK_NEXT,
      "next" },
    { Link::LINK_PARENT,
      "http://schemas.google.com/docs/2007#parent" },
    { Link::LINK_ALTERNATE,
      "alternate"},
    { Link::LINK_EDIT,
      "edit" },
    { Link::LINK_EDIT_MEDIA,
      "edit-media" },
    { Link::LINK_ALT_EDIT_MEDIA,
      "http://schemas.google.com/docs/2007#alt-edit-media" },
    { Link::LINK_ALT_POST,
      "http://schemas.google.com/docs/2007#alt-post" },
    { Link::LINK_FEED,
      "http://schemas.google.com/g/2005#feed"},
    { Link::LINK_POST,
      "http://schemas.google.com/g/2005#post"},
    { Link::LINK_BATCH,
      "http://schemas.google.com/g/2005#batch"},
    { Link::LINK_THUMBNAIL,
      "http://schemas.google.com/docs/2007/thumbnail"},
    { Link::LINK_RESUMABLE_EDIT_MEDIA,
      "http://schemas.google.com/g/2005#resumable-edit-media"},
    { Link::LINK_RESUMABLE_CREATE_MEDIA,
      "http://schemas.google.com/g/2005#resumable-create-media"},
    { Link::LINK_TABLES_FEED,
      "http://schemas.google.com/spreadsheets/2006#tablesfeed"},
    { Link::LINK_WORKSHEET_FEED,
      "http://schemas.google.com/spreadsheets/2006#worksheetsfeed"},
    { Link::LINK_EMBED,
      "http://schemas.google.com/docs/2007#embed"},
    { Link::LINK_PRODUCT,
      "http://schemas.google.com/docs/2007#product"},
    { Link::LINK_ICON,
      "http://schemas.google.com/docs/2007#icon"},
};

struct FeedLinkTypeMap {
  FeedLink::FeedLinkType type;
  const char* rel;
};

const FeedLinkTypeMap kFeedLinkTypeMap[] = {
    { FeedLink::FEED_LINK_ACL,
      "http://schemas.google.com/acl/2007#accessControlList" },
    { FeedLink::FEED_LINK_REVISIONS,
      "http://schemas.google.com/docs/2007/revisions" },
};

struct CategoryTypeMap {
  Category::CategoryType type;
  const char* scheme;
};

const CategoryTypeMap kCategoryTypeMap[] = {
    { Category::CATEGORY_KIND, "http://schemas.google.com/g/2005#kind" },
    { Category::CATEGORY_LABEL, "http://schemas.google.com/g/2005/labels" },
};

struct AppIconCategoryMap {
  AppIcon::IconCategory category;
  const char* category_name;
};

const AppIconCategoryMap kAppIconCategoryMap[] = {
    { AppIcon::ICON_DOCUMENT, "document" },
    { AppIcon::ICON_APPLICATION, "application" },
    { AppIcon::ICON_SHARED_DOCUMENT, "documentShared" },
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

bool SortBySize(const InstalledApp::IconList::value_type& a,
                const InstalledApp::IconList::value_type& b) {
  return a.first < b.first;
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

Link::Link() : type_(Link::LINK_UNKNOWN) {
}

Link::~Link() {
}

// static
bool Link::GetAppID(const base::StringPiece& rel, std::string* app_id) {
  DCHECK(app_id);
  // Fast return path if the link clearly isn't an OPEN_WITH link.
  if (rel.size() < kOpenWithPrefixSize) {
    app_id->clear();
    return true;
  }

  const std::string kOpenWithPrefixStr(kOpenWithPrefix);
  if (StartsWithASCII(rel.as_string(), kOpenWithPrefixStr, false)) {
    *app_id = rel.as_string().substr(kOpenWithPrefixStr.size());
    return true;
  }

  app_id->clear();
  return true;
}

// static.
bool Link::GetLinkType(const base::StringPiece& rel, Link::LinkType* type) {
  DCHECK(type);
  for (size_t i = 0; i < arraysize(kLinkTypeMap); i++) {
    if (rel == kLinkTypeMap[i].rel) {
      *type = kLinkTypeMap[i].type;
      return true;
    }
  }

  // OPEN_WITH links have extra information at the end of the rel that is unique
  // for each one, so we can't just check the usual map. This check is slightly
  // redundant to provide a quick skip if it's obviously not an OPEN_WITH url.
  if (rel.size() >= kOpenWithPrefixSize &&
      StartsWithASCII(rel.as_string(), kOpenWithPrefix, false)) {
    *type = LINK_OPEN_WITH;
    return true;
  }

  // Let unknown link types through, just report it; if the link type is needed
  // in the future, add it into LinkType and kLinkTypeMap.
  DVLOG(1) << "Ignoring unknown link type for rel " << rel;
  *type = LINK_UNKNOWN;
  return true;
}

// static
void Link::RegisterJSONConverter(base::JSONValueConverter<Link>* converter) {
  converter->RegisterCustomField<Link::LinkType>(kRelField,
                                                 &Link::type_,
                                                 &Link::GetLinkType);
  // We have to register kRelField twice because we extract two different pieces
  // of data from the same rel field.
  converter->RegisterCustomField<std::string>(kRelField,
                                              &Link::app_id_,
                                              &Link::GetAppID);
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
  if (xml_reader->NodeAttribute(kRelAttr, &rel)) {
    GetLinkType(rel, &link->type_);
    if (link->type_ == LINK_OPEN_WITH)
      GetAppID(rel, &link->app_id_);
  }

  return link;
}

////////////////////////////////////////////////////////////////////////////////
// FeedLink implementation

FeedLink::FeedLink() : type_(FeedLink::FEED_LINK_UNKNOWN) {
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

Category::Category() : type_(CATEGORY_UNKNOWN) {
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
// AppIcon implementation

AppIcon::AppIcon() : category_(AppIcon::ICON_UNKNOWN), icon_side_length_(0) {
}

AppIcon::~AppIcon() {
}

// static
void AppIcon::RegisterJSONConverter(
    base::JSONValueConverter<AppIcon>* converter) {
  converter->RegisterCustomField<AppIcon::IconCategory>(
      kInstalledAppIconCategoryField,
      &AppIcon::category_,
      &AppIcon::GetIconCategory);
  converter->RegisterCustomField<int>(kInstalledAppIconSizeField,
                                      &AppIcon::icon_side_length_,
                                      base::StringToInt);
  converter->RegisterRepeatedMessage(kLinkField, &AppIcon::links_);
}

GURL AppIcon::GetIconURL() const {
  for (size_t i = 0; i < links_.size(); ++i) {
    if (links_[i]->type() == Link::LINK_ICON)
      return links_[i]->href();
  }
  return GURL();
}

// static
bool AppIcon::GetIconCategory(const base::StringPiece& category,
                              AppIcon::IconCategory* result) {
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
      &gdata::util::GetTimeFromString);
}

////////////////////////////////////////////////////////////////////////////////
// DocumentEntry implementation

DocumentEntry::DocumentEntry()
    : kind_(ENTRY_KIND_UNKNOWN),
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
      &gdata::util::GetTimeFromString);
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
DriveEntryKind DocumentEntry::GetEntryKindFromTerm(
    const std::string& term) {
  if (!StartsWithASCII(term, kTermPrefix, false)) {
    DVLOG(1) << "Unexpected term prefix term " << term;
    return ENTRY_KIND_UNKNOWN;
  }

  std::string type = term.substr(strlen(kTermPrefix));
  for (size_t i = 0; i < arraysize(kEntryKindMap); i++) {
    if (type == kEntryKindMap[i].entry)
      return kEntryKindMap[i].kind;
  }
  DVLOG(1) << "Unknown entry type for term " << term << ", type " << type;
  return ENTRY_KIND_UNKNOWN;
}

// static
int DocumentEntry::ClassifyEntryKind(DriveEntryKind kind) {
  int classes = 0;

  // All DriveEntryKind members are listed here, so the compiler catches if a
  // newly added member is missing here.
  switch (kind) {
    case ENTRY_KIND_UNKNOWN:
    // Special entries.
    case ENTRY_KIND_ITEM:
    case ENTRY_KIND_SITE:
      break;

    // Hosted Google document.
    case ENTRY_KIND_DOCUMENT:
    case ENTRY_KIND_SPREADSHEET:
    case ENTRY_KIND_PRESENTATION:
    case ENTRY_KIND_DRAWING:
    case ENTRY_KIND_TABLE:
      classes = KIND_OF_GOOGLE_DOCUMENT | KIND_OF_HOSTED_DOCUMENT;
      break;

    // Hosted external application document.
    case ENTRY_KIND_EXTERNAL_APP:
      classes = KIND_OF_EXTERNAL_DOCUMENT | KIND_OF_HOSTED_DOCUMENT;
      break;

    // Folders, collections.
    case ENTRY_KIND_FOLDER:
      classes = KIND_OF_FOLDER;
      break;

    // Regular files.
    case ENTRY_KIND_FILE:
    case ENTRY_KIND_PDF:
      classes = KIND_OF_FILE;
      break;

    case ENTRY_KIND_MAX_VALUE:
      NOTREACHED();
  }

  return classes;
}

void DocumentEntry::FillRemainingFields() {
  // Set |kind_| and |labels_| based on the |categories_| in the class.
  // JSONValueConverter does not have the ability to catch an element in a list
  // based on a predicate.  Thus we need to iterate over |categories_| and
  // find the elements to set these fields as a post-process.
  for (size_t i = 0; i < categories_.size(); ++i) {
    const Category* category = categories_[i];
    if (category->type() == Category::CATEGORY_KIND)
      kind_ = GetEntryKindFromTerm(category->term());
    else if (category->type() == Category::CATEGORY_LABEL)
      labels_.push_back(category->label());
  }
}

// static
DocumentEntry* DocumentEntry::ExtractAndParse(
    const base::Value& value) {
  const base::DictionaryValue* as_dict = NULL;
  const base::DictionaryValue* entry_dict = NULL;
  if (value.GetAsDictionary(&as_dict) &&
      as_dict->GetDictionary(kEntryField, &entry_dict)) {
    return DocumentEntry::CreateFrom(*entry_dict);
  }
  return NULL;
}

// static
DocumentEntry* DocumentEntry::CreateFrom(const base::Value& value) {
  base::JSONValueConverter<DocumentEntry> converter;
  scoped_ptr<DocumentEntry> entry(new DocumentEntry());
  if (!converter.Convert(value, entry.get())) {
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
        gdata::util::GetTimeFromString(time, &entry->updated_time_);
      skip_read = true;
    } else if (xml_reader->NodeName() == kPublishedNode) {
      std::string time;
      if (xml_reader->ReadElementContent(&time))
        gdata::util::GetTimeFromString(time, &entry->published_time_);
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
DocumentEntry* DocumentEntry::CreateFromFileResource(const FileResource& file) {
  scoped_ptr<DocumentEntry> entry(new DocumentEntry());

  // DocumentEntry
  entry->resource_id_ = file.file_id();
  entry->id_ = file.file_id();
  entry->kind_ = file.GetKind();
  entry->title_ = UTF8ToUTF16(file.title());
  entry->published_time_ = file.created_date();
  // TODO(kochi): entry->labels_
  entry->content_.url_ = file.web_content_link();
  entry->content_.mime_type_ = file.mime_type();
  // TODO(kochi): entry->feed_links_

  // For file entries
  entry->filename_ = UTF8ToUTF16(file.title());
  entry->suggested_filename_ = UTF8ToUTF16(file.title());
  entry->file_md5_ = file.md5_checksum();
  entry->file_size_ = file.file_size();

  // If file is removed completely, that information is only available in
  // ChangeResource, and is reflected in |removed_|. If file is trashed, the
  // file entry still exists but with its "trashed" label true.
  entry->deleted_ = file.labels().is_trashed();

  // FeedEntry
  entry->etag_ = file.etag();
  // entry->authors_
  // entry->links_.
  if (!file.parents().empty()) {
    Link* link = new Link();
    link->type_ = Link::LINK_PARENT;
    link->href_ = file.parents()[0]->parent_link();
    entry->links_.push_back(link);
  }
  if (!file.self_link().is_empty()) {
    Link* link = new Link();
    link->type_ = Link::LINK_EDIT;
    link->href_ = file.self_link();
    entry->links_.push_back(link);
  }
  if (!file.thumbnail_link().is_empty()) {
    Link* link = new Link();
    link->type_ = Link::LINK_THUMBNAIL;
    link->href_ = file.thumbnail_link();
    entry->links_.push_back(link);
  }
  if (!file.alternate_link().is_empty()) {
    Link* link = new Link();
    link->type_ = Link::LINK_ALTERNATE;
    link->href_ = file.alternate_link();
    entry->links_.push_back(link);
  }
  if (!file.embed_link().is_empty()) {
    Link* link = new Link();
    link->type_ = Link::LINK_EMBED;
    link->href_ = file.embed_link();
    entry->links_.push_back(link);
  }
  // entry->categories_
  entry->updated_time_ = file.modified_by_me_date();

  entry->FillRemainingFields();
  return entry.release();
}

// static
DocumentEntry*
DocumentEntry::CreateFromChangeResource(const ChangeResource& change) {
  DocumentEntry* entry = CreateFromFileResource(change.file());

  // If |is_deleted()| returns true, the file is removed from Drive.
  entry->removed_ = change.is_deleted();

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
  converter->RegisterCustomField<int64>(
     kLargestChangestampField, &DocumentFeed::largest_changestamp_,
     &base::StringToInt64);
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
  const base::DictionaryValue* feed_dict = NULL;
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

// static
scoped_ptr<DocumentFeed> DocumentFeed::CreateFromChangeList(
    const ChangeList& changelist) {
  scoped_ptr<DocumentFeed> feed(new DocumentFeed());
  int64 largest_changestamp = 0;
  ScopedVector<ChangeResource>::const_iterator iter =
      changelist.items().begin();
  while (iter != changelist.items().end()) {
    const FileResource& file = (*iter)->file();
    largest_changestamp = std::max(largest_changestamp, (*iter)->change_id());
    feed->entries_.push_back(DocumentEntry::CreateFromFileResource(file));
    ++iter;
  }
  feed->largest_changestamp_ = largest_changestamp;
  return feed.Pass();
}

bool DocumentFeed::GetNextFeedURL(GURL* url) {
  DCHECK(url);
  for (size_t i = 0; i < links_.size(); ++i) {
    if (links_[i]->type() == Link::LINK_NEXT) {
      *url = links_[i]->href();
      return true;
    }
  }
  return false;
}

void DocumentFeed::ReleaseEntries(std::vector<DocumentEntry*>* entries) {
  entries_.release(entries);
}

////////////////////////////////////////////////////////////////////////////////
// InstalledApp implementation

InstalledApp::InstalledApp() : supports_create_(false) {
}

InstalledApp::~InstalledApp() {
}

InstalledApp::IconList InstalledApp::GetIconsForCategory(
    AppIcon::IconCategory category) const {
  IconList result;

  for (ScopedVector<AppIcon>::const_iterator icon_iter = app_icons_.begin();
       icon_iter != app_icons_.end(); ++icon_iter) {
    if ((*icon_iter)->category() != category)
      continue;
    GURL icon_url = (*icon_iter)->GetIconURL();
    if (icon_url.is_empty())
      continue;
    result.push_back(std::make_pair((*icon_iter)->icon_side_length(),
                                    icon_url));
  }

  // Return a sorted list, smallest to largest.
  std::sort(result.begin(), result.end(), SortBySize);
  return result;
}

GURL InstalledApp::GetProductUrl() const {
  for (ScopedVector<Link>::const_iterator it = links_.begin();
       it != links_.end(); ++it) {
    const Link* link = *it;
    if (link->type() == Link::LINK_PRODUCT)
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
  converter->RegisterRepeatedMessage(kInstalledAppIconField,
                                     &InstalledApp::app_icons_);
  converter->RegisterStringField(kInstalledAppIdField,
                                 &InstalledApp::app_id_);
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
  converter->RegisterCustomField<int64>(
      kLargestChangestampField,
      &AccountMetadataFeed::largest_changestamp_,
      &base::StringToInt64);
  converter->RegisterRepeatedMessage(kInstalledAppField,
                                     &AccountMetadataFeed::installed_apps_);
}

// static
scoped_ptr<AccountMetadataFeed> AccountMetadataFeed::CreateFrom(
    const base::Value& value) {
  scoped_ptr<AccountMetadataFeed> feed(new AccountMetadataFeed());
  const base::DictionaryValue* dictionary = NULL;
  const base::Value* entry = NULL;
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
