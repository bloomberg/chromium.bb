// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_INTERNALS_UI_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_INTERNALS_UI_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/services/storage/public/mojom/indexed_db_control.mojom.h"
#include "content/public/browser/web_ui_controller.h"

namespace base {
class ListValue;
}

namespace url {
class Origin;
}

namespace download {
class DownloadItem;
}

namespace content {

// The implementation for the chrome://indexeddb-internals page.
class IndexedDBInternalsUI : public WebUIController {
 public:
  explicit IndexedDBInternalsUI(WebUI* web_ui);
  ~IndexedDBInternalsUI() override;

 private:
  void GetAllOrigins(const base::ListValue* args);
  void OnOriginsReady(const base::Value& origins, const base::FilePath& path);

  void DownloadOriginData(const base::ListValue* args);
  void OnDownloadDataReady(const base::FilePath& partition_path,
                           const url::Origin& origin,
                           uint64_t connection_count,
                           bool success,
                           const base::FilePath& temp_path,
                           const base::FilePath& zip_path);
  void OnDownloadStarted(const base::FilePath& partition_path,
                         const url::Origin& origin,
                         const base::FilePath& temp_path,
                         size_t connection_count,
                         download::DownloadItem* item,
                         download::DownloadInterruptReason interrupt_reason);

  void ForceCloseOrigin(const base::ListValue* args);
  void OnForcedClose(const base::FilePath& partition_path,
                     const url::Origin& origin,
                     uint64_t connection_count);

  bool GetOriginControl(const base::FilePath& path,
                        const url::Origin& origin,
                        storage::mojom::IndexedDBControl** control);
  bool GetOriginData(const base::ListValue* args,
                     base::FilePath* path,
                     url::Origin* origin,
                     storage::mojom::IndexedDBControl** control);

  base::WeakPtrFactory<IndexedDBInternalsUI> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(IndexedDBInternalsUI);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_INTERNALS_UI_H_
