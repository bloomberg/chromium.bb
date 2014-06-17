// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SITE_LIST_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SITE_LIST_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"

class Profile;

namespace base {
class DictionaryValue;
class ListValue;
}

// This class represents a "site list" that is part of a content pack. It is
// loaded from a JSON file inside the extension bundle, which defines the sites
// on the list.
// Every site has -- among other attributes -- a whitelist of URLs that are
// required to use it. All sites from all installed content packs together with
// their respective whitelists are combined in the SupervisedUserURLFilter,
// which can tell for a given URL if it is part of the whitelist for any site.
// Effectively, SupervisedUserURLFilter then acts as a big whitelist which is
// the union of the whitelists in all sites in all content packs. See
// http://goo.gl/cBCB8 for a diagram.
class SupervisedUserSiteList {
 public:
  struct Site {
    Site(const base::string16& name, int category_id);
    ~Site();

    // The human-readable name for the site.
    base::string16 name;

    // An identifier for the category. Categories are hardcoded and start with
    // 1, but apart from the offset correspond to the return values from
    // GetCategoryNames() below.
    int category_id;

    // A list of URL patterns that should be whitelisted for the site.
    std::vector<std::string> patterns;

    // A list of SHA1 hashes of hostnames that should be whitelisted
    // for the site.
    std::vector<std::string> hostname_hashes;
  };

  SupervisedUserSiteList(const std::string& extension_id,
                         const base::FilePath& path);
  ~SupervisedUserSiteList();

  // Creates a copy of the site list.
  // Caller takes ownership of the returned value.
  SupervisedUserSiteList* Clone();

  // Returns a list of all categories.
  // TODO(bauerb): The list is hardcoded for now, but if we allow custom
  // categories, this should live in some registry.
  static void GetCategoryNames(std::vector<base::string16>* categories);

  // Returns a list of all sites in this site list.
  void GetSites(std::vector<Site>* sites);

 private:
  bool LazyLoad();
  void CopyThumbnailUrl(const base::DictionaryValue* source,
                        base::DictionaryValue* dest);

  std::string extension_id_;
  base::FilePath path_;
  scoped_ptr<base::DictionaryValue> categories_;
  scoped_ptr<base::ListValue> sites_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserSiteList);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SITE_LIST_H_
