// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TEST_PREFETCH_IMPORTER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TEST_PREFETCH_IMPORTER_H_

#include "components/offline_pages/core/prefetch/prefetch_importer.h"

namespace offline_pages {

// Testing prefetch importer that does nothing.
class TestPrefetchImporter : public PrefetchImporter {
 public:
  TestPrefetchImporter() {}
  ~TestPrefetchImporter() override = default;

  void ImportFile(const GURL& url,
                  const GURL& original_url,
                  const base::string16& title,
                  int64_t offline_id,
                  const ClientId& client_id,
                  const base::FilePath& file_path,
                  int64_t file_size,
                  const CompletedCallback& callback) override {}
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TEST_PREFETCH_IMPORTER_H_