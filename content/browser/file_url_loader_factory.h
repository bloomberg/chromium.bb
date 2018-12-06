// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_URL_LOADER_FACTORY_H_
#define CONTENT_BROWSER_FILE_URL_LOADER_FACTORY_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

class SharedCorsOriginAccessList;

// A URLLoaderFactory used for the file:// scheme used when Network Service is
// enabled.
// If a caller needs a request that has a fetch request mode other than
// "no-cors", this class should be used on the UI thread.
class CONTENT_EXPORT FileURLLoaderFactory
    : public network::mojom::URLLoaderFactory,
      public base::SupportsWeakPtr<FileURLLoaderFactory> {
 public:
  // SequencedTaskRunner must be allowed to block and should have background
  // priority since it will be used to schedule synchronous file I/O tasks.
  // |shared_cors_origin_access_list| can be nullptr if only "no-cors" requests
  // will be made.
  FileURLLoaderFactory(const base::FilePath& profile_path,
                       scoped_refptr<const SharedCorsOriginAccessList>
                           shared_cors_origin_access_list,
                       scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~FileURLLoaderFactory() override;

 private:
  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest loader,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override;
  void Clone(network::mojom::URLLoaderFactoryRequest loader) override;

  void CreateLoaderAndStartInternal(const network::ResourceRequest request,
                                    network::mojom::URLLoaderRequest loader,
                                    network::mojom::URLLoaderClientPtr client,
                                    bool cors_flag);

  const base::FilePath profile_path_;
  const scoped_refptr<const SharedCorsOriginAccessList>
      shared_cors_origin_access_list_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  mojo::BindingSet<network::mojom::URLLoaderFactory> bindings_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(FileURLLoaderFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_URL_LOADER_FACTORY_H_
