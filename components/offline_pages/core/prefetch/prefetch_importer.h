// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_IMPORTER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_IMPORTER_H_

#include <string>

class GURL;
namespace base {
class FilePath;
}  // namespace base

namespace offline_pages {

struct ClientId;

// Interface to import the downloaded archive such that it can be rendered as
// offline page.
class PrefetchImporter {
 public:
  using CompletedCallback =
      base::Callback<void(bool success, int64_t offline_id)>;

  virtual ~PrefetchImporter() = default;

  // Imports the downloaded archive by moving the file into archive directory
  // and creating an entry in the offline metadata database.
  virtual void ImportFile(const GURL& url,
                          const GURL& original_url,
                          const base::string16& title,
                          int64_t offline_id,
                          const ClientId& client_id,
                          const base::FilePath& file_path,
                          int64_t file_size,
                          const CompletedCallback& callback) = 0;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_IMPORTER_H_
