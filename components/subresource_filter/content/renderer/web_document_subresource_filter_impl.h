// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_RENDERER_WEB_DOCUMENT_SUBRESOURCE_FILTER_IMPL_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_RENDERER_WEB_DOCUMENT_SUBRESOURCE_FILTER_IMPL_H_

#include "base/callback.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "components/subresource_filter/core/common/document_subresource_filter.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "third_party/WebKit/public/platform/WebDocumentSubresourceFilter.h"
#include "url/origin.h"

namespace subresource_filter {

class MemoryMappedRuleset;

// Performs filtering of subresource loads in the scope of a given document.
class WebDocumentSubresourceFilterImpl
    : public blink::WebDocumentSubresourceFilter,
      public base::SupportsWeakPtr<WebDocumentSubresourceFilterImpl> {
 public:
  // This builder class is created on the main thread and passed to a worker
  // thread to create the subresource filter for the worker thread.
  class BuilderImpl : public blink::WebDocumentSubresourceFilter::Builder {
   public:
    BuilderImpl(url::Origin document_origin,
                ActivationState activation_state,
                base::File ruleset_file,
                base::OnceClosure first_disallowed_load_callback);
    ~BuilderImpl() override;

    std::unique_ptr<blink::WebDocumentSubresourceFilter> Build() override;

   private:
    url::Origin document_origin_;
    ActivationState activation_state_;
    base::File ruleset_file_;
    base::OnceClosure first_disallowed_load_callback_;
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

    DISALLOW_COPY_AND_ASSIGN(BuilderImpl);
  };

  // See DocumentSubresourceFilter description.
  //
  // Invokes |first_disallowed_load_callback|, if it is non-null, on the first
  // reportDisallowedLoad() call.
  WebDocumentSubresourceFilterImpl(
      url::Origin document_origin,
      ActivationState activation_state,
      scoped_refptr<const MemoryMappedRuleset> ruleset,
      base::OnceClosure first_disallowed_load_callback);

  ~WebDocumentSubresourceFilterImpl() override;

  const DocumentSubresourceFilter& filter() const { return filter_; }

  // blink::WebDocumentSubresourceFilter:
  LoadPolicy GetLoadPolicy(const blink::WebURL& resourceUrl,
                           blink::WebURLRequest::RequestContext) override;
  LoadPolicy GetLoadPolicyForWebSocketConnect(
      const blink::WebURL& url) override;
  void ReportDisallowedLoad() override;
  bool ShouldLogToConsole() override;

  const ActivationState& activation_state() const {
    return filter_.activation_state();
  }

 private:
  LoadPolicy getLoadPolicyImpl(
      const blink::WebURL& url,
      url_pattern_index::proto::ElementType element_type);

  ActivationState activation_state_;
  DocumentSubresourceFilter filter_;
  base::OnceClosure first_disallowed_load_callback_;

  DISALLOW_COPY_AND_ASSIGN(WebDocumentSubresourceFilterImpl);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_RENDERER_WEB_DOCUMENT_SUBRESOURCE_FILTER_IMPL_H_
