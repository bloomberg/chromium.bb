// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/kiosk_wm/navigator_host_impl.h"

#include "mojo/services/kiosk_wm/kiosk_wm.h"

namespace kiosk_wm {

NavigatorHostImpl::NavigatorHostImpl(KioskWM* window_manager)
    : current_index_(-1), kiosk_wm_(window_manager) {
}

NavigatorHostImpl::~NavigatorHostImpl() {
}

void NavigatorHostImpl::Bind(
    mojo::InterfaceRequest<mojo::NavigatorHost> request) {
  bindings_.AddBinding(this, request.Pass());
}

void NavigatorHostImpl::DidNavigateLocally(const mojo::String& url) {
  RecordNavigation(url);
  // TODO(abarth): Do something interesting.
}

void NavigatorHostImpl::RequestNavigate(mojo::Target target,
                                        mojo::URLRequestPtr request) {
  // kiosk_wm sets up default services including navigation.
  kiosk_wm_->ReplaceContentWithURL(request->url);
}

void NavigatorHostImpl::RequestNavigateHistory(int32_t delta) {
  if (history_.empty())
    return;
  current_index_ =
      std::max(0, std::min(current_index_ + delta,
                           static_cast<int32_t>(history_.size()) - 1));
  kiosk_wm_->ReplaceContentWithURL(history_[current_index_]);
}

void NavigatorHostImpl::RecordNavigation(const std::string& url) {
  if (current_index_ >= 0 && history_[current_index_] == url) {
    // This is a navigation to the current entry, ignore.
    return;
  }

  history_.erase(history_.begin() + (current_index_ + 1), history_.end());
  history_.push_back(url);
  ++current_index_;
}

}  // namespace kiosk_wm
