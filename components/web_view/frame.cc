// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_view/frame.h"

#include <algorithm>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/stl_util.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_property.h"
#include "components/web_view/frame_tree.h"
#include "components/web_view/frame_tree_delegate.h"
#include "components/web_view/frame_user_data.h"
#include "components/web_view/frame_utils.h"
#include "mojo/application/public/interfaces/shell.mojom.h"
#include "mojo/common/url_type_converters.h"
#include "url/gurl.h"

using mus::Window;

DECLARE_WINDOW_PROPERTY_TYPE(web_view::Frame*);

namespace web_view {

// Used to find the Frame associated with a Window.
DEFINE_LOCAL_WINDOW_PROPERTY_KEY(Frame*, kFrame, nullptr);

namespace {

const uint32_t kNoParentId = 0u;
const mus::ConnectionSpecificId kInvalidConnectionId = 0u;

mojom::FrameDataPtr FrameToFrameData(const Frame* frame) {
  mojom::FrameDataPtr frame_data(mojom::FrameData::New());
  frame_data->frame_id = frame->id();
  frame_data->parent_id = frame->parent() ? frame->parent()->id() : kNoParentId;
  frame_data->client_properties =
      mojo::Map<mojo::String, mojo::Array<uint8_t>>::From(
          frame->client_properties());
  return frame_data.Pass();
}

}  // namespace

struct Frame::FrameUserDataAndBinding {
  scoped_ptr<FrameUserData> user_data;
  scoped_ptr<mojo::Binding<mojom::Frame>> frame_binding;
};

Frame::Frame(FrameTree* tree,
             Window* window,
             uint32_t frame_id,
             uint32_t app_id,
             WindowOwnership window_ownership,
             mojom::FrameClient* frame_client,
             scoped_ptr<FrameUserData> user_data,
             const ClientPropertyMap& client_properties)
    : tree_(tree),
      window_(nullptr),
      embedded_connection_id_(kInvalidConnectionId),
      id_(frame_id),
      app_id_(app_id),
      parent_(nullptr),
      window_ownership_(window_ownership),
      user_data_(user_data.Pass()),
      frame_client_(frame_client),
      loading_(false),
      progress_(0.f),
      client_properties_(client_properties),
      waiting_for_on_will_navigate_ack_(false),
      embed_weak_ptr_factory_(this),
      navigate_weak_ptr_factory_(this) {
  if (window)
    SetWindow(window);
}

Frame::~Frame() {
  DVLOG(2) << "~Frame id=" << id_ << " this=" << this;
  if (window_)
    window_->RemoveObserver(this);
  while (!children_.empty())
    delete children_[0];
  if (parent_)
    parent_->Remove(this);
  if (window_) {
    window_->ClearLocalProperty(kFrame);
    if (window_ownership_ == WindowOwnership::OWNS_WINDOW)
      window_->Destroy();
  }
  tree_->delegate_->DidDestroyFrame(this);
}

void Frame::Init(Frame* parent,
                 mus::mojom::WindowTreeClientPtr window_tree_client,
                 mojo::InterfaceRequest<mojom::Frame> frame_request,
                 base::TimeTicks navigation_start_time) {
  {
    // Set the FrameClient to null so that we don't notify the client of the
    // add before OnConnect().
    base::AutoReset<mojom::FrameClient*> frame_client_resetter(&frame_client_,
                                                               nullptr);
    if (parent)
      parent->Add(this);
  }

  const ClientType client_type = frame_request.is_pending()
                                     ? ClientType::NEW_CHILD_FRAME
                                     : ClientType::EXISTING_FRAME_NEW_APP;
  InitClient(client_type, nullptr, window_tree_client.Pass(),
             frame_request.Pass(), navigation_start_time);

  tree_->delegate_->DidCreateFrame(this);

  DVLOG(2) << "Frame id=" << id_ << " parent=" << (parent_ ? parent_->id_ : 0)
           << " app_id=" << app_id_ << " this=" << this;
}

// static
Frame* Frame::FindFirstFrameAncestor(Window* window) {
  while (window && !window->GetLocalProperty(kFrame))
    window = window->parent();
  return window ? window->GetLocalProperty(kFrame) : nullptr;
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

void Frame::Find(int32 request_id,
                 const mojo::String& search_text,
                 mojom::FindOptionsPtr options,
                 bool wrap_within_frame,
                 const FindCallback& callback) {
  frame_client_->Find(request_id, search_text, options.Pass(),
                      wrap_within_frame, callback);
}

void Frame::StopFinding(bool clear_selection) {
  frame_client_->StopFinding(clear_selection);
}

void Frame::HighlightFindResults(int32_t request_id,
                                 const mojo::String& search_text,
                                 mojom::FindOptionsPtr options,
                                 bool reset) {
  frame_client_->HighlightFindResults(request_id, search_text, options.Pass(),
                                      reset);
}

void Frame::StopHighlightingFindResults() {
  frame_client_->StopHighlightingFindResults();
}

void Frame::InitClient(ClientType client_type,
                       scoped_ptr<FrameUserDataAndBinding> data_and_binding,
                       mus::mojom::WindowTreeClientPtr window_tree_client,
                       mojo::InterfaceRequest<mojom::Frame> frame_request,
                       base::TimeTicks navigation_start_time) {
  if (client_type == ClientType::EXISTING_FRAME_NEW_APP &&
      window_tree_client.get()) {
    embedded_connection_id_ = kInvalidConnectionId;
    embed_weak_ptr_factory_.InvalidateWeakPtrs();
    window_->Embed(
        window_tree_client.Pass(),
        mus::mojom::WindowTree::ACCESS_POLICY_DEFAULT,
        base::Bind(&Frame::OnEmbedAck, embed_weak_ptr_factory_.GetWeakPtr()));
  }

  if (client_type == ClientType::NEW_CHILD_FRAME) {
    // Don't install an error handler. We allow for the target to only
    // implement WindowTreeClient.
    // This frame (and client) was created by an existing FrameClient. There
    // is no need to send it OnConnect().
    frame_binding_.reset(
        new mojo::Binding<mojom::Frame>(this, frame_request.Pass()));
    frame_client_->OnConnect(
        nullptr, tree_->change_id(), id_, mojom::WINDOW_CONNECT_TYPE_USE_NEW,
        mojo::Array<mojom::FrameDataPtr>(),
        navigation_start_time.ToInternalValue(),
        base::Bind(&OnConnectAck, base::Passed(&data_and_binding)));
  } else {
    std::vector<const Frame*> frames;
    tree_->root()->BuildFrameTree(&frames);

    mojo::Array<mojom::FrameDataPtr> array(frames.size());
    for (size_t i = 0; i < frames.size(); ++i)
      array[i] = FrameToFrameData(frames[i]).Pass();

    mojom::FramePtr frame_ptr;
    // Don't install an error handler. We allow for the target to only
    // implement WindowTreeClient.
    frame_binding_.reset(
        new mojo::Binding<mojom::Frame>(this, GetProxy(&frame_ptr).Pass()));
    frame_client_->OnConnect(
        frame_ptr.Pass(), tree_->change_id(), id_,
        client_type == ClientType::EXISTING_FRAME_SAME_APP
            ? mojom::WINDOW_CONNECT_TYPE_USE_EXISTING
            : mojom::WINDOW_CONNECT_TYPE_USE_NEW,
        array.Pass(), navigation_start_time.ToInternalValue(),
        base::Bind(&OnConnectAck, base::Passed(&data_and_binding)));
    tree_->delegate_->DidStartNavigation(this);

    // We need |embedded_connection_id_| is order to validate requests to
    // create a child frame (OnCreatedFrame()). Pause incoming methods until
    // we get the id to prevent race conditions.
    if (embedded_connection_id_ == kInvalidConnectionId)
      frame_binding_->PauseIncomingMethodCallProcessing();
  }
}

// static
void Frame::OnConnectAck(scoped_ptr<FrameUserDataAndBinding> data_and_binding) {
}

void Frame::ChangeClient(mojom::FrameClient* frame_client,
                         scoped_ptr<FrameUserData> user_data,
                         mus::mojom::WindowTreeClientPtr window_tree_client,
                         uint32_t app_id,
                         base::TimeTicks navigation_start_time) {
  while (!children_.empty())
    delete children_[0];

  ClientType client_type = window_tree_client.get() == nullptr
                               ? ClientType::EXISTING_FRAME_SAME_APP
                               : ClientType::EXISTING_FRAME_NEW_APP;
  scoped_ptr<FrameUserDataAndBinding> data_and_binding;

  if (client_type == ClientType::EXISTING_FRAME_SAME_APP) {
    // See comment in InitClient() for details.
    data_and_binding.reset(new FrameUserDataAndBinding);
    data_and_binding->user_data = user_data_.Pass();
    data_and_binding->frame_binding = frame_binding_.Pass();
  } else {
    loading_ = false;
    progress_ = 0.f;
  }

  user_data_ = user_data.Pass();
  frame_client_ = frame_client;
  frame_binding_.reset();
  app_id_ = app_id;

  InitClient(client_type, data_and_binding.Pass(), window_tree_client.Pass(),
             nullptr, navigation_start_time);
}

void Frame::OnEmbedAck(bool success, mus::ConnectionSpecificId connection_id) {
  if (success)
    embedded_connection_id_ = connection_id;
  if (frame_binding_->is_bound())
    frame_binding_->ResumeIncomingMethodCallProcessing();
}

void Frame::OnWillNavigateAck(
    mojom::FrameClient* frame_client,
    scoped_ptr<FrameUserData> user_data,
    mus::mojom::WindowTreeClientPtr window_tree_client,
    uint32 app_id,
    base::TimeTicks navigation_start_time) {
  DCHECK(waiting_for_on_will_navigate_ack_);
  DVLOG(2) << "Frame::OnWillNavigateAck id=" << id_;
  waiting_for_on_will_navigate_ack_ = false;
  ChangeClient(frame_client, user_data.Pass(), window_tree_client.Pass(),
               app_id, navigation_start_time);
  if (pending_navigate_.get())
    StartNavigate(pending_navigate_.Pass());
}

void Frame::SetWindow(mus::Window* window) {
  DCHECK(!window_);
  DCHECK_EQ(id_, window->id());
  window_ = window;
  window_->SetLocalProperty(kFrame, this);
  window_->AddObserver(this);
  if (pending_navigate_.get())
    StartNavigate(pending_navigate_.Pass());
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

  tree_->root()->NotifyAdded(this, node, tree_->AdvanceChangeID());
}

void Frame::Remove(Frame* node) {
  DCHECK_EQ(node->parent_, this);
  auto iter = std::find(children_.begin(), children_.end(), node);
  DCHECK(iter != children_.end());
  node->parent_ = nullptr;
  children_.erase(iter);

  tree_->root()->NotifyRemoved(this, node, tree_->AdvanceChangeID());
}

void Frame::StartNavigate(mojo::URLRequestPtr request) {
  if (waiting_for_on_will_navigate_ack_) {
    // We're waiting for OnWillNavigate(). We need to wait until that completes
    // before attempting any other loads.
    pending_navigate_ = request.Pass();
    return;
  }

  pending_navigate_.reset();

  // We need a Window to navigate. When we get the Window we'll complete the
  // navigation.
  if (!window_) {
    pending_navigate_ = request.Pass();
    return;
  }

  // Drop any pending navigation requests.
  navigate_weak_ptr_factory_.InvalidateWeakPtrs();

  DVLOG(2) << "Frame::StartNavigate id=" << id_ << " url=" << request->url;

  const GURL requested_url(request->url);
  base::TimeTicks navigation_start_time =
      base::TimeTicks::FromInternalValue(request->originating_time_ticks);
  tree_->delegate_->CanNavigateFrame(
      this, request.Pass(), base::Bind(&Frame::OnCanNavigateFrame,
                                       navigate_weak_ptr_factory_.GetWeakPtr(),
                                       requested_url, navigation_start_time));
}

void Frame::OnCanNavigateFrame(
    const GURL& url,
    base::TimeTicks navigation_start_time,
    uint32_t app_id,
    mojom::FrameClient* frame_client,
    scoped_ptr<FrameUserData> user_data,
    mus::mojom::WindowTreeClientPtr window_tree_client) {
  DVLOG(2) << "Frame::OnCanNavigateFrame id=" << id_
           << " equal=" << (AreAppIdsEqual(app_id, app_id_) ? "true" : "false");
  if (AreAppIdsEqual(app_id, app_id_)) {
    // The app currently rendering the frame will continue rendering it. In this
    // case we do not use the WindowTreeClient (because the app has a Window
    // already
    // and ends up reusing it).
    DCHECK(!window_tree_client.get());
    ChangeClient(frame_client, user_data.Pass(), window_tree_client.Pass(),
                 app_id, navigation_start_time);
  } else {
    waiting_for_on_will_navigate_ack_ = true;
    DCHECK(window_tree_client.get());
    // TODO(sky): url isn't correct here, it should be a security origin.
    frame_client_->OnWillNavigate(
        url.spec(),
        base::Bind(&Frame::OnWillNavigateAck, base::Unretained(this),
                   frame_client, base::Passed(&user_data),
                   base::Passed(&window_tree_client), app_id,
                   navigation_start_time));
  }
}

void Frame::NotifyAdded(const Frame* source,
                        const Frame* added_node,
                        uint32_t change_id) {
  // |frame_client_| may be null during initial frame creation and parenting.
  if (frame_client_)
    frame_client_->OnFrameAdded(change_id, FrameToFrameData(added_node));

  for (Frame* child : children_)
    child->NotifyAdded(source, added_node, change_id);
}

void Frame::NotifyRemoved(const Frame* source,
                          const Frame* removed_node,
                          uint32_t change_id) {
  frame_client_->OnFrameRemoved(change_id, removed_node->id());

  for (Frame* child : children_)
    child->NotifyRemoved(source, removed_node, change_id);
}

void Frame::NotifyClientPropertyChanged(const Frame* source,
                                        const mojo::String& name,
                                        const mojo::Array<uint8_t>& value) {
  if (this != source)
    frame_client_->OnFrameClientPropertyChanged(source->id(), name,
                                                value.Clone());

  for (Frame* child : children_)
    child->NotifyClientPropertyChanged(source, name, value);
}

void Frame::NotifyFrameLoadingStateChanged(const Frame* frame, bool loading) {
  frame_client_->OnFrameLoadingStateChanged(frame->id(), loading);
}

void Frame::NotifyDispatchFrameLoadEvent(const Frame* frame) {
  frame_client_->OnDispatchFrameLoadEvent(frame->id());
}

void Frame::OnTreeChanged(const TreeChangeParams& params) {
  if (params.new_parent && this == tree_->root()) {
    Frame* child_frame = FindFrame(params.target->id());
    if (child_frame && !child_frame->window_)
      child_frame->SetWindow(params.target);
  }
}

void Frame::OnWindowDestroying(mus::Window* window) {
  if (parent_)
    parent_->Remove(this);

  // Reset |window_ownership_| so we don't attempt to delete |window_| in the
  // destructor.
  window_ownership_ = WindowOwnership::DOESNT_OWN_WINDOW;

  if (tree_->root() == this) {
    window_->RemoveObserver(this);
    window_ = nullptr;
    return;
  }

  delete this;
}

void Frame::OnWindowEmbeddedAppDisconnected(mus::Window* window) {
  // See FrameTreeDelegate::OnWindowEmbeddedAppDisconnected() for details of
  // when
  // this happens.
  //
  // Currently we have no way to distinguish between the cases that lead to this
  // being called, so we assume we can continue on. Continuing on is important
  // for html as it's entirely possible for a page to create a frame, navigate
  // to a bogus url and expect the frame to still exist.
  tree_->delegate_->OnWindowEmbeddedInFrameDisconnected(this);
}

void Frame::PostMessageEventToFrame(uint32_t target_frame_id,
                                    mojom::HTMLMessageEventPtr event) {
  // NOTE: |target_frame_id| is allowed to be from another connection.
  Frame* target = tree_->root()->FindFrame(target_frame_id);
  if (!target || target == this ||
      !tree_->delegate_->CanPostMessageEventToFrame(this, target, event.get()))
    return;

  target->frame_client_->OnPostMessageEvent(id_, target_frame_id, event.Pass());
}

void Frame::LoadingStateChanged(bool loading, double progress) {
  bool loading_state_changed = loading_ != loading;
  loading_ = loading;
  progress_ = progress;
  tree_->LoadingStateChanged();

  if (loading_state_changed && parent_ &&
      !AreAppIdsEqual(app_id_, parent_->app_id_)) {
    // We need to notify the parent if it is in a different app, so that it can
    // keep track of this frame's loading state. If the parent is in the same
    // app, we assume that the loading state is propagated directly within the
    // app itself and no notification is needed from our side.
    parent_->NotifyFrameLoadingStateChanged(this, loading_);
  }
}

void Frame::TitleChanged(const mojo::String& title) {
  // Only care about title changes on the root frame.
  if (!parent_)
    tree_->TitleChanged(title);
}

void Frame::DidCommitProvisionalLoad() {
  tree_->DidCommitProvisionalLoad(this);
}

void Frame::SetClientProperty(const mojo::String& name,
                              mojo::Array<uint8_t> value) {
  auto iter = client_properties_.find(name);
  const bool already_in_map = (iter != client_properties_.end());
  if (value.is_null()) {
    if (!already_in_map)
      return;
    client_properties_.erase(iter);
  } else {
    std::vector<uint8_t> as_vector(value.To<std::vector<uint8_t>>());
    if (already_in_map && iter->second == as_vector)
      return;
    client_properties_[name] = as_vector;
  }
  tree_->ClientPropertyChanged(this, name, value);
}

void Frame::OnCreatedFrame(
    mojo::InterfaceRequest<mojom::Frame> frame_request,
    mojom::FrameClientPtr client,
    uint32_t frame_id,
    mojo::Map<mojo::String, mojo::Array<uint8_t>> client_properties) {
  if ((frame_id >> 16) != embedded_connection_id_) {
    // TODO(sky): kill connection here?
    DVLOG(1) << "OnCreatedFrame supplied invalid frame id, expecting"
             << embedded_connection_id_;
    return;
  }

  if (FindFrame(frame_id)) {
    // TODO(sky): kill connection here?
    DVLOG(1) << "OnCreatedFrame supplied id of existing frame.";
    return;
  }

  Frame* child_frame = tree_->CreateChildFrame(
      this, frame_request.Pass(), client.Pass(), frame_id, app_id_,
      client_properties.To<ClientPropertyMap>());
  child_frame->embedded_connection_id_ = embedded_connection_id_;
}

void Frame::RequestNavigate(mojom::NavigationTargetType target_type,
                            uint32_t target_frame_id,
                            mojo::URLRequestPtr request) {
  if (target_type == mojom::NAVIGATION_TARGET_TYPE_EXISTING_FRAME) {
    // |target_frame| is allowed to come from another connection.
    Frame* target_frame = tree_->root()->FindFrame(target_frame_id);
    if (!target_frame) {
      DVLOG(1) << "RequestNavigate EXISTING_FRAME with no matching frame";
      return;
    }
    if (target_frame != tree_->root()) {
      target_frame->StartNavigate(request.Pass());
      return;
    }
    // Else case if |target_frame| == root. Treat at top level request.
  }
  tree_->delegate_->NavigateTopLevel(this, request.Pass());
}

void Frame::DidNavigateLocally(const mojo::String& url) {
  tree_->DidNavigateLocally(this, url.To<GURL>());
}

void Frame::DispatchLoadEventToParent() {
  if (parent_ && !AreAppIdsEqual(app_id_, parent_->app_id_)) {
    // Send notification to fire a load event in the parent, if the parent is in
    // a different app. If the parent is in the same app, we assume that the app
    // itself handles firing load event directly and no notification is needed
    // from our side.
    parent_->NotifyDispatchFrameLoadEvent(this);
  }
}

void Frame::OnFindInFrameCountUpdated(int32_t request_id,
                                      int32_t count,
                                      bool final_update) {
  tree_->delegate_->OnFindInFrameCountUpdated(request_id, this, count,
                                              final_update);
}

void Frame::OnFindInPageSelectionUpdated(int32_t request_id,
                                         int32_t active_match_ordinal) {
  tree_->delegate_->OnFindInPageSelectionUpdated(request_id, this,
                                                 active_match_ordinal);
}

}  // namespace web_view
