// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/intents/web_intent_picker_model.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/ui/intents/web_intent_picker_model_observer.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

WebIntentPickerModel::WebIntentPickerModel()
    : observer_(NULL),
      inline_disposition_index_(std::string::npos) {
}

WebIntentPickerModel::~WebIntentPickerModel() {
  DestroyAll();
}

void WebIntentPickerModel::AddItem(const string16& title,
                                   const GURL& url,
                                   Disposition disposition) {
  items_.push_back(new Item(title, url, disposition));
  if (observer_)
    observer_->OnModelChanged(this);
}

void WebIntentPickerModel::RemoveItemAt(size_t index) {
  DCHECK(index < items_.size());
  std::vector<Item*>::iterator iter = items_.begin() + index;
  delete *iter;
  items_.erase(iter);
  if (observer_)
    observer_->OnModelChanged(this);
}

void WebIntentPickerModel::Clear() {
  DestroyAll();
  inline_disposition_index_ = std::string::npos;
  if (observer_)
    observer_->OnModelChanged(this);
}

const WebIntentPickerModel::Item& WebIntentPickerModel::GetItemAt(
    size_t index) const {
  DCHECK(index < items_.size());
  return *items_[index];
}

size_t WebIntentPickerModel::GetItemCount() const {
  return items_.size();
}

void WebIntentPickerModel::UpdateFaviconAt(size_t index,
                                           const gfx::Image& image) {
  DCHECK(index < items_.size());
  items_[index]->favicon = image;
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
  DCHECK(index < suggested_extensions_.size());
  std::vector<SuggestedExtension*>::iterator iter =
      suggested_extensions_.begin() + index;
  delete *iter;
  suggested_extensions_.erase(iter);
  if (observer_)
    observer_->OnModelChanged(this);
}

const WebIntentPickerModel::SuggestedExtension&
    WebIntentPickerModel::GetSuggestedExtensionAt(size_t index) const {
  DCHECK(index < suggested_extensions_.size());
  return *suggested_extensions_[index];
}

size_t WebIntentPickerModel::GetSuggestedExtensionCount() const {
  return suggested_extensions_.size();
}

void WebIntentPickerModel::SetInlineDisposition(size_t index) {
  DCHECK(index < items_.size());
  inline_disposition_index_ = index;
  if (observer_)
    observer_->OnInlineDisposition(this);
}

bool WebIntentPickerModel::IsInlineDisposition() const {
  return inline_disposition_index_ != std::string::npos;
}

void WebIntentPickerModel::DestroyAll() {
  STLDeleteElements(&items_);
  STLDeleteElements(&suggested_extensions_);
}

WebIntentPickerModel::Item::Item(const string16& title,
                                 const GURL& url,
                                 Disposition disposition)
    : title(title),
      url(url),
      favicon(ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
          IDR_DEFAULT_FAVICON)),
      disposition(disposition) {
}

WebIntentPickerModel::Item::~Item() {
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
