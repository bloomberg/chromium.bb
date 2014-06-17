// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_site_list.h"

#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/extension.h"

using base::DictionaryValue;
using base::ListValue;
using base::Value;

const int kSitelistFormatVersion = 1;

const char kCategoriesKey[] = "categories";
const char kHostnameHashesKey[] = "hostname_hashes";
const char kNameKey[] = "name";
const char kSitesKey[] = "sites";
const char kSitelistFormatVersionKey[] = "version";
const char kThumbnailKey[] = "thumbnail";
const char kThumbnailUrlKey[] = "thumbnail_url";
const char kUrlKey[] = "url";
const char kWhitelistKey[] = "whitelist";

namespace {

struct CategoryInfo {
  const char* identifier;
  const char* name;
};

// These are placeholders for now.
CategoryInfo g_categories[] = {
  {"com.google.chrome.animals", "Animals and Plants"},
  {"com.google.chrome.arts", "Arts"},
  {"com.google.chrome.business", "Business"},
  {"com.google.chrome.computers", "Computers"},
  {"com.google.chrome.education", "Education"},
  {"com.google.chrome.entertainment", "Entertainment"},
  {"com.google.chrome.games", "Games"},
  {"com.google.chrome.health", "Health"},
  {"com.google.chrome.home", "Home"},
  {"com.google.chrome.international", "International"},
  {"com.google.chrome.news", "News"},
  {"com.google.chrome.people", "People and Society"},
  {"com.google.chrome.places", "Places"},
  {"com.google.chrome.pre-school", "Pre-School"},
  {"com.google.chrome.reference", "Reference"},
  {"com.google.chrome.science", "Science"},
  {"com.google.chrome.shopping", "Shopping"},
  {"com.google.chrome.sports", "Sports and Hobbies"},
  {"com.google.chrome.teens", "Teens"}
};

// Category 0 is "not listed"; actual category IDs start at 1.
int GetCategoryId(const std::string& category) {
  for (size_t i = 0; i < arraysize(g_categories); ++i) {
    if (g_categories[i].identifier == category)
      return i + 1;
  }
  return 0;
}

// Takes a DictionaryValue entry from the JSON file and fills the whitelist
// (via URL patterns or hostname hashes) and the URL in the corresponding Site
// struct.
void AddWhitelistEntries(const base::DictionaryValue* site_dict,
                         SupervisedUserSiteList::Site* site) {
  std::vector<std::string>* patterns = &site->patterns;

  bool found = false;
  const base::ListValue* whitelist = NULL;
  if (site_dict->GetList(kWhitelistKey, &whitelist)) {
    found = true;
    for (base::ListValue::const_iterator whitelist_it = whitelist->begin();
         whitelist_it != whitelist->end(); ++whitelist_it) {
      std::string pattern;
      if (!(*whitelist_it)->GetAsString(&pattern)) {
        LOG(ERROR) << "Invalid whitelist entry";
        continue;
      }

      patterns->push_back(pattern);
    }
  }

  std::vector<std::string>* hashes = &site->hostname_hashes;
  const base::ListValue* hash_list = NULL;
  if (site_dict->GetList(kHostnameHashesKey, &hash_list)) {
    found = true;
    for (base::ListValue::const_iterator hash_list_it = hash_list->begin();
         hash_list_it != hash_list->end(); ++hash_list_it) {
      std::string hash;
      if (!(*hash_list_it)->GetAsString(&hash)) {
        LOG(ERROR) << "Invalid whitelist entry";
        continue;
      }

      hashes->push_back(hash);
    }
  }

  if (found)
    return;

  // Fall back to using a whitelist based on the URL.
  std::string url_str;
  if (!site_dict->GetString(kUrlKey, &url_str)) {
    LOG(ERROR) << "Whitelist is invalid";
    return;
  }

  GURL url(url_str);
  if (!url.is_valid()) {
    LOG(ERROR) << "URL " << url_str << " is invalid";
    return;
  }

  patterns->push_back(url.host());
}

}  // namespace

SupervisedUserSiteList::Site::Site(const base::string16& name,
                                   int category_id)
    : name(name),
      category_id(category_id) {}

SupervisedUserSiteList::Site::~Site() {}

SupervisedUserSiteList::SupervisedUserSiteList(
    const std::string& extension_id,
    const base::FilePath& path)
    : extension_id_(extension_id),
      path_(path) {
}

SupervisedUserSiteList::~SupervisedUserSiteList() {
}

SupervisedUserSiteList* SupervisedUserSiteList::Clone() {
  return new SupervisedUserSiteList(extension_id_, path_);
}

// static
void SupervisedUserSiteList::GetCategoryNames(
    std::vector<base::string16>* categories) {
  // TODO(bauerb): Collect custom categories from extensions.
  for (size_t i = 0; i < arraysize(g_categories); ++i) {
    categories->push_back(base::ASCIIToUTF16(g_categories[i].name));
  }
}

void SupervisedUserSiteList::GetSites(std::vector<Site>* sites) {
  if (!LazyLoad())
    return;

  for (base::ListValue::iterator entry_it = sites_->begin();
       entry_it != sites_->end(); ++entry_it) {
    base::DictionaryValue* entry = NULL;
    if (!(*entry_it)->GetAsDictionary(&entry)) {
      LOG(ERROR) << "Entry is invalid";
      continue;
    }

    base::string16 name;
    entry->GetString(kNameKey, &name);

    // TODO(bauerb): We need to distinguish between "no category assigned" and
    // "not on any site list".
    int category_id = 0;
    const base::ListValue* categories = NULL;
    if (entry->GetList(kCategoriesKey, &categories)) {
      for (base::ListValue::const_iterator it = categories->begin();
           it != categories->end(); ++it) {
        std::string category;
        if (!(*it)->GetAsString(&category)) {
          LOG(ERROR) << "Invalid category";
          continue;
        }
        category_id = GetCategoryId(category);
        break;
      }
    }
    sites->push_back(Site(name, category_id));
    AddWhitelistEntries(entry, &sites->back());
  }
}

bool SupervisedUserSiteList::LazyLoad() {
  if (sites_.get())
    return true;

  JSONFileValueSerializer serializer(path_);
  std::string error;
  scoped_ptr<base::Value> value(serializer.Deserialize(NULL, &error));
  if (!value.get()) {
    LOG(ERROR) << "Couldn't load site list " << path_.value() << ": "
               << error;
    return false;
  }

  base::DictionaryValue* dict = NULL;
  if (!value->GetAsDictionary(&dict)) {
    LOG(ERROR) << "Site list " << path_.value() << " is invalid";
    return false;
  }

  int version = 0;
  if (!dict->GetInteger(kSitelistFormatVersionKey, &version)) {
    LOG(ERROR) << "Site list " << path_.value() << " has invalid version";
    return false;
  }

  if (version > kSitelistFormatVersion) {
    LOG(ERROR) << "Site list " << path_.value() << " has a too new format";
    return false;
  }

  base::ListValue* sites = NULL;
  if (dict->GetList(kSitesKey, &sites))
    sites_.reset(sites->DeepCopy());

  base::DictionaryValue* categories = NULL;
  if (dict->GetDictionary(kCategoriesKey, &categories))
    categories_.reset(categories->DeepCopy());

  return true;
}

void SupervisedUserSiteList::CopyThumbnailUrl(
    const base::DictionaryValue* source,
    base::DictionaryValue* dest) {
  if (!source->HasKey(kThumbnailKey))
    return;

  std::string thumbnail;
  if (!source->GetString(kThumbnailKey, &thumbnail)) {
    LOG(ERROR) << "Invalid thumbnail";
    return;
  }

  GURL base_url =
      extensions::Extension::GetBaseURLFromExtensionId(extension_id_);
  GURL thumbnail_url = base_url.Resolve(thumbnail);
  if (!thumbnail_url.is_valid()) {
    LOG(ERROR) << "Invalid thumbnail";
    return;
  }

  dest->SetString(kThumbnailUrlKey, thumbnail_url.spec());
}
