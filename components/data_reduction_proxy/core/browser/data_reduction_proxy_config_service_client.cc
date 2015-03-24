// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_service_client.h"

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"

namespace data_reduction_proxy {

DataReductionProxyConfigServiceClient::DataReductionProxyConfigServiceClient(
    DataReductionProxyParams* params,
    DataReductionProxyRequestOptions* request_options)
    : params_(params),
      request_options_(request_options) {
  DCHECK(params);
  DCHECK(request_options);
}

DataReductionProxyConfigServiceClient::
    ~DataReductionProxyConfigServiceClient() {
}

std::string
DataReductionProxyConfigServiceClient::ConstructStaticResponse() const {
  std::string response;
  scoped_ptr<base::DictionaryValue> values(new base::DictionaryValue());
  params_->PopulateConfigResponse(values.get());
  request_options_->PopulateConfigResponse(values.get());
  base::JSONWriter::Write(values.get(), &response);

  return response;
}

}  // namespace data_reduction_proxy
