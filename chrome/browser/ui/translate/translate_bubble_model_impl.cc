// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/translate/translate_bubble_model_impl.h"

#include "chrome/browser/translate/translate_ui_delegate.h"

TranslateBubbleModelImpl::TranslateBubbleModelImpl(
    TranslateBubbleModel::ViewState view_type,
    scoped_ptr<TranslateUIDelegate> ui_delegate)
    : ui_delegate_(ui_delegate.Pass()),
      view_state_transition_(view_type) {
}

TranslateBubbleModelImpl::~TranslateBubbleModelImpl() {
}

TranslateBubbleModel::ViewState TranslateBubbleModelImpl::GetViewState() const {
  return view_state_transition_.view_state();
}

void TranslateBubbleModelImpl::SetViewState(
    TranslateBubbleModel::ViewState view_state) {
  view_state_transition_.SetViewState(view_state);
}

void TranslateBubbleModelImpl::GoBackFromAdvanced() {
  view_state_transition_.GoBackFromAdvanced();
}

int TranslateBubbleModelImpl::GetNumberOfLanguages() const {
  return ui_delegate_->GetNumberOfLanguages();
}

string16 TranslateBubbleModelImpl::GetLanguageNameAt(int index) const {
  return ui_delegate_->GetLanguageNameAt(index);
}

int TranslateBubbleModelImpl::GetOriginalLanguageIndex() const {
  return ui_delegate_->GetOriginalLanguageIndex();
}

void TranslateBubbleModelImpl::SetOriginalLanguageIndex(int index) {
  ui_delegate_->SetOriginalLanguageIndex(index);
}

int TranslateBubbleModelImpl::GetTargetLanguageIndex() const {
  return ui_delegate_->GetTargetLanguageIndex();
}

void TranslateBubbleModelImpl::SetTargetLanguageIndex(int index) {
  ui_delegate_->SetTargetLanguageIndex(index);
}

void TranslateBubbleModelImpl::SetNeverTranslateLanguage(bool value) {
  ui_delegate_->SetLanguageBlocked(value);
}

void TranslateBubbleModelImpl::SetNeverTranslateSite(bool value) {
  ui_delegate_->SetSiteBlacklist(value);
}

bool TranslateBubbleModelImpl::ShouldAlwaysTranslate() const {
  return ui_delegate_->ShouldAlwaysTranslate();
}

void TranslateBubbleModelImpl::SetAlwaysTranslate(bool value) {
  ui_delegate_->SetAlwaysTranslate(value);
}

void TranslateBubbleModelImpl::Translate() {
  ui_delegate_->Translate();
}

void TranslateBubbleModelImpl::RevertTranslation() {
  ui_delegate_->RevertTranslation();
}

void TranslateBubbleModelImpl::TranslationDeclined() {
  ui_delegate_->TranslationDeclined();
}
