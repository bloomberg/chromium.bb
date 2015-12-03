// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SITE_LIST_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SITE_LIST_H_

#include <array>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/sha1.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "url/gurl.h"

class Profile;

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

// This class represents the content of a supervised user whitelist. It is
// loaded from a JSON file inside the extension bundle, which defines the sites
// on the list.
// All whitelists are combined in the SupervisedUserURLFilter, which can tell
// for a given URL if it is part of any whitelist. Effectively,
// SupervisedUserURLFilter then acts as a big whitelist which is the union of
// all the whitelists.
class SupervisedUserSiteList
    : public base::RefCountedThreadSafe<SupervisedUserSiteList> {
 public:
  class HostnameHash {
   public:
    explicit HostnameHash(const std::string& hostname);
    // |bytes| must have a size of at least |base::kSHA1Length|.
    explicit HostnameHash(const std::vector<uint8>& bytes);

    bool operator==(const HostnameHash& rhs) const;

    // Returns a hash code suitable for putting this into hash maps.
    size_t hash() const;

   private:
    std::array<uint8, base::kSHA1Length> bytes_;
    // Copy and assign are allowed.
  };

  using LoadedCallback =
      base::Callback<void(const scoped_refptr<SupervisedUserSiteList>&)>;

  // Asynchronously loads the site list from |file| and calls |callback| with
  // the newly created object.
  static void Load(const base::string16& title,
                   const base::FilePath& file,
                   const LoadedCallback& callback);

  const base::string16& title() const { return title_; }
  const GURL& entry_point() const { return entry_point_; }
  const std::vector<std::string>& patterns() const { return patterns_; }
  const std::vector<HostnameHash>& hostname_hashes() const {
    return hostname_hashes_;
  }

 private:
  friend class base::RefCountedThreadSafe<SupervisedUserSiteList>;

  SupervisedUserSiteList(const base::string16& title,
                         const GURL& entry_point,
                         const base::ListValue* patterns,
                         const base::ListValue* hostname_hashes);
  ~SupervisedUserSiteList();

  // Static private so it can access the private constructor.
  static void OnJsonLoaded(
      const base::string16& title,
      const base::FilePath& path,
      base::TimeTicks start_time,
      const SupervisedUserSiteList::LoadedCallback& callback,
      scoped_ptr<base::Value> value);

  base::string16 title_;
  GURL entry_point_;

  // A list of URL patterns that should be whitelisted.
  std::vector<std::string> patterns_;

  // A list of SHA1 hashes of hostnames that should be whitelisted.
  std::vector<HostnameHash> hostname_hashes_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserSiteList);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SITE_LIST_H_
