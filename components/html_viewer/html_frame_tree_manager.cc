// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/html_frame_tree_manager.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/logging.h"
#include "components/html_viewer/blink_basic_type_converters.h"
#include "components/html_viewer/blink_url_request_type_converters.h"
#include "components/html_viewer/document_resource_waiter.h"
#include "components/html_viewer/global_state.h"
#include "components/html_viewer/html_factory.h"
#include "components/html_viewer/html_frame.h"
#include "components/html_viewer/html_frame_delegate.h"
#include "components/html_viewer/html_frame_tree_manager_observer.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/web_view/web_view_switches.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebRemoteFrame.h"
#include "third_party/WebKit/public/web/WebTreeScopeType.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size.h"

namespace html_viewer {
namespace {

// Returns the index of the FrameData with the id of |frame_id| in |index|. On
// success returns true, otherwise false.
bool FindFrameDataIndex(
    const mojo::Array<web_view::mojom::FrameDataPtr>& frame_data,
    uint32_t frame_id,
    size_t* index) {
  for (size_t i = 0; i < frame_data.size(); ++i) {
    if (frame_data[i]->frame_id == frame_id) {
      *index = i;
      return true;
    }
  }
  return false;
}

}  // namespace

// Object that calls OnHTMLFrameTreeManagerChangeIdAdvanced() from the
// destructor.
class HTMLFrameTreeManager::ChangeIdAdvancedNotifier {
 public:
  explicit ChangeIdAdvancedNotifier(
      base::ObserverList<HTMLFrameTreeManagerObserver>* observers)
      : observers_(observers) {}
  ~ChangeIdAdvancedNotifier() {
    FOR_EACH_OBSERVER(HTMLFrameTreeManagerObserver, *observers_,
                      OnHTMLFrameTreeManagerChangeIdAdvanced());
  }

 private:
  base::ObserverList<HTMLFrameTreeManagerObserver>* observers_;

  DISALLOW_COPY_AND_ASSIGN(ChangeIdAdvancedNotifier);
};

// static
HTMLFrameTreeManager::TreeMap* HTMLFrameTreeManager::instances_ = nullptr;

// static
HTMLFrame* HTMLFrameTreeManager::CreateFrameAndAttachToTree(
    GlobalState* global_state,
    mus::Window* window,
    scoped_ptr<DocumentResourceWaiter> resource_waiter,
    HTMLFrameDelegate* delegate) {
  if (!instances_)
    instances_ = new TreeMap;

  mojo::InterfaceRequest<web_view::mojom::FrameClient> frame_client_request;
  web_view::mojom::FramePtr server_frame;
  mojo::Array<web_view::mojom::FrameDataPtr> frame_data;
  uint32_t change_id;
  uint32_t window_id;
  web_view::mojom::WindowConnectType window_connect_type;
  web_view::mojom::FrameClient::OnConnectCallback on_connect_callback;
  resource_waiter->Release(&frame_client_request, &server_frame, &frame_data,
                           &change_id, &window_id, &window_connect_type,
                           &on_connect_callback);
  resource_waiter.reset();

  on_connect_callback.Run();

  HTMLFrameTreeManager* frame_tree =
      FindFrameTreeWithRoot(frame_data[0]->frame_id);

  DCHECK(!frame_tree || change_id <= frame_tree->change_id_);

  DVLOG(2) << "HTMLFrameTreeManager::CreateFrameAndAttachToTree "
           << " frame_tree=" << frame_tree << " use_existing="
           << (window_connect_type ==
               web_view::mojom::WINDOW_CONNECT_TYPE_USE_EXISTING)
           << " frame_id=" << window_id;
  if (window_connect_type ==
          web_view::mojom::WINDOW_CONNECT_TYPE_USE_EXISTING &&
      !frame_tree) {
    DVLOG(1) << "was told to use existing window but do not have frame tree";
    return nullptr;
  }

  if (!frame_tree) {
    frame_tree = new HTMLFrameTreeManager(global_state);
    frame_tree->Init(delegate, window, frame_data, change_id);
    (*instances_)[frame_data[0]->frame_id] = frame_tree;
  } else if (window_connect_type ==
             web_view::mojom::WINDOW_CONNECT_TYPE_USE_EXISTING) {
    HTMLFrame* existing_frame = frame_tree->root_->FindFrame(window_id);
    if (!existing_frame) {
      DVLOG(1) << "was told to use existing window but could not find window";
      return nullptr;
    }
    if (!existing_frame->IsLocal()) {
      DVLOG(1) << "was told to use existing window, but frame is remote";
      return nullptr;
    }
    existing_frame->SwapDelegate(delegate);
  } else {
    // We're going to share a frame tree. We should know about the frame.
    CHECK(window->id() != frame_data[0]->frame_id);
    HTMLFrame* existing_frame = frame_tree->root_->FindFrame(window->id());
    if (existing_frame) {
      CHECK(!existing_frame->IsLocal());
      size_t frame_data_index = 0u;
      CHECK(FindFrameDataIndex(frame_data, window->id(), &frame_data_index));
      const web_view::mojom::FrameDataPtr& data = frame_data[frame_data_index];
      existing_frame->SwapToLocal(delegate, window, data->client_properties);
    } else {
      // If we can't find the frame and the change_id of the incoming
      // tree is before the change id we've processed, then we removed the
      // frame and need do nothing.
      if (change_id < frame_tree->change_id_)
        return nullptr;

      // We removed the frame but it hasn't been acked yet.
      if (frame_tree->pending_remove_ids_.count(window->id()))
        return nullptr;

      // We don't know about the frame, but should. Something is wrong.
      DVLOG(1) << "unable to locate frame to attach to";
      return nullptr;
    }
  }

  HTMLFrame* frame = frame_tree->root_->FindFrame(window_id);
  DCHECK(frame);
  frame->Bind(server_frame.Pass(), frame_client_request.Pass());
  return frame;
}

// static
HTMLFrameTreeManager* HTMLFrameTreeManager::FindFrameTreeWithRoot(
    uint32_t root_frame_id) {
  return (!base::CommandLine::ForCurrentProcess()->HasSwitch(
              web_view::switches::kOOPIFAlwaysCreateNewFrameTree) &&
          instances_ && instances_->count(root_frame_id))
             ? (*instances_)[root_frame_id]
             : nullptr;
}

blink::WebView* HTMLFrameTreeManager::GetWebView() {
  return root_->web_view();
}

void HTMLFrameTreeManager::AddObserver(HTMLFrameTreeManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void HTMLFrameTreeManager::RemoveObserver(
    HTMLFrameTreeManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void HTMLFrameTreeManager::OnFrameDestroyed(HTMLFrame* frame) {
  if (!in_process_on_frame_removed_)
    pending_remove_ids_.insert(frame->id());

  if (frame == root_) {
    // If |root_| is removed before |local_frame_|, we don't have a way to
    // select a new local frame when |local_frame_| is removed. On the other
    // hand, it is impossible to remove |root_| after all local frames are gone,
    // because by that time this object has already been deleted.
    CHECK_EQ(root_, local_frame_);
    root_ = nullptr;
  }

  if (frame != local_frame_)
    return;

  // |local_frame_| is being removed. We need to find out whether we have local
  // frames that are not descendants of |local_frame_|, and if yes select a new
  // |local_frame_|.
  local_frame_ = FindNewLocalFrame();
  if (!local_frame_)
    delete this;
}

HTMLFrameTreeManager::HTMLFrameTreeManager(GlobalState* global_state)
    : global_state_(global_state),
      root_(nullptr),
      local_frame_(nullptr),
      change_id_(0u),
      in_process_on_frame_removed_(false),
      weak_factory_(this) {}

HTMLFrameTreeManager::~HTMLFrameTreeManager() {
  DCHECK(!local_frame_);
  RemoveFromInstances();

  FOR_EACH_OBSERVER(HTMLFrameTreeManagerObserver, observers_,
                    OnHTMLFrameTreeManagerDestroyed());
}

void HTMLFrameTreeManager::Init(
    HTMLFrameDelegate* delegate,
    mus::Window* local_window,
    const mojo::Array<web_view::mojom::FrameDataPtr>& frame_data,
    uint32_t change_id) {
  change_id_ = change_id;
  root_ =
      BuildFrameTree(delegate, frame_data, local_window->id(), local_window);
  local_frame_ = root_->FindFrame(local_window->id());
  CHECK(local_frame_);
  local_frame_->UpdateFocus();
}

HTMLFrame* HTMLFrameTreeManager::BuildFrameTree(
    HTMLFrameDelegate* delegate,
    const mojo::Array<web_view::mojom::FrameDataPtr>& frame_data,
    uint32_t local_frame_id,
    mus::Window* local_window) {
  std::vector<HTMLFrame*> parents;
  HTMLFrame* root = nullptr;
  HTMLFrame* last_frame = nullptr;
  for (size_t i = 0; i < frame_data.size(); ++i) {
    if (last_frame && frame_data[i]->parent_id == last_frame->id()) {
      parents.push_back(last_frame);
    } else if (!parents.empty()) {
      while (parents.back()->id() != frame_data[i]->parent_id)
        parents.pop_back();
    }
    HTMLFrame::CreateParams params(this,
                                   !parents.empty() ? parents.back() : nullptr,
                                   frame_data[i]->frame_id, local_window,
                                   frame_data[i]->client_properties, nullptr);
    if (frame_data[i]->frame_id == local_frame_id)
      params.delegate = delegate;

    HTMLFrame* frame = delegate->GetHTMLFactory()->CreateHTMLFrame(&params);
    if (!last_frame)
      root = frame;
    else
      DCHECK(frame->parent());
    last_frame = frame;
  }
  return root;
}

void HTMLFrameTreeManager::RemoveFromInstances() {
  for (auto pair : *instances_) {
    if (pair.second == this) {
      instances_->erase(pair.first);
      return;
    }
  }
}

bool HTMLFrameTreeManager::PrepareForStructureChange(HTMLFrame* source,
                                                     uint32_t change_id) {
  // The change ids may differ if multiple HTMLDocuments are attached to the
  // same tree (which means we have multiple FrameClient for the same tree).
  if (change_id != (change_id_ + 1))
    return false;

  // We only process changes for the topmost local root.
  if (source != local_frame_)
    return false;

  // Update the id as the change is going to be applied (or we can assume it
  // will be applied if we get here).
  change_id_ = change_id;
  return true;
}

void HTMLFrameTreeManager::ProcessOnFrameAdded(
    HTMLFrame* source,
    uint32_t change_id,
    web_view::mojom::FrameDataPtr frame_data) {
  if (!PrepareForStructureChange(source, change_id))
    return;

  ChangeIdAdvancedNotifier notifier(&observers_);

  HTMLFrame* parent = root_->FindFrame(frame_data->parent_id);
  if (!parent) {
    DVLOG(1) << "Received invalid parent in OnFrameAdded "
             << frame_data->parent_id;
    return;
  }
  if (root_->FindFrame(frame_data->frame_id)) {
    DVLOG(1) << "Child with id already exists in OnFrameAdded "
             << frame_data->frame_id;
    return;
  }

  // Because notification is async it's entirely possible for us to create a
  // new frame, and remove it before we get the add from the server. This check
  // ensures we don't add back a frame we explicitly removed.
  if (pending_remove_ids_.count(frame_data->frame_id))
    return;

  DVLOG(2) << "OnFrameAdded this=" << this
           << " frame_id=" << frame_data->frame_id;

  HTMLFrame::CreateParams params(this, parent, frame_data->frame_id, nullptr,
                                 frame_data->client_properties, nullptr);
  // |parent| takes ownership of created HTMLFrame.
  source->GetFirstAncestorWithDelegate()
      ->delegate_->GetHTMLFactory()
      ->CreateHTMLFrame(&params);
}

void HTMLFrameTreeManager::ProcessOnFrameRemoved(HTMLFrame* source,
                                                 uint32_t change_id,
                                                 uint32_t frame_id) {
  if (!PrepareForStructureChange(source, change_id))
    return;

  ChangeIdAdvancedNotifier notifier(&observers_);

  pending_remove_ids_.erase(frame_id);

  HTMLFrame* frame = root_->FindFrame(frame_id);
  if (!frame) {
    DVLOG(1) << "OnFrameRemoved with unknown frame " << frame_id;
    return;
  }

  // We shouldn't see requests to remove the root.
  if (frame == root_) {
    DVLOG(1) << "OnFrameRemoved supplied root; ignoring";
    return;
  }

  // Requests to remove local frames are followed by the Window being destroyed.
  // We handle destruction there.
  if (frame->IsLocal())
    return;

  DVLOG(2) << "OnFrameRemoved this=" << this << " frame_id=" << frame_id;

  DCHECK(!in_process_on_frame_removed_);
  in_process_on_frame_removed_ = true;
  base::WeakPtr<HTMLFrameTreeManager> ref(weak_factory_.GetWeakPtr());
  frame->Close();
  if (!ref)
    return;  // We were deleted.

  in_process_on_frame_removed_ = false;
}

void HTMLFrameTreeManager::ProcessOnFrameClientPropertyChanged(
    HTMLFrame* source,
    uint32_t frame_id,
    const mojo::String& name,
    mojo::Array<uint8_t> new_data) {
  if (source != local_frame_)
    return;

  HTMLFrame* frame = root_->FindFrame(frame_id);
  if (frame)
    frame->SetValueFromClientProperty(name, new_data.Pass());
}

HTMLFrame* HTMLFrameTreeManager::FindNewLocalFrame() {
  HTMLFrame* new_local_frame = nullptr;

  if (root_) {
    std::queue<HTMLFrame*> nodes;
    nodes.push(root_);

    while (!nodes.empty()) {
      HTMLFrame* node = nodes.front();
      nodes.pop();

      if (node == local_frame_)
        continue;

      if (node->IsLocal()) {
        new_local_frame = node;
        break;
      }

      for (const auto& child : node->children())
        nodes.push(child);
    }
  }

  return new_local_frame;
}

}  // namespace mojo
