// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_PREFETCH_IMPORTER_IMPL_H_
#define CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_PREFETCH_IMPORTER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/prefetch/prefetch_importer.h"

namespace base {
class TaskRunner;
}

namespace content {
class BrowserContext;
}

namespace offline_pages {

enum class AddPageResult;
struct OfflinePageItem;

// The implementation of PrefetchImporter.
class PrefetchImporterImpl : public PrefetchImporter {
 public:
  PrefetchImporterImpl(PrefetchDispatcher* dispatcher,
                       content::BrowserContext* context,
                       scoped_refptr<base::TaskRunner> background_task_runner);
  ~PrefetchImporterImpl() override;

  // PrefetchImporter implementation.
  void ImportArchive(const PrefetchArchiveInfo& archive) override;
  void MarkImportCompleted(int64_t offline_id) override;
  std::set<int64_t> GetOutstandingImports() const override;

 private:
  void OnMoveFileDone(const OfflinePageItem& offline_page,
                      bool success);
  void OnPageAdded(AddPageResult result, int64_t offline_id);

  content::BrowserContext* context_;
  scoped_refptr<base::TaskRunner> background_task_runner_;
  std::set<int64_t> outstanding_import_offline_ids_;
  base::WeakPtrFactory<PrefetchImporterImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchImporterImpl);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_PREFETCH_IMPORTER_IMPL_H_
