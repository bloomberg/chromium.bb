// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_URL_LOADER_FACTORY_H_
#define CONTENT_BROWSER_FILE_URL_LOADER_FACTORY_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "content/common/content_export.h"
#include "content/public/common/url_loader_factory.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace content {

// A URLLoaderFactory used for the file:// scheme used when Network Service is
// enabled.
class CONTENT_EXPORT FileURLLoaderFactory : public mojom::URLLoaderFactory {
 public:
  // SequencedTaskRunner must be allowed to block and should have background
  // priority since it will be used to schedule synchronous file I/O tasks.
  FileURLLoaderFactory(const base::FilePath& profile_path,
                       scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~FileURLLoaderFactory() override;

  void BindRequest(mojom::URLLoaderFactoryRequest loader);

 private:
  // mojom::URLLoaderFactory:
  void CreateLoaderAndStart(mojom::URLLoaderRequest loader,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const ResourceRequest& request,
                            mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override;
  void Clone(mojom::URLLoaderFactoryRequest loader) override;

  const base::FilePath profile_path_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  mojo::BindingSet<mojom::URLLoaderFactory> bindings_;

  DISALLOW_COPY_AND_ASSIGN(FileURLLoaderFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_URL_LOADER_FACTORY_H_
