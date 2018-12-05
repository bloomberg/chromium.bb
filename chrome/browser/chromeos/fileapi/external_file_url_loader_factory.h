// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILEAPI_EXTERNAL_FILE_URL_LOADER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_FILEAPI_EXTERNAL_FILE_URL_LOADER_FACTORY_H_

#include "base/macros.h"
#include "chromeos/chromeos_export.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace chromeos {

// URLLoaderFactory that creates URLLoader instances for URLs with the
// externalfile scheme.
class CHROMEOS_EXPORT ExternalFileURLLoaderFactory
    : public network::mojom::URLLoaderFactory {
 public:
  explicit ExternalFileURLLoaderFactory(void* profile_id);
  ~ExternalFileURLLoaderFactory() override;

 private:
  friend class ExternalFileURLLoaderFactoryTest;

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

  mojo::BindingSet<network::mojom::URLLoaderFactory> bindings_;
  void* profile_id_;

  DISALLOW_COPY_AND_ASSIGN(ExternalFileURLLoaderFactory);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILEAPI_EXTERNAL_FILE_URL_LOADER_FACTORY_H_
