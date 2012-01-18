// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_parser.h"

#include "base/basictypes.h"
#include "base/json/json_value_converter.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/values.h"

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

struct EntryKindMap {
  DocumentEntry::EntryKind kind;
  const char* entry;
};

const EntryKindMap kEntryKindMap[] = {
    { DocumentEntry::ITEM,         "item" },
    { DocumentEntry::DOCUMENT,     "document"},
    { DocumentEntry::SPREADSHEET,  "spreadsheet" },
    { DocumentEntry::PRESENTATION, "presentation" },
    { DocumentEntry::FOLDER,       "folder"},
    { DocumentEntry::FILE,         "file"},
    { DocumentEntry::PDF,          "pdf"},
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

}  // namespace

const char Author::kNameField[] = "name.$t";
const char Author::kEmailField[] = "email.$t";

Author::Author() {
}

// static
void Author::RegisterJSONConverter(
    base::JSONValueConverter<Author>* converter) {
  converter->RegisterStringField(kNameField, &Author::name_);
  converter->RegisterStringField(kEmailField, &Author::email_);
}

const char Link::kHrefField[] = "href";
const char Link::kRelField[] = "rel";
const char Link::kTitleField[] = "title";
const char Link::kTypeField[] = "type";

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
  DVLOG(1) << "Unknown link type for rel " << rel;
  return false;
}

// static
void Link::RegisterJSONConverter(base::JSONValueConverter<Link>* converter) {
  converter->RegisterCustomField<Link::LinkType>(
      kRelField, &Link::type_, &Link::GetLinkType);
  converter->RegisterCustomField(kHrefField, &Link::href_, &GetGURLFromString);
  converter->RegisterStringField(kTitleField, &Link::title_);
  converter->RegisterStringField(kTypeField, &Link::mime_type_);
}

const char FeedLink::kHrefField[] = "href";
const char FeedLink::kRelField[] = "rel";

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

const char Category::kLabelField[] = "label";
const char Category::kSchemeField[] = "scheme";
const char Category::kTermField[] = "term";

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

const Link* GDataEntry::GetLinkByType(Link::LinkType type) const {
  for (size_t i = 0; i < links_.size(); ++i) {
    if (links_[i]->type() == type)
      return links_[i];
  }
  return NULL;
}

const char Content::kSrcField[] = "src";
const char Content::kTypeField[] = "type";

Content::Content() {
}

// static
void Content::RegisterJSONConverter(
    base::JSONValueConverter<Content>* converter) {
  converter->RegisterCustomField(kSrcField, &Content::url_, &GetGURLFromString);
  converter->RegisterStringField(kTypeField, &Content::mime_type_);
}

const char GDataEntry::kTimeParsingDelimiters[] = "-:.TZ";
const char GDataEntry::kAuthorField[] = "author";
const char GDataEntry::kLinkField[] = "link";
const char GDataEntry::kCategoryField[] = "category";
const char GDataEntry::kETagField[] = "gd$etag";
const char GDataEntry::kUpdatedField[] = "updated.$t";

GDataEntry::GDataEntry() {
}

GDataEntry::~GDataEntry() {
}

// static
void GDataEntry::RegisterJSONConverter(
    base::JSONValueConverter<GDataEntry>* converter) {
  converter->RegisterStringField(kETagField, &GDataEntry::etag_);
  converter->RegisterRepeatedMessage(kAuthorField, &GDataEntry::authors_);
  converter->RegisterRepeatedMessage(kLinkField, &GDataEntry::links_);
  converter->RegisterRepeatedMessage(kCategoryField, &GDataEntry::categories_);
  converter->RegisterCustomField<base::Time>(
      kUpdatedField,
      &GDataEntry::updated_time_,
      &GDataEntry::GetTimeFromString);
}

// static
bool GDataEntry::GetTimeFromString(const base::StringPiece& raw_value,
                                   base::Time* time) {
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

const char DocumentEntry::kFeedLinkField[] = "gd$feedLink";
const char DocumentEntry::kContentField[] = "content";
const char DocumentEntry::kFileNameField[] = "docs$filename.$t";
const char DocumentEntry::kMD5Field[] = "docs$md5Checksum.$t";
const char DocumentEntry::kSizeField[] = "docs$size.$t";
const char DocumentEntry::kSuggestedFileNameField[] =
    "docs$suggestedFilename.$t";
const char DocumentEntry::kResourceIdField[] = "gd$resourceId.$t";
const char DocumentEntry::kIDField[] = "id.$t";
const char DocumentEntry::kTitleField[] = "title.$t";
const char DocumentEntry::kPublishedField[] = "published.$t";

DocumentEntry::DocumentEntry() : kind_(DocumentEntry::UNKNOWN), file_size_(0) {
}

DocumentEntry::~DocumentEntry() {
}

// static
void DocumentEntry::RegisterJSONConverter(
    base::JSONValueConverter<DocumentEntry>* converter) {
  // inheritant the parent registrations.
  GDataEntry::RegisterJSONConverter(
      reinterpret_cast<base::JSONValueConverter<GDataEntry>*>(converter));
  converter->RegisterStringField(
      kResourceIdField, &DocumentEntry::resource_id_);
  converter->RegisterStringField(kIDField, &DocumentEntry::id_);
  converter->RegisterStringField(kTitleField, &DocumentEntry::title_);
  converter->RegisterCustomField<base::Time>(
      kPublishedField, &DocumentEntry::published_time_,
      &GDataEntry::GetTimeFromString);
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

const char DocumentFeed::kStartIndexField[] = "openSearch$startIndex.$t";
const char DocumentFeed::kItemsPerPageField[] =
    "openSearch$itemsPerPage.$t";
const char DocumentFeed::kTitleField[] = "title.$t";
const char DocumentFeed::kEntryField[] = "entry";

DocumentFeed::DocumentFeed() : start_index_(0), items_per_page_(0) {
}

DocumentFeed::~DocumentFeed() {
}

// static
void DocumentFeed::RegisterJSONConverter(
    base::JSONValueConverter<DocumentFeed>* converter) {
  // inheritance
  GDataEntry::RegisterJSONConverter(
      reinterpret_cast<base::JSONValueConverter<GDataEntry>*>(converter));
  // TODO(zelidrag): Once we figure out where these will be used, we should
  // check for valid start_index_ and items_per_page_ values.
  converter->RegisterCustomField<int>(
      kStartIndexField, &DocumentFeed::start_index_, &base::StringToInt);
  converter->RegisterCustomField<int>(
      kItemsPerPageField, &DocumentFeed::items_per_page_, &base::StringToInt);
  converter->RegisterStringField(kTitleField, &DocumentFeed::title_);
  converter->RegisterRepeatedMessage(kEntryField, &DocumentFeed::entries_);
}

// static
DocumentEntry* DocumentEntry::CreateFrom(base::Value* value) {
  base::JSONValueConverter<DocumentEntry> converter;
  scoped_ptr<DocumentEntry> entry(new DocumentEntry());
  if (!converter.Convert(*value, entry.get())) {
    DVLOG(1) << "Invalid document entry!";
    return NULL;
  }

  entry->FillRemainingFields();
  return entry.release();
}


bool DocumentFeed::Parse(base::Value* value) {
  base::JSONValueConverter<DocumentFeed> converter;
  if (!converter.Convert(*value, this)) {
    DVLOG(1) << "Invalid document feed!";
    return false;
  }

  for (size_t i = 0; i < entries_.size(); ++i) {
    entries_[i]->FillRemainingFields();
  }
  return true;
}

// static
DocumentFeed* DocumentFeed::CreateFrom(base::Value* value) {
  scoped_ptr<DocumentFeed> feed(new DocumentFeed());
  if (!feed->Parse(value)) {
    DVLOG(1) << "Invalid document feed!";
    return NULL;
  }

  return feed.release();
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

}  // namespace gdata
