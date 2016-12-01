// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_DOWNLOADED_TEMP_FILE_IMPL_H_
#define CONTENT_BROWSER_LOADER_DOWNLOADED_TEMP_FILE_IMPL_H_

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/common/url_loader_factory.mojom.h"

namespace content {

// DownloadedTempFileImpl is created on the download_to_file mode of resource
// loading. The instance is held by the client and lives until the client
// destroys the interface, slightly after the URLLoader is destroyed.
class CONTENT_EXPORT DownloadedTempFileImpl final
    : public mojom::DownloadedTempFile {
 public:
  static mojo::InterfacePtr<mojom::DownloadedTempFile> Create(int child_id,
                                                              int request_id);

  DownloadedTempFileImpl(int child_id, int request_id);
  ~DownloadedTempFileImpl() override;

 private:
  int child_id_;
  int request_id_;

  DISALLOW_COPY_AND_ASSIGN(DownloadedTempFileImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_DOWNLOADED_TEMP_FILE_IMPL_H_
