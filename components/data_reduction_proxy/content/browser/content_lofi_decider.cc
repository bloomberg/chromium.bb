// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/content/browser/content_lofi_decider.h"

#include <string>

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "content/public/browser/resource_request_info.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"

namespace data_reduction_proxy {

ContentLoFiDecider::ContentLoFiDecider() {}

ContentLoFiDecider::~ContentLoFiDecider() {}

bool ContentLoFiDecider::IsUsingLoFiMode(const net::URLRequest& request) const {
  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(&request);
  // The Lo-Fi directive should not be added for users in the Lo-Fi field
  // trial "Control" group. Check that the user is in a group that can get
  // "q=low".
  bool lofi_enabled_via_flag_or_field_trial =
      params::IsLoFiOnViaFlags() || params::IsIncludedInLoFiEnabledFieldTrial();

  // Return if the user is using Lo-Fi and not part of the "Control" group.
  if (request_info)
    return request_info->IsUsingLoFi() && lofi_enabled_via_flag_or_field_trial;
  return false;
}

bool ContentLoFiDecider::MaybeAddLoFiDirectiveToHeaders(
    const net::URLRequest& request,
    net::HttpRequestHeaders* headers) const {
  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(&request);

  if (!request_info)
    return false;

  // The Lo-Fi directive should not be added for users in the Lo-Fi field
  // trial "Control" group. Check that the user is in a group that should
  // get "q=low".
  bool lofi_enabled_via_flag_or_field_trial =
      params::IsLoFiOnViaFlags() || params::IsIncludedInLoFiEnabledFieldTrial();

  bool lofi_preview_via_flag_or_field_trial =
      params::AreLoFiPreviewsEnabledViaFlags() ||
      params::IsIncludedInLoFiPreviewFieldTrial();

  std::string header_value;

  // User is using Lo-Fi and not part of the "Control" group.
  if (request_info->IsUsingLoFi() && lofi_enabled_via_flag_or_field_trial) {
    if (headers->HasHeader(chrome_proxy_header())) {
      headers->GetHeader(chrome_proxy_header(), &header_value);
      headers->RemoveHeader(chrome_proxy_header());
      header_value += ", ";
    }

    // Only add the "q=preview" directive on mainframe requests. Otherwise,
    // add "q=low"
    if (lofi_preview_via_flag_or_field_trial &&
        (request.load_flags() & net::LOAD_MAIN_FRAME)) {
      header_value += chrome_proxy_lo_fi_preview_directive();
    } else {
      header_value += chrome_proxy_lo_fi_directive();
    }

    headers->SetHeader(chrome_proxy_header(), header_value);
    return true;
  }

  // User is part of Lo-Fi active control experiment.
  if (request_info->IsUsingLoFi() &&
      params::IsIncludedInLoFiControlFieldTrial()) {
    if (headers->HasHeader(chrome_proxy_header())) {
      headers->GetHeader(chrome_proxy_header(), &header_value);
      headers->RemoveHeader(chrome_proxy_header());
      header_value += ", ";
    }
    header_value += chrome_proxy_lo_fi_experiment_directive();
    headers->SetHeader(chrome_proxy_header(), header_value);
  }

  return false;
}

}  // namespace data_reduction_proxy
