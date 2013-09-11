// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_RECOVERY_PRIVATE_WRITE_FROM_URL_OPERATION_H_
#define CHROME_BROWSER_EXTENSIONS_API_RECOVERY_PRIVATE_WRITE_FROM_URL_OPERATION_H_

#include "base/scoped_observer.h"
#include "chrome/browser/extensions/api/recovery_private/recovery_operation.h"
#include "content/public/browser/download_item.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

namespace content {

class RenderViewHost;

}  // namespace content

namespace extensions {
namespace recovery {

class RecoveryOperationManager;

// Encapsulates a write of an image accessed via URL.
class WriteFromUrlOperation : public RecoveryOperation,
                              public content::DownloadItem::Observer {
 public:
  WriteFromUrlOperation(RecoveryOperationManager* manager,
                        const ExtensionId& extension_id,
                        content::RenderViewHost* rvh,
                        GURL url,
                        const std::string& hash,
                        bool saveImageAsDownload,
                        const std::string& storage_unit_id);
  virtual void Cancel() OVERRIDE;
  virtual void Start() OVERRIDE;
 private:
  virtual ~WriteFromUrlOperation();
  void CreateTempFile();

  void DownloadStart();
  void OnDownloadStarted(content::DownloadItem*, net::Error);
  virtual void OnDownloadUpdated(content::DownloadItem* download) OVERRIDE;
  void DownloadComplete();

  void VerifyDownloadStart();
  void VerifyDownloadRun();
  void VerifyDownloadCompare(scoped_ptr<std::string> download_hash);
  void VerifyDownloadComplete();

  content::RenderViewHost* rvh_;
  GURL url_;
  const std::string hash_;
  const bool saveImageAsDownload_;
  scoped_ptr<base::FilePath> tmp_file_;
  content::DownloadItem* download_;

  base::FilePath download_path_;
};

} // namespace recovery
} // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_RECOVERY_PRIVATE_WRITE_FROM_URL_OPERATION_H_
