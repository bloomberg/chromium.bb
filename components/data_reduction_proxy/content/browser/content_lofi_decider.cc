// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/content/browser/content_lofi_decider.h"

#include <string>

#include "base/feature_list.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/resource_type.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request.h"

namespace data_reduction_proxy {

ContentLoFiDecider::ContentLoFiDecider() {}

ContentLoFiDecider::~ContentLoFiDecider() {}

bool ContentLoFiDecider::IsUsingLoFi(const net::URLRequest& request) const {
  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(&request);
  // The Lo-Fi directive should not be added for users in the Lo-Fi field
  // trial "Control" group. Check that the user is in a group that can get
  // "q=low".
  bool lofi_enabled_via_flag_or_field_trial =
      params::IsLoFiOnViaFlags() || params::IsIncludedInLoFiEnabledFieldTrial();

  // Return if the user is using Lo-Fi and not part of the "Control" group.
  if (request_info) {
    return (request_info->GetPreviewsState() & content::SERVER_LOFI_ON) &&
           lofi_enabled_via_flag_or_field_trial;
  }
  return false;
}

void ContentLoFiDecider::MaybeSetAcceptTransformHeader(
    const net::URLRequest& request,
    bool are_previews_disabled,
    net::HttpRequestHeaders* headers) const {
  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(&request);

  if (!request_info)
    return;

  // Previews only operate on HTTP.
  if (!request.url().SchemeIs("http"))
    return;

  // Chrome-Proxy-Accept-Transform takes at most one token.
  if (headers->HasHeader(chrome_proxy_accept_transform_header()))
    return;

  content::ResourceType resource_type = request_info->GetResourceType();

  if (resource_type == content::RESOURCE_TYPE_MEDIA) {
    headers->SetHeader(chrome_proxy_accept_transform_header(),
                       compressed_video_directive());
    return;
  }

  content::PreviewsState previews_state = request_info->GetPreviewsState();

  // Do not add the Chrome-Proxy-Accept-Transform header when the page load
  // explicitly forbids previews transformations.
  if (previews_state & content::PREVIEWS_NO_TRANSFORM ||
      previews_state & content::PREVIEWS_OFF) {
    return;
  }

  // For the proxy-decides-transform feature, the header is set based only
  // on the request's PreviewsState (no checking of previews flags).
  // This is for a newer version of the proxy server protocol where the
  // server makes the transform decision and client simply advertises
  // if is accepts transforms for the resource type (determined here from
  // previously set previews state bits). The previous logic in this method
  // that checks various flags will be deprecated.
  bool check_previews_flags = !base::FeatureList::IsEnabled(
      features::kDataReductionProxyDecidesTransform);

  bool lofi_enabled_via_flags_or_field_trial = false;
  bool lite_page_enabled_via_flags_or_field_trial = false;
  if (check_previews_flags) {
    // The Lo-Fi and Lite Page directives should not be added for users in the
    // Lo-Fi field trial "Control" group.
    lofi_enabled_via_flags_or_field_trial =
        params::IsLoFiOnViaFlags() ||
        params::IsIncludedInLoFiEnabledFieldTrial();

    lite_page_enabled_via_flags_or_field_trial =
        (params::IsLoFiOnViaFlags() && params::AreLitePagesEnabledViaFlags()) ||
        params::IsIncludedInLitePageFieldTrial();

    // Previews have been disabled.
    if (are_previews_disabled)
      return;

    // User does not have previews enabled.
    if (!lofi_enabled_via_flags_or_field_trial &&
        !lite_page_enabled_via_flags_or_field_trial) {
      return;
    }
  }

  // Lo-Fi is not allowed on the main frame, stylesheet, script, font resource,
  // media, service worker, or CSP report.
  bool resource_type_supports_empty_image =
      !(resource_type == content::RESOURCE_TYPE_MAIN_FRAME ||
        resource_type == content::RESOURCE_TYPE_STYLESHEET ||
        resource_type == content::RESOURCE_TYPE_SCRIPT ||
        resource_type == content::RESOURCE_TYPE_FONT_RESOURCE ||
        resource_type == content::RESOURCE_TYPE_MEDIA ||
        resource_type == content::RESOURCE_TYPE_CSP_REPORT);

  std::string accept_transform_value;
  if (!check_previews_flags) {
    // For the proxy-decides-transform feature, check PreviewsState bits and
    // type of resource to determine which, if any, transformation to accept.
    if ((previews_state & content::SERVER_LITE_PAGE_ON) &&
        resource_type == content::RESOURCE_TYPE_MAIN_FRAME) {
      accept_transform_value = lite_page_directive();
    } else if ((previews_state & content::SERVER_LOFI_ON) &&
               resource_type_supports_empty_image) {
      // Note that for subresource requests, the Lo-Fi bit should only be set
      // if the main frame response provided the "empty-image" directive (for
      // the client to echo back to the server here for any image resources).
      accept_transform_value = empty_image_directive();
    }
  } else if (lite_page_enabled_via_flags_or_field_trial &&
             resource_type == content::RESOURCE_TYPE_MAIN_FRAME) {
    // If the Lite Page field trial or flag is enabled, only add the "lite-page"
    // directive on main frame requests. Only add "empty-image" directives to
    // other requests when Lite Page previews are not enabled after the main
    // frame. Add the "if-heavy" qualifier to allow the server to provide a
    // preview when the page is data heavy on if a preview was not otherwise
    // triggered.
    accept_transform_value = lite_page_directive();

    // Since a Lite Page was not triggered client side, ask the server to
    // provide a Lite Page only if the page is otherwise data-heavy.
    if (!(previews_state & content::SERVER_LITE_PAGE_ON))
      accept_transform_value += base::StringPrintf(";%s", if_heavy_qualifier());
  } else if (lofi_enabled_via_flags_or_field_trial &&
             !lite_page_enabled_via_flags_or_field_trial &&
             resource_type_supports_empty_image &&
             !(previews_state & content::SERVER_LITE_PAGE_ON)) {
    accept_transform_value = empty_image_directive();

    // Since Lo-Fi was not triggered client side, ask the server to provide
    // Lo-Fi only if the page is otherwise data-heavy.
    if (!(previews_state & content::SERVER_LOFI_ON))
      accept_transform_value += base::StringPrintf(";%s", if_heavy_qualifier());
  }
  if (accept_transform_value.empty())
    return;

  headers->SetHeader(chrome_proxy_accept_transform_header(),
                     accept_transform_value);
}

bool ContentLoFiDecider::IsSlowPagePreviewRequested(
    const net::HttpRequestHeaders& headers) const {
  std::string accept_transform_header_value;
  if (!headers.GetHeader(chrome_proxy_accept_transform_header(),
                         &accept_transform_header_value)) {
    return false;
  }

  std::vector<std::string> tokens =
      base::SplitString(base::ToLowerASCII(accept_transform_header_value), ";",
                        base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  // A slow page preview is a request for any unqualified transform type.
  if (tokens.size() != 1)
    return false;
  std::string transform_type;
  base::TrimWhitespaceASCII(tokens[0], base::TRIM_ALL, &transform_type);
  return (transform_type == lite_page_directive() ||
          transform_type == empty_image_directive());
}

bool ContentLoFiDecider::IsLitePagePreviewRequested(
    const net::HttpRequestHeaders& headers) const {
  std::string accept_transform_header_value;
  if (!headers.GetHeader(chrome_proxy_accept_transform_header(),
                         &accept_transform_header_value)) {
    return false;
  }
  std::vector<std::string> tokens =
      base::SplitString(base::ToLowerASCII(accept_transform_header_value), ";",
                        base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (tokens.empty())
    return false;
  std::string transform_type;
  base::TrimWhitespaceASCII(tokens[0], base::TRIM_ALL, &transform_type);
  return transform_type == lite_page_directive();
}

void ContentLoFiDecider::RemoveAcceptTransformHeader(
    net::HttpRequestHeaders* headers) const {
  headers->RemoveHeader(chrome_proxy_accept_transform_header());
}

bool ContentLoFiDecider::ShouldRecordLoFiUMA(
    const net::URLRequest& request) const {
  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(&request);

  // User is not using Lo-Fi.
  if (!request_info ||
      !(request_info->GetPreviewsState() & content::SERVER_LOFI_ON ||
        request_info->GetPreviewsState() & content::SERVER_LITE_PAGE_ON)) {
    return false;
  }

  return params::IsIncludedInLoFiEnabledFieldTrial() ||
         params::IsIncludedInLoFiControlFieldTrial();
}

bool ContentLoFiDecider::IsClientLoFiImageRequest(
    const net::URLRequest& request) const {
  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(&request);
  return request_info &&
         request_info->GetResourceType() == content::RESOURCE_TYPE_IMAGE &&
         (request_info->GetPreviewsState() & content::CLIENT_LOFI_ON);
}

bool ContentLoFiDecider::IsClientLoFiAutoReloadRequest(
    const net::URLRequest& request) const {
  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(&request);
  return request_info &&
         (request_info->GetPreviewsState() & content::CLIENT_LOFI_AUTO_RELOAD);
}
}  // namespace data_reduction_proxy
