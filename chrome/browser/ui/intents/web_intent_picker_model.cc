// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/intents/web_intent_picker_model.h"

#include <algorithm>

#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/ui/intents/web_intent_picker_model_observer.h"
#include "content/public/browser/download_item.h"
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
      pending_extension_install_download_progress_(0),
      pending_extension_install_delegate_(NULL),
      show_use_another_service_(true) {
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
  ClearPendingExtensionInstall();
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

void WebIntentPickerModel::UpdateFaviconForServiceWithURL(
    const GURL& url, const gfx::Image& image) {
  for (size_t i = 0; i < installed_services_.size(); ++i) {
    InstalledService* service = installed_services_[i];
    if (service->url == url) {
      service->favicon = image;
      if (observer_)
        observer_->OnFaviconChanged(this, i);
      return;
    }
  }
  NOTREACHED();  // Calling this with an invalid URL is not allowed.
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

const WebIntentPickerModel::SuggestedExtension*
WebIntentPickerModel::GetSuggestedExtensionWithId(
    const std::string& id) const {
  for (size_t i = 0; i < suggested_extensions_.size(); ++i) {
    const SuggestedExtension* extension = &suggested_extensions_[i];
    if (extension->id == id)
      return extension;
  }
  return NULL;
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
    const std::string& id,
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
  if (!observer_)
    return;

  if (url.is_empty()) {
    observer_->OnModelChanged(this);
  } else {
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

void WebIntentPickerModel::SetPendingExtensionInstallId(const std::string& id) {
  pending_extension_install_id_ = id;
  if (observer_)
    observer_->OnModelChanged(this);
}

void WebIntentPickerModel::UpdateExtensionDownloadState(
    content::DownloadItem* item) {
  pending_extension_install_download_progress_ = item->PercentComplete();
  DownloadItemModel download_model(item);
  pending_extension_install_status_string_ = download_model.GetStatusText();
  if (observer_)
    observer_->OnModelChanged(this);
}

void WebIntentPickerModel::SetPendingExtensionInstallDownloadProgress(
    int progress) {
  pending_extension_install_download_progress_ = progress;
  if (observer_)
    observer_->OnModelChanged(this);
}

void WebIntentPickerModel::SetPendingExtensionInstallStatusString(
    const string16& status) {
  pending_extension_install_status_string_ = status;
  if (observer_)
    observer_->OnModelChanged(this);
}

void WebIntentPickerModel::SetPendingExtensionInstallDelegate(
    ExtensionInstallPrompt::Delegate* delegate) {
  pending_extension_install_delegate_ = delegate;
  if (observer_)
    observer_->OnModelChanged(this);
}

void WebIntentPickerModel::SetPendingExtensionInstallPrompt(
    const ExtensionInstallPrompt::Prompt& prompt) {
  pending_extension_install_prompt_.reset(
      new ExtensionInstallPrompt::Prompt(prompt));
  if (observer_)
    observer_->OnModelChanged(this);
}

void WebIntentPickerModel::ClearPendingExtensionInstall() {
  pending_extension_install_id_.clear();
  pending_extension_install_download_progress_ = 0;
  pending_extension_install_status_string_.clear();
  pending_extension_install_delegate_ = NULL;
  pending_extension_install_prompt_.reset();
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
    const std::string& id,
    double average_rating)
    : title(title),
      id(id),
      average_rating(average_rating),
      icon(ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
          IDR_DEFAULT_FAVICON)) {
}

WebIntentPickerModel::SuggestedExtension::~SuggestedExtension() {
}
