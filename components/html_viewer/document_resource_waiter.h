// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_DOCUMENT_RESOURCE_WAITER_H_
#define COMPONENTS_HTML_VIEWER_DOCUMENT_RESOURCE_WAITER_H_

#include "base/basictypes.h"
#include "components/web_view/public/interfaces/frame_tree.mojom.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"

namespace mojo {
class View;
}

namespace html_viewer {

class HTMLDocumentOOPIF;
class GlobalState;

// DocumentResourceWaiter waits for the necessary resources needed to load an
// HTMLDocument. Use IsReady() to determine when everything is available. Once
// ready it is assumed HTMLDocument will call back for the FrameTreeClient and
// FrameData.
class DocumentResourceWaiter : public web_view::FrameTreeClient {
 public:
  DocumentResourceWaiter(GlobalState* global_state,
                         mojo::URLResponsePtr response,
                         HTMLDocumentOOPIF* document);
  ~DocumentResourceWaiter() override;

  // Releases all the resources that have been accumulated.
  void Release(mojo::InterfaceRequest<web_view::FrameTreeClient>*
                   frame_tree_client_request,
               web_view::FrameTreeServerPtr* frame_tree_server,
               mojo::Array<web_view::FrameDataPtr>* frame_data,
               uint32_t* view_id,
               uint32_t* change_id,
               web_view::ViewConnectType* view_connect_type,
               OnConnectCallback* on_connect_callback);

  mojo::URLResponsePtr ReleaseURLResponse();

  // See class description.
  bool IsReady() const;

  void set_root(mojo::View* root) { root_ = root; }
  mojo::View* root() { return root_; }

  void Bind(mojo::InterfaceRequest<web_view::FrameTreeClient> request);

 private:
  // web_view::FrameTreeClient:
  void OnConnect(web_view::FrameTreeServerPtr server,
                 uint32_t change_id,
                 uint32_t view_id,
                 web_view::ViewConnectType view_connect_type,
                 mojo::Array<web_view::FrameDataPtr> frame_data,
                 const OnConnectCallback& callback) override;
  void OnFrameAdded(uint32_t change_id,
                    web_view::FrameDataPtr frame_data) override;
  void OnFrameRemoved(uint32_t change_id, uint32_t frame_id) override;
  void OnFrameClientPropertyChanged(uint32_t frame_id,
                                    const mojo::String& name,
                                    mojo::Array<uint8_t> new_value) override;
  void OnPostMessageEvent(uint32_t source_frame_id,
                          uint32_t target_frame_id,
                          web_view::HTMLMessageEventPtr event) override;
  void OnWillNavigate(uint32_t target_frame_id) override;

  GlobalState* global_state_;
  HTMLDocumentOOPIF* document_;
  mojo::URLResponsePtr response_;
  mojo::View* root_;
  web_view::FrameTreeServerPtr server_;
  mojo::Array<web_view::FrameDataPtr> frame_data_;
  uint32_t change_id_;
  uint32_t view_id_;
  web_view::ViewConnectType view_connect_type_;
  OnConnectCallback on_connect_callback_;

  // Once we get OnConnect() we unbind |frame_tree_client_binding_| and put it
  // here.
  mojo::InterfaceRequest<web_view::FrameTreeClient> frame_tree_client_request_;
  mojo::Binding<web_view::FrameTreeClient> frame_tree_client_binding_;

  DISALLOW_COPY_AND_ASSIGN(DocumentResourceWaiter);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_DOCUMENT_RESOURCE_WAITER_H_
