// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/intents/web_intent_picker_model.h"

#include <algorithm>

#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/ui/intents/web_intent_picker_model_observer.h"
#include "grit/ui_resources_standard.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

namespace {

const size_t kMaxSuggestionCount = 5;  // Maximum number of visible suggestions.

}  // namespace

WebIntentPickerModel::WebIntentPickerModel()
    : observer_(NULL) {
}

WebIntentPickerModel::~WebIntentPickerModel() {
  DestroyAll();
}

void WebIntentPickerModel::AddInstalledService(const string16& title,
                                               const GURL& url,
                                               Disposition disposition) {
  // TODO(groby): Revisit to remove O(n^2) complexity.
  for (size_t i = 0; i < installed_services_.size(); ++i) {
    InstalledService* service = installed_services_[i];
    if (service->title == title && service->url == url &&
        service->disposition == disposition)
      return;
  }
  installed_services_.push_back(new InstalledService(title, url, disposition));
  if (observer_)
    observer_->OnModelChanged(this);
}

void WebIntentPickerModel::RemoveInstalledServiceAt(size_t index) {
  DCHECK_LT(index, installed_services_.size());
  InstalledService* service = installed_services_[index];
  installed_services_.erase(installed_services_.begin() + index);
  delete service;
  if (observer_)
    observer_->OnModelChanged(this);
}

void WebIntentPickerModel::Clear() {
  DestroyAll();
  action_.clear();
  mimetype_.clear();
  inline_disposition_url_ = GURL::EmptyGURL();
  if (observer_)
    observer_->OnModelChanged(this);
}

const WebIntentPickerModel::InstalledService&
    WebIntentPickerModel::GetInstalledServiceAt(size_t index) const {
  DCHECK_LT(index, installed_services_.size());
  return *installed_services_[index];
}

const WebIntentPickerModel::InstalledService*
    WebIntentPickerModel::GetInstalledServiceWithURL(const GURL& url) const {
  for (size_t i = 0; i < installed_services_.size(); ++i) {
    InstalledService* service = installed_services_[i];
    if (service->url == url)
      return service;
  }
  return NULL;
}

size_t WebIntentPickerModel::GetInstalledServiceCount() const {
  return installed_services_.size();
}

void WebIntentPickerModel::UpdateFaviconAt(size_t index,
                                           const gfx::Image& image) {
  DCHECK_LT(index, installed_services_.size());
  installed_services_[index]->favicon = image;
  if (observer_)
    observer_->OnFaviconChanged(this, index);
}

void WebIntentPickerModel::AddSuggestedExtension(const string16& title,
                                                 const string16& id,
                                                 double average_rating) {
  suggested_extensions_.push_back(
      new SuggestedExtension(title, id, average_rating));
  if (observer_)
    observer_->OnModelChanged(this);
}

void WebIntentPickerModel::RemoveSuggestedExtensionAt(size_t index) {
  DCHECK_LT(index, suggested_extensions_.size());
  SuggestedExtension* extension = suggested_extensions_[index];
  suggested_extensions_.erase(suggested_extensions_.begin() + index);
  delete extension;
  if (observer_)
    observer_->OnModelChanged(this);
}

const WebIntentPickerModel::SuggestedExtension&
    WebIntentPickerModel::GetSuggestedExtensionAt(size_t index) const {
  DCHECK_LT(index, suggested_extensions_.size());
  return *suggested_extensions_[index];
}

size_t WebIntentPickerModel::GetSuggestedExtensionCount() const {
  return std::min(suggested_extensions_.size(), kMaxSuggestionCount);
}

void WebIntentPickerModel::SetSuggestedExtensionIconWithId(
    const string16& id,
    const gfx::Image& image) {
  for (size_t i = 0; i < suggested_extensions_.size(); ++i) {
    SuggestedExtension* extension = suggested_extensions_[i];
    if (extension->id == id) {
      extension->icon = image;

      if (observer_)
        observer_->OnExtensionIconChanged(this, extension->id);
      break;
    }
  }
}

void WebIntentPickerModel::SetInlineDisposition(const GURL& url) {
  inline_disposition_url_ = url;
  if (observer_) {
    const InstalledService* service = GetInstalledServiceWithURL(url);
    DCHECK(service);
    observer_->OnInlineDisposition(service->title, url);
  }
}

bool WebIntentPickerModel::IsInlineDisposition() const {
  return !inline_disposition_url_.is_empty();
}

void WebIntentPickerModel::DestroyAll() {
  STLDeleteElements(&installed_services_);
  STLDeleteElements(&suggested_extensions_);
}

WebIntentPickerModel::InstalledService::InstalledService(
    const string16& title,
    const GURL& url,
    Disposition disposition)
    : title(title),
      url(url),
      favicon(ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
          IDR_DEFAULT_FAVICON)),
      disposition(disposition) {
}

WebIntentPickerModel::InstalledService::~InstalledService() {
}

WebIntentPickerModel::SuggestedExtension::SuggestedExtension(
    const string16& title,
    const string16& id,
    double average_rating)
    : title(title),
      id(id),
      average_rating(average_rating),
      icon(ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
          IDR_DEFAULT_FAVICON)) {
}

WebIntentPickerModel::SuggestedExtension::~SuggestedExtension() {
}
