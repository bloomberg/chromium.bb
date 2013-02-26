// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_RESOURCE_CACHE_H_
#define CHROME_BROWSER_POLICY_RESOURCE_CACHE_H_

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"

namespace base {
class FilePath;
}

namespace leveldb {
class DB;
}

namespace policy {

// Manages storage of data at a given path. The data is keyed by a key and
// a subkey, and can be queried by (key, subkey) or (key) lookups.
// The contents of the cache have to be manually cleared using Delete() or
// PurgeOtherSubkeys().
// Instances of this class can be created on any thread, but from then on must
// be always used from the same thread, and it must support file I/O.
class ResourceCache : public base::NonThreadSafe {
 public:
  explicit ResourceCache(const base::FilePath& cache_path);
  ~ResourceCache();

  // Returns true if the underlying database was opened, and false otherwise.
  // When this returns false then all the other operations will fail.
  bool IsOpen() const { return db_; }

  // Stores |data| under (key, subkey). Returns true if the store suceeded, and
  // false otherwise.
  bool Store(const std::string& key,
             const std::string& subkey,
             const std::string& data);

  // Loads the contents of (key, subkey) into |data| and returns true. Returns
  // false if (key, subkey) isn't found or if there is a problem reading the
  // data.
  bool Load(const std::string& key,
            const std::string& subkey,
            std::string* data);

  // Loads all the subkeys of |key| into |contents|.
  void LoadAllSubkeys(const std::string& key,
                      std::map<std::string, std::string>* contents);

  // Deletes (key, subkey).
  void Delete(const std::string& key, const std::string& subkey);

  // Deletes all the subkeys of |key| not in |subkeys_to_keep|.
  void PurgeOtherSubkeys(const std::string& key,
                         const std::set<std::string>& subkeys_to_keep);

 private:
  std::string GetStringWithPrefix(const std::string& s);
  std::string CreatePathPrefix(const std::string& key);
  std::string CreatePath(const std::string& key, const std::string& subkey);
  std::string GetSubkey(const std::string& path);

  scoped_ptr<leveldb::DB> db_;

  DISALLOW_COPY_AND_ASSIGN(ResourceCache);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_RESOURCE_CACHE_H_
