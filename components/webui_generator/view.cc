// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webui_generator/view.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "components/webui_generator/view_model.h"

namespace webui_generator {

View::View(const std::string& id)
    : parent_(NULL),
      id_(id),
      ready_children_(0),
      view_model_ready_(false),
      ready_(false),
      weak_factory_(this) {
}

View::~View() {
}

void View::Init() {
  CreateAndAddChildren();

  if (!IsRootView())
    path_ = parent_->path() + "$";

  path_ += id_;
  view_model_.reset(CreateViewModel());
  view_model_->SetView(this);

  for (auto& id_child : children_)
    id_child.second->Init();

  if (children_.empty())
    OnChildrenReady();
}

bool View::IsRootView() const {
  return !parent_;
}

ViewModel* View::GetViewModel() const {
  return view_model_.get();
}

void View::OnViewModelReady() {
  view_model_ready_ = true;
  if (ready_children_ == static_cast<int>(children_.size()))
    OnReady();
}

View* View::GetChild(const std::string& id) const {
  auto it = children_.find(id);
  if (it == children_.end())
    return nullptr;

  return it->second;
}

void View::OnReady() {
  if (ready_)
    return;

  ready_ = true;
  if (!IsRootView())
    parent_->OnChildReady(this);
}

void View::UpdateContext(const base::DictionaryValue& diff) {
  DCHECK(ready_);
  view_model_->UpdateContext(diff);
}

void View::HandleEvent(const std::string& event) {
  DCHECK(ready_);
  view_model_->OnEvent(event);
}

void View::AddChild(View* child) {
  DCHECK(children_.find(child->id()) == children_.end());
  DCHECK(!child->id().empty());
  children_.set(child->id(), make_scoped_ptr(child));
  child->set_parent(this);
}

void View::OnChildReady(View* child) {
  ++ready_children_;
  if (ready_children_ == static_cast<int>(children_.size()))
    OnChildrenReady();
}

void View::OnChildrenReady() {
  view_model_->OnChildrenReady();
  if (view_model_ready_)
    OnReady();
}

}  // namespace webui_generator
