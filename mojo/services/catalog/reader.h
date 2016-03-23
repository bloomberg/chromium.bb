// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_CATALOG_READER_H_
#define MOJO_SERVICES_CATALOG_READER_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"

namespace base {
class TaskRunner;
}

namespace catalog {

class Entry;

// Encapsulates manifest reading.
class Reader {
 public:
  using ReadManifestCallback =
      base::Callback<void(scoped_ptr<Entry> entry)>;

  // Loads manifests relative to |package_path|. Performs file operations on
  // |file_task_runner|.
  Reader(const base::FilePath& package_path,
         base::TaskRunner* file_task_runner);
  ~Reader();

  // Attempts to load a manifest for |name|, and returns an Entry built from the
  // metadata it contains via |callback|.
  void Read(const std::string& name,
            const ReadManifestCallback& callback);

 private:
  // Construct a manifest path for the application named |name| within
  // |package_path_|.
  base::FilePath GetManifestPath(const std::string& name) const;

  base::FilePath package_path_;
  base::TaskRunner* file_task_runner_;
  base::WeakPtrFactory<Reader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Reader);
};

}  // namespace catalog

#endif  // MOJO_SERVICES_CATALOG_READER_H_
