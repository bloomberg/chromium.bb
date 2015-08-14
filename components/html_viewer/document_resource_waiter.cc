// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/document_resource_waiter.h"

#include "components/html_viewer/global_state.h"
#include "components/html_viewer/html_document_oopif.h"
#include "components/html_viewer/html_frame_tree_manager.h"
#include "components/view_manager/public/cpp/view.h"

namespace html_viewer {

DocumentResourceWaiter::DocumentResourceWaiter(GlobalState* global_state,
                                               mojo::URLResponsePtr response,
                                               HTMLDocumentOOPIF* document)
    : global_state_(global_state),
      document_(document),
      response_(response.Pass()),
      root_(nullptr),
      change_id_(0u),
      frame_tree_client_binding_(this) {}

DocumentResourceWaiter::~DocumentResourceWaiter() {
}

void DocumentResourceWaiter::Release(
    mojo::InterfaceRequest<mandoline::FrameTreeClient>*
        frame_tree_client_request,
    mandoline::FrameTreeServerPtr* frame_tree_server,
    mojo::Array<mandoline::FrameDataPtr>* frame_data,
    uint32_t* change_id) {
  DCHECK(IsReady());
  *frame_tree_client_request = frame_tree_client_request_.Pass();
  *frame_tree_server = server_.Pass();
  *frame_data = frame_data_.Pass();
  *change_id = change_id_;
}

mojo::URLResponsePtr DocumentResourceWaiter::ReleaseURLResponse() {
  return response_.Pass();
}

bool DocumentResourceWaiter::IsReady() const {
  return root_ && root_->viewport_metrics().device_pixel_ratio != 0.0f &&
         !frame_data_.is_null();
}

void DocumentResourceWaiter::Bind(
    mojo::InterfaceRequest<mandoline::FrameTreeClient> request) {
  if (frame_tree_client_binding_.is_bound() || !frame_data_.is_null()) {
    DVLOG(1) << "Request for FrameTreeClient after already supplied one";
    return;
  }
  frame_tree_client_binding_.Bind(request.Pass());
}

void DocumentResourceWaiter::OnConnect(
    mandoline::FrameTreeServerPtr server,
    uint32 change_id,
    mojo::Array<mandoline::FrameDataPtr> frame_data) {
  DCHECK(frame_data_.is_null());
  change_id_ = change_id;
  server_ = server.Pass();
  frame_data_ = frame_data.Pass();
  CHECK(frame_data_.size() > 0u);
  frame_tree_client_request_ = frame_tree_client_binding_.Unbind();
  if (IsReady())
    document_->LoadIfNecessary();
}

void DocumentResourceWaiter::OnFrameAdded(uint32 change_id,
                                          mandoline::FrameDataPtr frame_data) {
  // It is assumed we receive OnConnect() (which unbinds) before anything else.
  NOTREACHED();
}

void DocumentResourceWaiter::OnFrameRemoved(uint32 change_id,
                                            uint32_t frame_id) {
  // It is assumed we receive OnConnect() (which unbinds) before anything else.
  NOTREACHED();
}

void DocumentResourceWaiter::OnFrameClientPropertyChanged(
    uint32_t frame_id,
    const mojo::String& name,
    mojo::Array<uint8_t> new_value) {
  // It is assumed we receive OnConnect() (which unbinds) before anything else.
  NOTREACHED();
}

void DocumentResourceWaiter::OnPostMessageEvent(
    uint32_t source_frame_id,
    uint32_t target_frame_id,
    mandoline::HTMLMessageEventPtr event) {
  // It is assumed we receive OnConnect() (which unbinds) before anything else.
  NOTREACHED();
}

}  // namespace html_viewer
