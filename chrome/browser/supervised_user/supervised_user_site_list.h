// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SITE_LIST_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SITE_LIST_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/time/time.h"

class Profile;

namespace base {
class DictionaryValue;
class ListValue;
class Value;
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
class SupervisedUserSiteList
    : public base::RefCountedThreadSafe<SupervisedUserSiteList> {
 public:
  typedef base::Callback<void(const scoped_refptr<SupervisedUserSiteList>&)>
      LoadedCallback;

  struct Site {
    explicit Site(const base::string16& name);
    ~Site();

    // The human-readable name for the site.
    base::string16 name;

    // A list of URL patterns that should be whitelisted for the site.
    std::vector<std::string> patterns;

    // A list of SHA1 hashes of hostnames that should be whitelisted
    // for the site.
    std::vector<std::string> hostname_hashes;

    // Copying and assignment is allowed.
  };

  // Asynchronously loads the site list from |file| and calls |callback| with
  // the newly created object.
  static void Load(const base::FilePath& file, const LoadedCallback& callback);

  // Sets whether the site list should be loaded in-process or out-of-process.
  // In-process loading should only be used in tests (to avoid having to set up
  // child process handling).
  static void SetLoadInProcessForTesting(bool in_process);

  // Returns a list of all sites in this site list.
  const std::vector<Site>& sites() const { return sites_; }

 private:
  friend class base::RefCountedThreadSafe<SupervisedUserSiteList>;

  explicit SupervisedUserSiteList(const base::ListValue& sites);
  ~SupervisedUserSiteList();

  // Static private so they can access the private constructor.
  static void ParseJson(const base::FilePath& path,
                        const SupervisedUserSiteList::LoadedCallback& callback,
                        const std::string& json);
  static void OnJsonParseSucceeded(
      const base::FilePath& path,
      base::TimeTicks start_time,
      const SupervisedUserSiteList::LoadedCallback& callback,
      scoped_ptr<base::Value> value);

  std::vector<Site> sites_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserSiteList);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SITE_LIST_H_
