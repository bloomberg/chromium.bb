// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_parser.h"

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/string_number_conversions.h"
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

const struct {
  DocumentEntry::EntryKind kind;
  const char* entry;
} kEntryKindMap[] = {
    { DocumentEntry::ITEM,         "item" },
    { DocumentEntry::DOCUMENT,     "document"},
    { DocumentEntry::SPREADSHEET,  "spreadsheet" },
    { DocumentEntry::PRESENTATION, "presentation" },
    { DocumentEntry::FOLDER,       "folder"},
    { DocumentEntry::FILE,         "file"},
    { DocumentEntry::PDF,          "pdf"},
    { DocumentEntry::UNKNOWN,      NULL}
};

const struct {
  Link::LinkType type;
  const char* rel;
} kLinkTypeMap[] = {
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
    { Link::UNKNOWN,
      NULL}
};

const struct {
  FeedLink::FeedLinkType type;
  const char* rel;
} kFeedLinkTypeMap[] = {
    { FeedLink::ACL,
      "http://schemas.google.com/acl/2007#accessControlList" },
    { FeedLink::REVISIONS,
      "http://schemas.google.com/docs/2007/revisions" },
    { FeedLink::UNKNOWN,
      NULL}
};

const struct {
  Category::CategoryType type;
  const char* scheme;
} kCategoryTypeMap[] = {
    { Category::KIND,
      "http://schemas.google.com/g/2005#kind" },
    { Category::LABEL,
      "http://schemas.google.com/g/2005/labels" },
    { Category::UNKNOWN,
      NULL}
};

}  // namespace

const char Author::kNameField[] = "name.$t";
const char Author::kEmailField[] = "email.$t";

Author::Author() {
}

bool Author::Parse(const DictionaryValue* dictionary) {
  if (dictionary->GetString(kNameField, &name_) &&
      dictionary->GetString(kEmailField, &email_)) {
    return true;
  }
  return false;
}

const char Link::kHrefField[] = "href";
const char Link::kRelField[] = "rel";
const char Link::kTitleField[] = "title";
const char Link::kTypeField[] = "type";

Link::Link() : type_(Link::UNKNOWN) {
}

bool Link::Parse(const DictionaryValue* dictionary) {
  std::string rel;
  std::string href;
  if (dictionary->GetString(kRelField, &rel) &&
      dictionary->GetString(kHrefField, &href) &&
      dictionary->GetString(kTypeField, &mime_type_)) {
    href_ = GURL(href);
    type_ = GetLinkType(rel);
    if (type_ == Link::PARENT)
      dictionary->GetString(kTitleField, &title_);

    if (type_ != Link::UNKNOWN)
      return true;
  }
  return false;
}

// static.
Link::LinkType Link::GetLinkType(const std::string& rel) {
  for (size_t i = 0; kLinkTypeMap[i].rel; i++) {
    if (rel == kLinkTypeMap[i].rel)
      return kLinkTypeMap[i].type;
  }
  DVLOG(1) << "Unknown link type for rel " << rel;
  return Link::UNKNOWN;
}

const char FeedLink::kHrefField[] = "href";
const char FeedLink::kRelField[] = "rel";

FeedLink::FeedLink() : type_(FeedLink::UNKNOWN) {
}

bool FeedLink::Parse(const DictionaryValue* dictionary) {
  std::string rel;
  std::string href;
  if (dictionary->GetString(kRelField, &rel) &&
      dictionary->GetString(kHrefField, &href)) {
    href_ = GURL(href);
    type_ = GetFeedLinkType(rel);
    if (type_ != FeedLink::UNKNOWN)
      return true;
  }
  return false;
}

// static.
FeedLink::FeedLinkType FeedLink::GetFeedLinkType(const std::string& rel) {
  for (size_t i = 0; kFeedLinkTypeMap[i].rel; i++) {
    if (rel == kFeedLinkTypeMap[i].rel)
      return kFeedLinkTypeMap[i].type;
  }
  DVLOG(1) << "Unknown feed link type for rel " << rel;
  return FeedLink::UNKNOWN;
}

const char Category::kLabelField[] = "label";
const char Category::kSchemeField[] = "scheme";
const char Category::kTermField[] = "term";

Category::Category() {
}

bool Category::Parse(const DictionaryValue* dictionary) {
  std::string scheme;
  if (dictionary->GetString(kLabelField, &label_) &&
      dictionary->GetString(kSchemeField, &scheme) &&
      dictionary->GetString(kTermField, &term_)) {
    type_ = GetCategoryTypeFromScheme(scheme);
    if (type_ != Category::UNKNOWN)
      return true;

    DVLOG(1) << "Unknown category:"
             << "\n  label = " << label_
             << "\n  scheme = " << scheme
             << "\n  term = " << term_;
  }
  return false;
}

// Converts category.scheme into CategoryType enum.
Category::CategoryType Category::GetCategoryTypeFromScheme(
    const std::string& scheme) {
  for (size_t i = 0; kCategoryTypeMap[i].scheme; i++) {
    if (scheme == kCategoryTypeMap[i].scheme)
      return kCategoryTypeMap[i].type;
  }
  DVLOG(1) << "Unknown feed link type for scheme " << scheme;
  return Category::UNKNOWN;
}


const Link* GDataEntry::GetLinkByType(Link::LinkType type) const {
  for (ScopedVector<Link>::const_iterator iter = links_.begin();
       iter != links_.end(); ++iter) {
    if ((*iter)->type() == type)
      return (*iter);
  }
  return NULL;
}

const char GDataEntry::kTimeParsingDelimiters[] = "-:.TZ";
const char GDataEntry::kAuthorField[] = "author";
const char GDataEntry::kLinkField[] = "link";
const char GDataEntry::kCategoryField[] = "category";

GDataEntry::GDataEntry() {
}

GDataEntry::~GDataEntry() {
}

bool GDataEntry::ParseAuthors(const DictionaryValue* value_dict) {
  ListValue* authors = NULL;
  if (value_dict->GetList(kAuthorField, &authors)) {
    for (ListValue::iterator iter = authors->begin();
         iter != authors->end();
         ++iter) {
      if ((*iter)->GetType() != Value::TYPE_DICTIONARY) {
        DVLOG(1) << "Invalid author list element";
        return false;
      }
      DictionaryValue* author_dict =
          reinterpret_cast<DictionaryValue*>(*iter);
      scoped_ptr<Author> author(new Author());
      if (author->Parse(author_dict)) {
        authors_.push_back(author.release());
      } else {
        DVLOG(1) << "Invalid author etag = " << etag_;
      }
    }
  }
  return true;
}

bool GDataEntry::ParseLinks(const DictionaryValue* value_dict) {
  ListValue* links = NULL;
  if (value_dict->GetList(kLinkField, &links)) {
    for (ListValue::iterator iter = links->begin();
         iter != links->end();
         ++iter) {
      if ((*iter)->GetType() != Value::TYPE_DICTIONARY) {
        DVLOG(1) << "Invalid link list element";
        return false;
      }
      DictionaryValue* link_dict =
          reinterpret_cast<DictionaryValue*>(*iter);
      scoped_ptr<Link> link(new Link());
      if (link->Parse(link_dict))
        links_.push_back(link.release());
      else
        DVLOG(1) << "Invalid link etag = " << etag_;
    }
  }
  return true;
}

// static.
bool GDataEntry::GetTimeFromString(const std::string& raw_value,
                                   base::Time* time) {
  std::vector<std::string> parts;
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

bool GDataEntry::ParseCategories(const DictionaryValue* value_dict) {
  ListValue* categories = NULL;
  if (value_dict->GetList(kCategoryField, &categories)) {
    for (ListValue::iterator iter = categories->begin();
         iter != categories->end();
         ++iter) {
      if ((*iter)->GetType() != Value::TYPE_DICTIONARY) {
        DVLOG(1) << "Invalid category list element";
        return false;
      }
      DictionaryValue* category_dict =
          reinterpret_cast<DictionaryValue*>(*iter);
      scoped_ptr<Category> category(new Category());
      if (category->Parse(category_dict)) {
        OnAddCategory(category.get());
        categories_->push_back(category.release());
      } else {
        DVLOG(1) << "Invalid category etag = " << etag_;
      }
    }
  }
  return true;
}

// static.
bool GDataEntry::ParseDateTime(const DictionaryValue* dict,
                               const std::string& field,
                               base::Time* time) {
  std::string raw_value;
  if (!dict->GetString(field, &raw_value))
    return false;

  return GetTimeFromString(raw_value, time);
}

const char DocumentEntry::kFeedLinkField[] = "gd$feedLink";
const char DocumentEntry::kContentField[] = "content";
const char DocumentEntry::kSrcField[] = "src";
const char DocumentEntry::kTypeField[] = "type";
const char DocumentEntry::kFileNameField[] = "docs$filename.$t";
const char DocumentEntry::kMD5Field[] = "docs$md5Checksum.$t";
const char DocumentEntry::kSizeField[] = "docs$size.$t";
const char DocumentEntry::kSuggestedFileNameField[] =
    "docs$suggestedFilename.$t";
const char DocumentEntry::kETagField[] = "gd$etag";
const char DocumentEntry::kResourceIdField[] = "gd$resourceId.$t";
const char DocumentEntry::kIDField[] = "id.$t";
const char DocumentEntry::kTitleField[] = "title.$t";
const char DocumentEntry::kUpdatedField[] = "updated.$t";
const char DocumentEntry::kPublishedField[] = "published.$t";

DocumentEntry::DocumentEntry() : kind_(DocumentEntry::UNKNOWN), file_size_(0) {
}

DocumentEntry::~DocumentEntry() {
}

bool DocumentEntry::ParseFeedLinks(const DictionaryValue* value_dict) {
  ListValue* links = NULL;
  if (value_dict->GetList(kFeedLinkField, &links)) {
    for (ListValue::iterator iter = links->begin();
         iter != links->end();
         ++iter) {
      if ((*iter)->GetType() != Value::TYPE_DICTIONARY) {
        DVLOG(1) << "Invalid feed link list element";
        return false;
      }
      DictionaryValue* link_dict =
          reinterpret_cast<DictionaryValue*>(*iter);
      scoped_ptr<FeedLink> link(new FeedLink());
      if (link->Parse(link_dict)) {
        feed_links_.push_back(link.release());
      } else {
        DVLOG(1) << "Invalid feed link etag = " << etag_;
      }
    }
  }
  return true;
}

bool DocumentEntry::ParseContent(const DictionaryValue* value_dict) {
  base::DictionaryValue* content = NULL;
  if (value_dict->GetDictionary(kContentField, &content)) {
    std::string src;
    if (content->GetString(kSrcField, &src) &&
        content->GetString(kTypeField, &content_mime_type_)) {
      content_url_ = GURL(src);
      return true;
    }
  }
  DVLOG(1) << "Invalid item content etag = " << etag_;
  return false;
}

bool DocumentEntry::ParseFileProperties(const DictionaryValue* value_dict) {
  if (!value_dict->GetString(kFileNameField, &filename_)) {
    DVLOG(1) << "File item with no name! " << etag_;
    return false;
  }

  if (!value_dict->GetString(kMD5Field, &file_md5_)) {
    DVLOG(1) << "File item with no md5! " << etag_;
    return false;
  }

  std::string file_size;
  if (!value_dict->GetString(kSizeField, &file_size) ||
      !file_size.length()) {
    DVLOG(1) << "File item with no size! " << etag_;
    return false;
  }

  if (!base::StringToInt64(file_size, &file_size_)) {
    DVLOG(1) << "Invalid file size '" << file_size << "' for " << etag_;
    return false;
  }

  if (!value_dict->GetString(kSuggestedFileNameField, &suggested_filename_))
    DVLOG(1) << "File item with no docs$suggestedFilename! " << etag_;

  return true;
}

bool DocumentEntry::Parse(const DictionaryValue* value_dict) {
  if (!value_dict->GetString(kETagField, &etag_)) {
    DVLOG(1) << "Item with no etag!";
    return false;
  }

  if (!value_dict->GetString(kResourceIdField, &resource_id_)) {
    DVLOG(1) << "Item with no resource id! " << etag_;
    return false;
  }

  if (!value_dict->GetString(kIDField, &id_)) {
    DVLOG(1) << "Item with no id! " << etag_;
    return false;
  }

  if (!value_dict->GetString(kTitleField, &title_)) {
    DVLOG(1) << "Item with no title! " << etag_;
    return false;
  }

  if (!ParseDateTime(value_dict, kUpdatedField, &updated_time_)) {
    DVLOG(1) << "Item with no updated date! " << etag_;
    return false;
  }

  if (!ParseDateTime(value_dict, kPublishedField, &published_time_)) {
    DVLOG(1) << "Item with no published date! " << etag_;
    return false;
  }

  // Parse categories, will set up entry->kind as well.
  if (!ParseCategories(value_dict))
    return false;

  if (kind_ == DocumentEntry::FILE || kind_ == DocumentEntry::PDF) {
    if (!ParseFileProperties(value_dict))
      return false;
  }

  if (!ParseAuthors(value_dict))
    return false;

  if (!ParseContent(value_dict))
    return false;

  if (!ParseLinks(value_dict))
    return false;

  if (!ParseFeedLinks(value_dict))
    return false;

  return true;
}

// static.
DocumentEntry::EntryKind DocumentEntry::GetEntryKindFromTerm(
    const std::string& term) {
  if (!StartsWithASCII(term, kTermPrefix, false)) {
    DVLOG(1) << "Unexpected term prefix term " << term;
    return DocumentEntry::UNKNOWN;
  }

  std::string type = term.substr(strlen(kTermPrefix));
  for (size_t i = 0; kEntryKindMap[i].entry; i++) {
    if (type == kEntryKindMap[i].entry)
      return kEntryKindMap[i].kind;
  }
  DVLOG(1) << "Unknown entry type for term " << term << ", type " << type;
  return DocumentEntry::UNKNOWN;
}

void DocumentEntry::OnAddCategory(Category* category) {
  if (category->type() == Category::KIND)
    kind_ = GetEntryKindFromTerm(category->term());
  else if (category->type() == Category::LABEL)
    labels_.push_back(category->label());
}


const char DocumentFeed::kETagField[] = "gd$etag";
const char DocumentFeed::kStartIndexField[] = "openSearch$startIndex.$t";
const char DocumentFeed::kItemsPerPageField[] =
    "openSearch$itemsPerPage.$t";
const char DocumentFeed::kUpdatedField[] = "updated.$t";
const char DocumentFeed::kTitleField[] = "title.$t";
const char DocumentFeed::kEntryField[] = "entry";

DocumentFeed::DocumentFeed() : start_index_(0), items_per_page_(0) {
}

DocumentFeed::~DocumentFeed() {
}

bool DocumentFeed::Parse(const DictionaryValue* value_dict) {
  if (!value_dict->GetString(kETagField, &etag_)) {
    DVLOG(1) << "Feed with no etag!";
    return false;
  }

  // TODO(zelidrag): Once we figure out where these will be used, we should
  // check for valid start_index_ and items_per_page_ values.
  std::string start_index;
  if (!value_dict->GetString(kStartIndexField, &start_index) ||
      !base::StringToInt(start_index, &start_index_)) {
    DVLOG(1) << "Feed with no startIndex! " << etag_;
    return false;
  }

  std::string items_per_page;
  if (!value_dict->GetString(kItemsPerPageField, &items_per_page) ||
      !base::StringToInt(items_per_page, &items_per_page_)) {
    DVLOG(1) << "Feed with no itemsPerPage! " << etag_;
    return false;
  }

  if (!ParseDateTime(value_dict, kUpdatedField, &updated_time_)) {
    DVLOG(1) << "Feed with no updated date! " << etag_;
    return false;
  }

  if (!value_dict->GetString(kTitleField, &title_)) {
    DVLOG(1) << "Feed with no title!";
    return false;
  }

  ListValue* entries = NULL;
  if (value_dict->GetList(kEntryField, &entries)) {
    for (ListValue::iterator iter = entries->begin();
         iter != entries->end();
         ++iter) {
      DocumentEntry* entry = DocumentEntry::CreateFrom(*iter);
      if (entry)
        entries_.push_back(entry);
    }
  }

  // Parse categories.
  if (!ParseCategories(value_dict))
    return false;

  // Parse author list.
  if (!ParseAuthors(value_dict))
    return false;

  if (!ParseLinks(value_dict))
    return false;

  return true;
}

// static.
DocumentEntry* DocumentEntry::CreateFrom(base::Value* value) {
  if (!value || value->GetType() != Value::TYPE_DICTIONARY)
    return NULL;

  DictionaryValue* root = reinterpret_cast<DictionaryValue*>(value);
  scoped_ptr<DocumentEntry> entry(new DocumentEntry());
  if (!entry->Parse(root)) {
    DVLOG(1) << "Invalid document entry!";
    return NULL;
  }

  return entry.release();
}


// static.
DocumentFeed* DocumentFeed::CreateFrom(base::Value* value) {
  if (!value || value->GetType() != Value::TYPE_DICTIONARY)
    return NULL;

  DictionaryValue* root_entry_dict =
      reinterpret_cast<DictionaryValue*>(value);
  scoped_ptr<DocumentFeed> feed(new DocumentFeed());
  if (!feed->Parse(root_entry_dict)) {
    DVLOG(1) << "Invalid document feed!";
    return NULL;
  }

  return feed.release();
}

bool DocumentFeed::GetNextFeedURL(GURL* url) {
  DCHECK(url);
  for (ScopedVector<Link>::iterator iter = links_.begin();
      iter != links_.end(); ++iter) {
    if ((*iter)->type() == Link::NEXT) {
      *url = (*iter)->href();
      return true;
    }
  }
  return false;
}

}  // namespace gdata
