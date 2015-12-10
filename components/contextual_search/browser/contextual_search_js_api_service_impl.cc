// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/browser/contextual_search_js_api_service_impl.h"

#include "components/contextual_search/browser/contextual_search_ui_handle.h"

namespace contextual_search {

ContextualSearchJsApiServiceImpl::ContextualSearchJsApiServiceImpl(
    ContextualSearchUIHandle* contextual_search_ui_handle,
    mojo::InterfaceRequest<ContextualSearchJsApiService> request)
    : binding_(this, request.Pass()),
      contextual_search_ui_handle_(contextual_search_ui_handle) {}

ContextualSearchJsApiServiceImpl::~ContextualSearchJsApiServiceImpl() {}

void ContextualSearchJsApiServiceImpl::HandleSetCaption(
    const mojo::String& caption,
    bool does_answer) {
  contextual_search_ui_handle_->SetCaption(caption, does_answer);
}

// static
void CreateContextualSearchJsApiService(
    ContextualSearchUIHandle* contextual_search_ui_handle,
    mojo::InterfaceRequest<ContextualSearchJsApiService> request) {
  // This is strongly bound and owned by the pipe.
  new ContextualSearchJsApiServiceImpl(contextual_search_ui_handle,
                                       request.Pass());
}

}  // namespace contextual_search
