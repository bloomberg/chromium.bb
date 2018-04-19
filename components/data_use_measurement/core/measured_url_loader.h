// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_USE_MEASUREMENT_CORE_MEASURED_URL_LOADER_H_
#define COMPONENTS_DATA_USE_MEASUREMENT_CORE_MEASURED_URL_LOADER_H_

#include <memory>

#include "base/macros.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace data_use_measurement {

// This class should be used by all features/services so that their data use can
// be tracked to verify that no features/services are using more data than they
// should.
//
// The goal of this class is to appropriately attribute each service/feature
// related load to a specific service/feature for the purpose of recording
// metrics about how often and how much data is used by certain
// services/features.
class MeasuredURLLoader : public network::SimpleURLLoader {
 public:
  enum class ServiceName {
    // URL loads related to the translation service.
    kTranslate = 0,
    // URL loads related to the data reduction proxy.
    kDataReductionProxy = 1,
    kCount = 2,
  };

  // Creates a default SimpleURLLoader (provided by
  // network::SimpleURLLoader::Create) wrapped in a MeasuredURLLoader.
  static std::unique_ptr<network::SimpleURLLoader> Create(
      std::unique_ptr<network::ResourceRequest> resource_request,
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      ServiceName service_name);

  ~MeasuredURLLoader() override;

  // network::SimpleURLLoader implementation.
  void DownloadToString(
      network::mojom::URLLoaderFactory* url_loader_factory,
      network::SimpleURLLoader::BodyAsStringCallback body_as_string_callback,
      size_t max_body_size) override;
  void DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      network::mojom::URLLoaderFactory* url_loader_factory,
      network::SimpleURLLoader::BodyAsStringCallback body_as_string_callback)
      override;
  void DownloadToFile(network::mojom::URLLoaderFactory* url_loader_factory,
                      network::SimpleURLLoader::DownloadToFileCompleteCallback
                          download_to_file_complete_callback,
                      const base::FilePath& file_path,
                      int64_t max_body_size) override;
  void DownloadToTempFile(
      network::mojom::URLLoaderFactory* url_loader_factory,
      network::SimpleURLLoader::DownloadToFileCompleteCallback
          download_to_file_complete_callback,
      int64_t max_body_size) override;
  void DownloadAsStream(
      network::mojom::URLLoaderFactory* url_loader_factory,
      network::SimpleURLLoaderStreamConsumer* stream_consumer) override;
  void SetOnRedirectCallback(const network::SimpleURLLoader::OnRedirectCallback&
                                 on_redirect_callback) override;
  void SetOnResponseStartedCallback(
      OnResponseStartedCallback on_response_started_callback) override;
  void SetAllowPartialResults(bool allow_partial_results) override;
  void SetAllowHttpErrorResults(bool allow_http_error_results) override;
  void AttachStringForUpload(const std::string& upload_data,
                             const std::string& upload_content_type) override;
  void AttachFileForUpload(const base::FilePath& upload_file_path,
                           const std::string& upload_content_type) override;
  void SetRetryOptions(int max_retries, int retry_mode) override;
  int NetError() const override;
  const network::ResourceResponseHead* ResponseInfo() const override;
  const GURL& GetFinalURL() const override;

 protected:
  // |default_loader| will load URL and all calls to MeasuredURLLoader will be
  // forwarded to |default_loader|. |service_name| is the service that is
  // requesting the URL load.
  // Protected for testing.
  MeasuredURLLoader(std::unique_ptr<network::SimpleURLLoader> default_loader,
                    ServiceName service_name);

 private:
  // A SimpleURLLoader that performs all loading actions.
  const std::unique_ptr<network::SimpleURLLoader> default_loader_;

  // The service that is requesting the URL load.
  const ServiceName service_name_;

  DISALLOW_COPY_AND_ASSIGN(MeasuredURLLoader);
};

}  // namespace data_use_measurement

#endif  // COMPONENTS_DATA_USE_MEASUREMENT_CORE_MEASURED_URL_LOADER_H_
