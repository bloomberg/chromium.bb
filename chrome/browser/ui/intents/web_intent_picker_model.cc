// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/intents/web_intent_picker_model.h"

#include <algorithm>

#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/ui/intents/web_intent_picker_model_observer.h"
#include "grit/generated_resources.h"
#include "grit/ui_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

namespace {

const size_t kMaxSuggestionCount = 5;  // Maximum number of visible suggestions.

}  // namespace

WebIntentPickerModel::WebIntentPickerModel()
    : observer_(NULL),
      waiting_for_suggestions_(true),
      default_service_hash_(0) {
}

WebIntentPickerModel::~WebIntentPickerModel() {
  DestroyAll();
}

void WebIntentPickerModel::AddInstalledService(
    const string16& title,
    const GURL& url,
    webkit_glue::WebIntentServiceData::Disposition disposition) {
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
  type_.clear();
  inline_disposition_url_ = GURL::EmptyGURL();
  waiting_for_suggestions_ = true;
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

void WebIntentPickerModel::AddSuggestedExtensions(
    const std::vector<SuggestedExtension>& suggestions) {
  suggested_extensions_.insert(suggested_extensions_.end(),
                               suggestions.begin(),
                               suggestions.end());
  if (observer_)
    observer_->OnModelChanged(this);
}

const WebIntentPickerModel::SuggestedExtension&
    WebIntentPickerModel::GetSuggestedExtensionAt(size_t index) const {
  DCHECK_LT(index, suggested_extensions_.size());
  return suggested_extensions_[index];
}

size_t WebIntentPickerModel::GetSuggestedExtensionCount() const {
  return std::min(suggested_extensions_.size(), kMaxSuggestionCount);
}

string16 WebIntentPickerModel::GetSuggestionsLinkText() const {
  if (suggested_extensions_.size() <= kMaxSuggestionCount)
    return string16();

  return l10n_util::GetStringUTF16(GetInstalledServiceCount() == 0 ?
      IDS_INTENT_PICKER_GET_MORE_SERVICES_NONE_INSTALLED :
      IDS_INTENT_PICKER_GET_MORE_SERVICES);
}

void WebIntentPickerModel::SetSuggestedExtensionIconWithId(
    const string16& id,
    const gfx::Image& image) {
  for (size_t i = 0; i < suggested_extensions_.size(); ++i) {
    SuggestedExtension& extension = suggested_extensions_[i];
    if (extension.id == id) {
      extension.icon = image;

      if (observer_)
        observer_->OnExtensionIconChanged(this, extension.id);
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

bool WebIntentPickerModel::IsWaitingForSuggestions() const {
  return waiting_for_suggestions_;
}

void WebIntentPickerModel::SetWaitingForSuggestions(bool waiting) {
  waiting_for_suggestions_ = waiting;
  if (observer_)
    observer_->OnModelChanged(this);
}

void WebIntentPickerModel::DestroyAll() {
  STLDeleteElements(&installed_services_);
}

WebIntentPickerModel::InstalledService::InstalledService(
    const string16& title,
    const GURL& url,
    webkit_glue::WebIntentServiceData::Disposition disposition)
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
