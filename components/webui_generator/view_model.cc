// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webui_generator/view_model.h"

#include "components/webui_generator/view.h"

namespace webui_generator {

ViewModel::ContextEditor::ContextEditor(ViewModel& view_model)
    : view_model_(view_model), context_(view_model_.context()) {
}

ViewModel::ContextEditor::~ContextEditor() {
  view_model_.CommitContextChanges();
}

const ViewModel::ContextEditor& ViewModel::ContextEditor::SetBoolean(
    const KeyType& key,
    bool value) const {
  context_.SetBoolean(key, value);
  return *this;
}

const ViewModel::ContextEditor& ViewModel::ContextEditor::SetInteger(
    const KeyType& key,
    int value) const {
  context_.SetInteger(key, value);
  return *this;
}

const ViewModel::ContextEditor& ViewModel::ContextEditor::SetDouble(
    const KeyType& key,
    double value) const {
  context_.SetDouble(key, value);
  return *this;
}

const ViewModel::ContextEditor& ViewModel::ContextEditor::SetString(
    const KeyType& key,
    const std::string& value) const {
  context_.SetString(key, value);
  return *this;
}

const ViewModel::ContextEditor& ViewModel::ContextEditor::SetString(
    const KeyType& key,
    const base::string16& value) const {
  context_.SetString(key, value);
  return *this;
}

const ViewModel::ContextEditor& ViewModel::ContextEditor::SetStringList(
    const KeyType& key,
    const StringList& value) const {
  context_.SetStringList(key, value);
  return *this;
}

const ViewModel::ContextEditor& ViewModel::ContextEditor::SetString16List(
    const KeyType& key,
    const String16List& value) const {
  context_.SetString16List(key, value);
  return *this;
}

ViewModel::ViewModel() : view_(nullptr) {
}

ViewModel::~ViewModel() {
}

void ViewModel::SetView(View* view) {
  view_ = view;
  Initialize();
}

void ViewModel::OnChildrenReady() {
  OnAfterChildrenReady();
  view_->OnViewModelReady();
}

void ViewModel::UpdateContext(const base::DictionaryValue& diff) {
  std::vector<std::string> changed_keys;
  context_.ApplyChanges(diff, &changed_keys);
  OnContextChanged(changed_keys);
}

void ViewModel::OnEvent(const std::string& event) {
}

ViewModel::ContextEditor ViewModel::GetContextEditor() {
  return ContextEditor(*this);
}

void ViewModel::CommitContextChanges() {
  if (!context().HasChanges())
    return;

  base::DictionaryValue diff;
  context().GetChangesAndReset(&diff);
  view_->OnContextChanged(diff);
}

}  // namespace webui_generator
