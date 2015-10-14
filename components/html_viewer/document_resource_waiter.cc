// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/document_resource_waiter.h"

#include "components/html_viewer/global_state.h"
#include "components/html_viewer/html_document.h"
#include "components/html_viewer/html_frame_tree_manager.h"
#include "components/mus/public/cpp/view.h"

using web_view::mojom::ViewConnectType;

namespace html_viewer {

DocumentResourceWaiter::DocumentResourceWaiter(GlobalState* global_state,
                                               mojo::URLResponsePtr response,
                                               HTMLDocument* document)
    : global_state_(global_state),
      document_(document),
      response_(response.Pass()),
      root_(nullptr),
      change_id_(0u),
      view_id_(0u),
      view_connect_type_(web_view::mojom::VIEW_CONNECT_TYPE_USE_NEW),
      frame_client_binding_(this),
      is_ready_(false),
      waiting_for_change_id_(false),
      target_frame_tree_(nullptr) {}

DocumentResourceWaiter::~DocumentResourceWaiter() {
  if (root_)
    root_->RemoveObserver(this);
  if (target_frame_tree_)
    target_frame_tree_->RemoveObserver(this);
}

void DocumentResourceWaiter::Release(
    mojo::InterfaceRequest<web_view::mojom::FrameClient>* frame_client_request,
    web_view::mojom::FramePtr* frame,
    mojo::Array<web_view::mojom::FrameDataPtr>* frame_data,
    uint32_t* change_id,
    uint32_t* view_id,
    ViewConnectType* view_connect_type,
    OnConnectCallback* on_connect_callback) {
  DCHECK(is_ready_);
  *frame_client_request = frame_client_request_.Pass();
  *frame = frame_.Pass();
  *frame_data = frame_data_.Pass();
  *change_id = change_id_;
  *view_id = view_id_;
  *view_connect_type = view_connect_type_;
  *on_connect_callback = on_connect_callback_;
}

mojo::URLResponsePtr DocumentResourceWaiter::ReleaseURLResponse() {
  return response_.Pass();
}

void DocumentResourceWaiter::SetRoot(mus::View* root) {
  DCHECK(!root_);
  root_ = root;
  root_->AddObserver(this);
  UpdateIsReady();
}

void DocumentResourceWaiter::Bind(
    mojo::InterfaceRequest<web_view::mojom::FrameClient> request) {
  if (frame_client_binding_.is_bound() || !frame_data_.is_null()) {
    DVLOG(1) << "Request for FrameClient after already supplied one";
    return;
  }
  frame_client_binding_.Bind(request.Pass());
}

void DocumentResourceWaiter::UpdateIsReady() {
  if (is_ready_)
    return;

  // See description of |waiting_for_change_id_| for details.
  if (waiting_for_change_id_) {
    if (target_frame_tree_->change_id() == change_id_) {
      is_ready_ = true;
      waiting_for_change_id_ = false;
      document_->Load();
    }
    return;
  }

  // The first portion of ready is when we have received OnConnect()
  // (|frame_data_| is valid) and we have a view with valid metrics. The view
  // is not necessary is ViewConnectType is USE_EXISTING, which means the
  // application is not asked for a ViewTreeClient. The metrics are necessary
  // to initialize ResourceBundle. If USE_EXISTING is true, it means a View has
  // already been provided to another HTMLDocument and there is no need to wait
  // for metrics.
  bool is_ready =
      (!frame_data_.is_null() &&
       ((view_connect_type_ ==
         web_view::mojom::VIEW_CONNECT_TYPE_USE_EXISTING) ||
        (root_ && root_->viewport_metrics().device_pixel_ratio != 0.0f)));
  if (is_ready) {
    HTMLFrameTreeManager* frame_tree =
        HTMLFrameTreeManager::FindFrameTreeWithRoot(frame_data_[0]->frame_id);
    // Once we've received OnConnect() and the view (if necessary), we determine
    // which HTMLFrameTreeManager the new frame ends up in. If there is an
    // existing HTMLFrameTreeManager then we must wait for the change_id
    // supplied to OnConnect() to be <= that of the HTMLFrameTreeManager's
    // change_id. If we did not wait for the change id to be <= then the
    // structure of the tree is not in the expected state and it's possible the
    // frame communicated in OnConnect() does not exist yet.
    if (frame_tree && change_id_ > frame_tree->change_id()) {
      waiting_for_change_id_ = true;
      target_frame_tree_ = frame_tree;
      target_frame_tree_->AddObserver(this);
    } else {
      is_ready_ = true;
      document_->Load();
    }
  }
}

void DocumentResourceWaiter::OnConnect(
    web_view::mojom::FramePtr frame,
    uint32_t change_id,
    uint32_t view_id,
    ViewConnectType view_connect_type,
    mojo::Array<web_view::mojom::FrameDataPtr> frame_data,
    const OnConnectCallback& callback) {
  DCHECK(frame_data_.is_null());
  change_id_ = change_id;
  view_id_ = view_id;
  view_connect_type_ = view_connect_type;
  frame_ = frame.Pass();
  frame_data_ = frame_data.Pass();
  on_connect_callback_ = callback;
  CHECK(frame_data_.size() > 0u);
  frame_client_request_ = frame_client_binding_.Unbind();
  UpdateIsReady();
}

void DocumentResourceWaiter::OnFrameAdded(
    uint32_t change_id,
    web_view::mojom::FrameDataPtr frame_data) {
  // It is assumed we receive OnConnect() (which unbinds) before anything else.
  NOTREACHED();
}

void DocumentResourceWaiter::OnFrameRemoved(uint32_t change_id,
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
    web_view::mojom::HTMLMessageEventPtr event) {
  // It is assumed we receive OnConnect() (which unbinds) before anything else.
  NOTREACHED();
}

void DocumentResourceWaiter::OnWillNavigate(
    const mojo::String& origin,
    const OnWillNavigateCallback& callback) {
  // It is assumed we receive OnConnect() (which unbinds) before anything else.
  NOTREACHED();
}

void DocumentResourceWaiter::OnFrameLoadingStateChanged(uint32_t frame_id,
                                                        bool loading) {
  // It is assumed we receive OnConnect() (which unbinds) before anything else.
  NOTREACHED();
}

void DocumentResourceWaiter::OnDispatchFrameLoadEvent(uint32_t frame_id) {
  // It is assumed we receive OnConnect() (which unbinds) before anything else.
  NOTREACHED();
}

void DocumentResourceWaiter::Find(int32_t request_id,
                                  const mojo::String& search_text,
                                  web_view::mojom::FindOptionsPtr options,
                                  bool wrap_within_frame,
                                  const FindCallback& callback) {
  // It is assumed we receive OnConnect() (which unbinds) before anything else.
  NOTREACHED();
}

void DocumentResourceWaiter::StopFinding(bool clear_selection) {
  // It is assumed we receive OnConnect() (which unbinds) before anything else.
  NOTREACHED();
}

void DocumentResourceWaiter::HighlightFindResults(
    int32_t request_id,
    const mojo::String& search_test,
    web_view::mojom::FindOptionsPtr options,
    bool reset) {
  // It is assumed we receive OnConnect() (which unbinds) before anything else.
  NOTREACHED();
}

void DocumentResourceWaiter::StopHighlightingFindResults() {
  // It is assumed we receive OnConnect() (which unbinds) before anything else.
  NOTREACHED();
}

void DocumentResourceWaiter::OnViewViewportMetricsChanged(
    mus::View* view,
    const mojo::ViewportMetrics& old_metrics,
    const mojo::ViewportMetrics& new_metrics) {
  UpdateIsReady();
}

void DocumentResourceWaiter::OnViewDestroyed(mus::View* view) {
  root_->RemoveObserver(this);
  root_ = nullptr;
}

void DocumentResourceWaiter::OnHTMLFrameTreeManagerChangeIdAdvanced() {
  UpdateIsReady();
}

void DocumentResourceWaiter::OnHTMLFrameTreeManagerDestroyed() {
  document_->Destroy();  // This destroys us.
}

}  // namespace html_viewer
