// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_VIEW_FRAME_H_
#define COMPONENTS_WEB_VIEW_FRAME_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "components/mus/public/cpp/types.h"
#include "components/mus/public/cpp/view_observer.h"
#include "components/web_view/public/interfaces/frame_tree.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"

namespace web_view {

class FrameTest;
class FrameTree;
class FrameTreeClient;
class FrameUserData;

enum class ViewOwnership {
  OWNS_VIEW,
  DOESNT_OWN_VIEW,
};

// Frame represents an embedding in a frame. Frames own their children.
// Frames automatically delete themself if the View the frame is associated
// with is deleted.
//
// In general each Frame has a View. When a new Frame is created by a client
// there may be a small amount of time where the View is not yet known
// (separate pipes are used for the view and frame, resulting in undefined
// message ordering). In this case the view is null and will be set once we
// see the view (OnTreeChanged()).
//
// Each frame has an identifier of the app providing the FrameTreeClient
// (|app_id|). This id is used when servicing a request to navigate the frame.
// When navigating, if the id of the new app matches that of the existing app,
// then it is expected that the new FrameTreeClient will take over rendering to
// the existing view. Because of this a new ViewTreeClient is not obtained and
// Embed() is not invoked on the View. The FrameTreeClient can detect this case
// by the argument |reuse_existing_view| supplied to OnConnect(). Typically the
// id is that of content handler id, but this is left up to the
// FrameTreeDelegate to decide.
class Frame : public mus::ViewObserver, public FrameTreeServer {
 public:
  using ClientPropertyMap = std::map<std::string, std::vector<uint8_t>>;

  Frame(FrameTree* tree,
        mus::View* view,
        uint32_t frame_id,
        uint32_t app_id,
        ViewOwnership view_ownership,
        FrameTreeClient* frame_tree_client,
        scoped_ptr<FrameUserData> user_data,
        const ClientPropertyMap& client_properties);
  ~Frame() override;

  void Init(Frame* parent,
            mojo::ViewTreeClientPtr view_tree_client,
            mojo::InterfaceRequest<FrameTreeServer> server_request);

  // Walks the View tree starting at |view| going up returning the first
  // Frame that is associated with |view|. For example, if |view|
  // has a Frame associated with it, then that is returned. Otherwise
  // this checks view->parent() and so on.
  static Frame* FindFirstFrameAncestor(mus::View* view);

  FrameTree* tree() { return tree_; }

  Frame* parent() { return parent_; }
  const Frame* parent() const { return parent_; }

  mus::View* view() { return view_; }
  const mus::View* view() const { return view_; }

  uint32_t id() const { return id_; }

  uint32_t app_id() const { return app_id_; }

  const ClientPropertyMap& client_properties() const {
    return client_properties_;
  }

  // Finds the descendant with the specified id.
  Frame* FindFrame(uint32_t id) {
    return const_cast<Frame*>(const_cast<const Frame*>(this)->FindFrame(id));
  }
  const Frame* FindFrame(uint32_t id) const;

  bool HasAncestor(const Frame* frame) const;

  FrameUserData* user_data() { return user_data_.get(); }

  const std::vector<Frame*>& children() const { return children_; }

  // Returns true if this Frame or any child Frame is loading.
  bool IsLoading() const;

  // Returns the sum total of loading progress from this Frame and all of its
  // children, as well as the number of Frames accumulated.
  double GatherProgress(int* frame_count) const;

 private:
  friend class FrameTest;
  friend class FrameTree;

  // Identifies whether the FrameTreeClient is from the same app or a different
  // app.
  enum class ClientType {
    // The client is either the root frame, or navigating an existing frame
    // to a different app.
    EXISTING_FRAME_NEW_APP,

    // The client is the result of navigating an existing frame to a new app.
    EXISTING_FRAME_SAME_APP,

    // The client is the result of a new frame (not the root).
    NEW_CHILD_FRAME
  };

  struct FrameTreeServerBinding;

  // Initializes the client by sending it the state of the tree.
  // |frame_tree_server_binding| contains the current FrameTreeServerBinding
  // (if any) and is destroyed after the connection responds to OnConnect().
  //
  // If |client_type| is SAME_APP we can't destroy the existing client
  // (and related data) until we get back the ack from OnConnect(). This way
  // we know the client has completed the switch. If we did not do this it
  // would be possible for the app to see it's existing FrameTreeServer
  // connection lost (and assume the frame is being torn down) before the
  // OnConnect().
  void InitClient(ClientType client_type,
                  scoped_ptr<FrameTreeServerBinding> frame_tree_server_binding,
                  mojo::ViewTreeClientPtr view_tree_client,
                  mojo::InterfaceRequest<FrameTreeServer> server_request);

  // Callback from OnConnect(). This does nothing (other than destroying
  // |frame_tree_server_binding|). See InitClient() for details as to why
  // destruction of |frame_tree_server_binding| happens after OnConnect().
  static void OnConnectAck(
      scoped_ptr<FrameTreeServerBinding> frame_tree_server_binding);

  // Callback from OnEmbed().
  void OnEmbedAck(bool success, mus::ConnectionSpecificId connection_id);

  // Completes a navigation request; swapping the existing FrameTreeClient to
  // the supplied arguments.
  void ChangeClient(FrameTreeClient* frame_tree_client,
                    scoped_ptr<FrameUserData> user_data,
                    mojo::ViewTreeClientPtr view_tree_client,
                    uint32 app_id);

  void SetView(mus::View* view);

  // Adds this to |frames| and recurses through the children calling the
  // same function.
  void BuildFrameTree(std::vector<const Frame*>* frames) const;

  void Add(Frame* node);
  void Remove(Frame* node);

  // Starts a new navigation to |request|. The navigation proceeds as long
  // as there is a View and once OnWillNavigate() has returned. If there is
  // no View the navigation waits until the View is available.
  void StartNavigate(mojo::URLRequestPtr request);
  void OnCanNavigateFrame(uint32_t app_id,
                          FrameTreeClient* frame_tree_client,
                          scoped_ptr<FrameUserData> user_data,
                          mojo::ViewTreeClientPtr view_tree_client);

  // Notifies the client and all descendants as appropriate.
  void NotifyAdded(const Frame* source,
                   const Frame* added_node,
                   uint32_t change_id);
  void NotifyRemoved(const Frame* source,
                     const Frame* removed_node,
                     uint32_t change_id);
  void NotifyClientPropertyChanged(const Frame* source,
                                   const mojo::String& name,
                                   const mojo::Array<uint8_t>& value);
  void NotifyFrameLoadingStateChanged(const Frame* frame, bool loading);
  void NotifyDispatchFrameLoadEvent(const Frame* frame);

  // mus::ViewObserver:
  void OnTreeChanged(const TreeChangeParams& params) override;
  void OnViewDestroying(mus::View* view) override;
  void OnViewEmbeddedAppDisconnected(mus::View* view) override;

  // FrameTreeServer:
  void PostMessageEventToFrame(uint32_t target_frame_id,
                               HTMLMessageEventPtr event) override;
  void LoadingStateChanged(bool loading, double progress) override;
  void TitleChanged(const mojo::String& title) override;
  void SetClientProperty(const mojo::String& name,
                         mojo::Array<uint8_t> value) override;
  void OnCreatedFrame(
      mojo::InterfaceRequest<FrameTreeServer> server_request,
      FrameTreeClientPtr client,
      uint32_t frame_id,
      mojo::Map<mojo::String, mojo::Array<uint8_t>> client_properties) override;
  void RequestNavigate(NavigationTargetType target_type,
                       uint32_t target_frame_id,
                       mojo::URLRequestPtr request) override;
  void DidNavigateLocally(const mojo::String& url) override;
  void DispatchLoadEventToParent() override;

  FrameTree* const tree_;
  // WARNING: this may be null. See class description for details.
  mus::View* view_;
  // The connection id returned from ViewManager::Embed(). Frames created by
  // way of OnCreatedFrame() inherit the id from the parent.
  mus::ConnectionSpecificId embedded_connection_id_;
  // ID for the frame, which is the same as that of the view.
  const uint32_t id_;
  // ID of the app providing the FrameTreeClient and ViewTreeClient.
  uint32_t app_id_;
  Frame* parent_;
  ViewOwnership view_ownership_;
  std::vector<Frame*> children_;
  scoped_ptr<FrameUserData> user_data_;

  FrameTreeClient* frame_tree_client_;

  bool loading_;
  double progress_;

  ClientPropertyMap client_properties_;

  // StartNavigate() stores the request here if the view isn't available at
  // the time of StartNavigate().
  mojo::URLRequestPtr pending_navigate_;

  scoped_ptr<mojo::Binding<FrameTreeServer>> frame_tree_server_binding_;

  base::WeakPtrFactory<Frame> embed_weak_ptr_factory_;

  base::WeakPtrFactory<Frame> navigate_weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Frame);
};

}  // namespace web_view

#endif  // COMPONENTS_WEB_VIEW_FRAME_H_
