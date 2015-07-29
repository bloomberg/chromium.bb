// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/tab/frame.h"

#include <algorithm>

#include "base/stl_util.h"
#include "components/view_manager/public/cpp/view.h"
#include "components/view_manager/public/cpp/view_property.h"
#include "mandoline/tab/frame_tree.h"
#include "mandoline/tab/frame_tree_delegate.h"
#include "mandoline/tab/frame_user_data.h"

using mojo::View;

DECLARE_VIEW_PROPERTY_TYPE(mandoline::Frame*);

namespace mandoline {

// Used to find the Frame associated with a View.
DEFINE_LOCAL_VIEW_PROPERTY_KEY(Frame*, kFrame, nullptr);

namespace {

const uint32_t kNoParentId = 0u;

FrameDataPtr FrameToFrameData(const Frame* frame) {
  FrameDataPtr frame_data(FrameData::New());
  frame_data->frame_id = frame->id();
  frame_data->parent_id = frame->parent() ? frame->parent()->id() : kNoParentId;
  frame_data->name = frame->name();
  // TODO(sky): implement me.
  frame_data->origin = std::string();
  return frame_data.Pass();
}

}  // namespace

Frame::Frame(FrameTree* tree,
             View* view,
             uint32_t id,
             ViewOwnership view_ownership,
             FrameTreeClient* frame_tree_client,
             scoped_ptr<FrameUserData> user_data)
    : tree_(tree),
      view_(nullptr),
      id_(id),
      parent_(nullptr),
      view_ownership_(view_ownership),
      user_data_(user_data.Pass()),
      frame_tree_client_(frame_tree_client),
      loading_(false),
      progress_(0.f),
      frame_tree_server_binding_(this) {
  if (view)
    SetView(view);
}

Frame::~Frame() {
  if (view_)
    view_->RemoveObserver(this);
  while (!children_.empty())
    delete children_[0];
  if (parent_)
    parent_->Remove(this);
  if (view_) {
    view_->ClearLocalProperty(kFrame);
    if (view_ownership_ == ViewOwnership::OWNS_VIEW)
      view_->Destroy();
  }
}

void Frame::Init(Frame* parent) {
  if (parent)
    parent->Add(this);

  InitClient();
}

void Frame::Swap(FrameTreeClient* frame_tree_client,
                 scoped_ptr<FrameUserData> user_data) {
  while (!children_.empty())
    delete children_[0];

  user_data_ = user_data.Pass();
  frame_tree_client_ = frame_tree_client;
  frame_tree_server_binding_.Close();

  InitClient();
}

// static
Frame* Frame::FindFirstFrameAncestor(View* view) {
  while (view && !view->GetLocalProperty(kFrame))
    view = view->parent();
  return view ? view->GetLocalProperty(kFrame) : nullptr;
}

const Frame* Frame::FindFrame(uint32_t id) const {
  if (id == id_)
    return this;

  for (const Frame* child : children_) {
    const Frame* match = child->FindFrame(id);
    if (match)
      return match;
  }
  return nullptr;
}

bool Frame::HasAncestor(const Frame* frame) const {
  const Frame* current = this;
  while (current && current != frame)
    current = current->parent_;
  return current == frame;
}

bool Frame::IsLoading() const {
  if (loading_)
    return true;
  for (const Frame* child : children_) {
    if (child->IsLoading())
      return true;
  }
  return false;
}

double Frame::GatherProgress(int* frame_count) const {
  ++(*frame_count);
  double progress = progress_;
  for (const Frame* child : children_)
    progress += child->GatherProgress(frame_count);
  return progress_;
}

void Frame::InitClient() {
  std::vector<const Frame*> frames;
  tree_->root()->BuildFrameTree(&frames);

  mojo::Array<FrameDataPtr> array(frames.size());
  for (size_t i = 0; i < frames.size(); ++i)
    array[i] = FrameToFrameData(frames[i]).Pass();

  // TODO(sky): error handling.
  FrameTreeServerPtr frame_tree_server_ptr;
  frame_tree_server_binding_.Bind(GetProxy(&frame_tree_server_ptr).Pass());
  if (frame_tree_client_)
    frame_tree_client_->OnConnect(frame_tree_server_ptr.Pass(), array.Pass());
}

void Frame::SetView(mojo::View* view) {
  DCHECK(!view_);
  DCHECK_EQ(id_, view->id());
  view_ = view;
  view_->SetLocalProperty(kFrame, this);
  view_->AddObserver(this);
}

void Frame::BuildFrameTree(std::vector<const Frame*>* frames) const {
  frames->push_back(this);
  for (const Frame* frame : children_)
    frame->BuildFrameTree(frames);
}

void Frame::Add(Frame* node) {
  DCHECK(!node->parent_);

  node->parent_ = this;
  children_.push_back(node);

  tree_->root()->NotifyAdded(this, node);
}

void Frame::Remove(Frame* node) {
  DCHECK_EQ(node->parent_, this);
  auto iter = std::find(children_.begin(), children_.end(), node);
  DCHECK(iter != children_.end());
  node->parent_ = nullptr;
  children_.erase(iter);

  tree_->root()->NotifyRemoved(this, node);
}

void Frame::LoadingStartedImpl() {
  DCHECK(!loading_);
  loading_ = true;
  progress_ = 0.f;
  tree_->LoadingStateChanged();
}

void Frame::LoadingStoppedImpl() {
  DCHECK(loading_);
  loading_ = false;
  tree_->LoadingStateChanged();
}

void Frame::ProgressChangedImpl(double progress) {
  DCHECK(loading_);
  progress_ = progress;
  tree_->ProgressChanged();
}

void Frame::SetFrameNameImpl(const mojo::String& name) {
  if (name_ == name)
    return;

  name_ = name;
  tree_->FrameNameChanged(this);
}

Frame* Frame::FindTargetFrame(uint32_t frame_id) {
  if (frame_id == id_)
    return this;  // Common case.

  // TODO(sky): I need a way to sanity check frame_id here, but the connection
  // id isn't known to us.

  Frame* frame = FindFrame(frame_id);
  if (frame->frame_tree_client_) {
    // The frame has it's own client/server pair. It should make requests using
    // the server it has rather than an ancestor.
    DVLOG(1) << "ignore request for a frame that has its own client.";
    return nullptr;
  }

  return frame;
}

void Frame::NotifyAdded(const Frame* source, const Frame* added_node) {
  if (added_node == this)
    return;

  if (source != this && frame_tree_client_)
    frame_tree_client_->OnFrameAdded(FrameToFrameData(added_node));

  for (Frame* child : children_)
    child->NotifyAdded(source, added_node);
}

void Frame::NotifyRemoved(const Frame* source, const Frame* removed_node) {
  if (removed_node == this)
    return;

  if (source != this && frame_tree_client_)
    frame_tree_client_->OnFrameRemoved(removed_node->id());

  for (Frame* child : children_)
    child->NotifyRemoved(source, removed_node);
}

void Frame::NotifyFrameNameChanged(const Frame* source) {
  if (this != source && frame_tree_client_)
    frame_tree_client_->OnFrameNameChanged(source->id(), source->name_);

  for (Frame* child : children_)
    child->NotifyFrameNameChanged(source);
}

void Frame::OnTreeChanged(const TreeChangeParams& params) {
  if (params.new_parent && this == tree_->root()) {
    Frame* child_frame = FindFrame(params.target->id());
    if (child_frame && !child_frame->view_)
      child_frame->SetView(params.target);
  }
}

void Frame::OnViewDestroying(mojo::View* view) {
  if (parent_)
    parent_->Remove(this);

  // Reset |view_ownership_| so we don't attempt to delete |view_| in the
  // destructor.
  view_ownership_ = ViewOwnership::DOESNT_OWN_VIEW;

  // TODO(sky): Change browser to create a child for each FrameTree.
  if (tree_->root() == this) {
    view_->RemoveObserver(this);
    view_ = nullptr;
    return;
  }

  delete this;
}

void Frame::PostMessageEventToFrame(uint32_t frame_id, MessageEventPtr event) {
  Frame* target = tree_->root()->FindFrame(frame_id);
  if (!target ||
      !tree_->delegate_->CanPostMessageEventToFrame(this, target, event.get()))
    return;

  NOTIMPLEMENTED();
}

void Frame::LoadingStarted(uint32_t frame_id) {
  Frame* target_frame = FindTargetFrame(frame_id);
  if (target_frame)
    target_frame->LoadingStartedImpl();
}

void Frame::LoadingStopped(uint32_t frame_id) {
  Frame* target_frame = FindTargetFrame(frame_id);
  if (target_frame)
    target_frame->LoadingStoppedImpl();
}

void Frame::ProgressChanged(uint32_t frame_id, double progress) {
  Frame* target_frame = FindTargetFrame(frame_id);
  if (target_frame)
    target_frame->ProgressChangedImpl(progress);
}

void Frame::SetFrameName(uint32_t frame_id, const mojo::String& name) {
  Frame* target_frame = FindTargetFrame(frame_id);
  if (target_frame)
    target_frame->SetFrameNameImpl(name);
}

void Frame::OnCreatedFrame(uint32_t parent_id, uint32_t frame_id) {
  // TODO(sky): I need a way to verify the id. Unfortunately the code here
  // doesn't know the connection id of the embedder, so it's not possible to
  // do it.

  if (FindFrame(frame_id)) {
    // TODO(sky): kill connection here?
    DVLOG(1) << "OnCreatedLocalFrame supplied id of existing frame.";
    return;
  }

  Frame* parent_frame = FindFrame(parent_id);
  if (!parent_frame) {
    DVLOG(1) << "OnCreatedLocalFrame supplied invalid parent_id.";
    return;
  }

  if (parent_frame != this && parent_frame->frame_tree_client_) {
    DVLOG(1) << "OnCreatedLocalFrame supplied parent from another connection.";
    return;
  }

  tree_->CreateSharedFrame(parent_frame, frame_id);
}

void Frame::RequestNavigate(uint32_t frame_id,
                            mandoline::NavigationTarget target,
                            mojo::URLRequestPtr request) {
  Frame* frame = FindFrame(frame_id);
  if (!frame) {
    DVLOG(1) << "RequestNavigate for unknown frame " << frame_id;
    return;
  }
  if (tree_->delegate_)
    tree_->delegate_->RequestNavigate(frame, target, request.Pass());
}

void Frame::DidNavigateLocally(uint32_t frame_id, const mojo::String& url) {
  NOTIMPLEMENTED();
}

}  // namespace mandoline
