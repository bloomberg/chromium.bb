// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PRINTING_PPD_CACHE_H_
#define CHROMEOS_PRINTING_PPD_CACHE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {

// PpdCache manages a cache of locally-stored PPD files.  At its core, it
// operates like a persistent hash from PpdReference to files.  If you give the
// same PpdReference to Find() that was previously given to store, you should
// get the same FilePath back out (unless the previous entry has timed out of
// the cache).  However, changing *any* field in PpdReference will make the
// previous cache entry invalid.  This is the intentional behavior -- we want to
// re-run the resolution logic if we have new meta-information about a printer.
//
// All cache functions must be called on a thread which is permitted to do I/O.
class CHROMEOS_EXPORT PpdCache : public base::RefCounted<PpdCache> {
 public:
  struct FindResult {
    // Did we find something?  If this is false, none of the other fields are
    // valid.
    bool success = false;

    // How old is this entry?
    base::TimeDelta age;

    // Contents of the entry.
    std::string contents;
  };

  using FindCallback = base::Callback<void(const FindResult& result)>;

  // Create and return a Ppdcache that uses cache_dir to store state.  If
  // cache_base_dir does not exist, it will be lazily created the first time the
  // cache needs to store state.
  static scoped_refptr<PpdCache> Create(const base::FilePath& cache_base_dir);

  // Start a Find, looking, for an entry with the given key that is at most
  // |max_age| old.  |cb| will be invoked on the calling thread.
  virtual void Find(const std::string& key, const FindCallback& cb) = 0;

  // Store the given contents at the given key.  If cb is non-null, it will
  // be invoked on completion.
  virtual void Store(const std::string& key,
                     const std::string& contents,
                     const base::Closure& cb) = 0;

 protected:
  friend class base::RefCounted<PpdCache>;
  virtual ~PpdCache() {}
};

}  // namespace chromeos

#endif  // CHROMEOS_PRINTING_PPD_CACHE_H_
