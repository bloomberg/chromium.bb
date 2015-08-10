// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_DOCUMENT_RESOURCE_WAITER_H_
#define COMPONENTS_HTML_VIEWER_DOCUMENT_RESOURCE_WAITER_H_

#include "base/basictypes.h"
#include "mandoline/tab/public/interfaces/frame_tree.mojom.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"

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
class DocumentResourceWaiter : public mandoline::FrameTreeClient {
 public:
  DocumentResourceWaiter(GlobalState* global_state,
                         mojo::URLResponsePtr response,
                         HTMLDocumentOOPIF* document);
  ~DocumentResourceWaiter() override;

  // Releases all the resources that have been accumulated.
  void Release(mojo::InterfaceRequest<mandoline::FrameTreeClient>*
                   frame_tree_client_request,
               mandoline::FrameTreeServerPtr* frame_tree_server,
               mojo::Array<mandoline::FrameDataPtr>* frame_data,
               uint32_t* change_id);

  mojo::URLResponsePtr ReleaseURLResponse();

  // See class description.
  bool IsReady() const;

  void set_root(mojo::View* root) { root_ = root; }
  mojo::View* root() { return root_; }

  void Bind(mojo::InterfaceRequest<mandoline::FrameTreeClient> request);

 private:
  // mandoline::FrameTreeClient:
  void OnConnect(mandoline::FrameTreeServerPtr server,
                 uint32 change_id,
                 mojo::Array<mandoline::FrameDataPtr> frame_data) override;
  void OnFrameAdded(uint32 change_id,
                    mandoline::FrameDataPtr frame_data) override;
  void OnFrameRemoved(uint32 change_id, uint32_t frame_id) override;
  void OnFrameClientPropertyChanged(uint32_t frame_id,
                                    const mojo::String& name,
                                    mojo::Array<uint8_t> new_value) override;

  GlobalState* global_state_;
  HTMLDocumentOOPIF* document_;
  mojo::URLResponsePtr response_;
  mojo::View* root_;
  mandoline::FrameTreeServerPtr server_;
  mojo::Array<mandoline::FrameDataPtr> frame_data_;
  uint32_t change_id_;

  // Once we get OnConnect() we unbind |frame_tree_client_binding_| and put it
  // here.
  mojo::InterfaceRequest<mandoline::FrameTreeClient> frame_tree_client_request_;
  mojo::Binding<mandoline::FrameTreeClient> frame_tree_client_binding_;

  DISALLOW_COPY_AND_ASSIGN(DocumentResourceWaiter);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_DOCUMENT_RESOURCE_WAITER_H_
