// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/contents/blimp_navigation_controller_impl.h"

#include "blimp/client/core/contents/blimp_navigation_controller_delegate.h"

namespace {
// TODO(shaktisahu): NavigationFeature currently needs a tab_id. Remove this
// later when it is fully integrated with BlimpClientContext.
const int kDummyTabId = 0;
}  // namespace

namespace blimp {
namespace client {

BlimpNavigationControllerImpl::BlimpNavigationControllerImpl(
    BlimpNavigationControllerDelegate* delegate,
    NavigationFeature* feature)
    : navigation_feature_(feature), delegate_(delegate) {
  if (navigation_feature_)
    navigation_feature_->SetDelegate(kDummyTabId, this);
}

BlimpNavigationControllerImpl::~BlimpNavigationControllerImpl() = default;

void BlimpNavigationControllerImpl::SetNavigationFeatureForTesting(
    NavigationFeature* feature) {
  navigation_feature_ = feature;
  navigation_feature_->SetDelegate(kDummyTabId, this);
}

void BlimpNavigationControllerImpl::LoadURL(const GURL& url) {
  current_url_ = url;
  navigation_feature_->NavigateToUrlText(kDummyTabId, current_url_.spec());
}

void BlimpNavigationControllerImpl::Reload() {
  navigation_feature_->Reload(kDummyTabId);
}

bool BlimpNavigationControllerImpl::CanGoBack() const {
  NOTIMPLEMENTED();
  return true;
}

bool BlimpNavigationControllerImpl::CanGoForward() const {
  NOTIMPLEMENTED();
  return true;
}

void BlimpNavigationControllerImpl::GoBack() {
  navigation_feature_->GoBack(kDummyTabId);
}

void BlimpNavigationControllerImpl::GoForward() {
  navigation_feature_->GoForward(kDummyTabId);
}

const GURL& BlimpNavigationControllerImpl::GetURL() {
  return current_url_;
}

const std::string& BlimpNavigationControllerImpl::GetTitle() {
  return current_title_;
}

void BlimpNavigationControllerImpl::OnUrlChanged(int tab_id, const GURL& url) {
  current_url_ = url;
  delegate_->OnNavigationStateChanged();
}

void BlimpNavigationControllerImpl::OnFaviconChanged(int tab_id,
                                                     const SkBitmap& favicon) {
  delegate_->OnNavigationStateChanged();
}

void BlimpNavigationControllerImpl::OnTitleChanged(int tab_id,
                                                   const std::string& title) {
  current_title_ = title;
  delegate_->OnNavigationStateChanged();
}

void BlimpNavigationControllerImpl::OnLoadingChanged(int tab_id, bool loading) {
  delegate_->OnNavigationStateChanged();
}

void BlimpNavigationControllerImpl::OnPageLoadStatusUpdate(int tab_id,
                                                           bool completed) {
  delegate_->OnNavigationStateChanged();
}

}  // namespace client
}  // namespace blimp
