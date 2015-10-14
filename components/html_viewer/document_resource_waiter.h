// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_DOCUMENT_RESOURCE_WAITER_H_
#define COMPONENTS_HTML_VIEWER_DOCUMENT_RESOURCE_WAITER_H_

#include "base/basictypes.h"
#include "components/html_viewer/html_frame_tree_manager_observer.h"
#include "components/mus/public/cpp/view_observer.h"
#include "components/web_view/public/interfaces/frame.mojom.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"

namespace mojo {
class View;
}

namespace html_viewer {

class HTMLDocument;
class HTMLFrameTreeManager;
class GlobalState;

// DocumentResourceWaiter waits for the necessary resources needed to load an
// HTMLDocument. Once ready it calls to HTMLDocument::Load(). Once ready it is
// assumed HTMLDocument will call back for the FrameClient and FrameData.
class DocumentResourceWaiter : public web_view::mojom::FrameClient,
                               public HTMLFrameTreeManagerObserver,
                               public mus::ViewObserver {
 public:
  DocumentResourceWaiter(GlobalState* global_state,
                         mojo::URLResponsePtr response,
                         HTMLDocument* document);
  ~DocumentResourceWaiter() override;

  // Releases all the resources that have been accumulated.
  void Release(mojo::InterfaceRequest<web_view::mojom::FrameClient>*
                   frame_client_request,
               web_view::mojom::FramePtr* frame,
               mojo::Array<web_view::mojom::FrameDataPtr>* frame_data,
               uint32_t* view_id,
               uint32_t* change_id,
               web_view::mojom::ViewConnectType* view_connect_type,
               OnConnectCallback* on_connect_callback);

  uint32_t change_id() const { return change_id_; }

  mojo::URLResponsePtr ReleaseURLResponse();

  // See class description.
  bool is_ready() const { return is_ready_; }

  void SetRoot(mus::View* root);
  mus::View* root() { return root_; }

  void Bind(mojo::InterfaceRequest<web_view::mojom::FrameClient> request);

 private:
  // Updates |is_ready_|, and if ready starts the Load() in the document.
  void UpdateIsReady();

  // web_view::mojom::FrameClient:
  void OnConnect(web_view::mojom::FramePtr frame,
                 uint32_t change_id,
                 uint32_t view_id,
                 web_view::mojom::ViewConnectType view_connect_type,
                 mojo::Array<web_view::mojom::FrameDataPtr> frame_data,
                 const OnConnectCallback& callback) override;
  void OnFrameAdded(uint32_t change_id,
                    web_view::mojom::FrameDataPtr frame_data) override;
  void OnFrameRemoved(uint32_t change_id, uint32_t frame_id) override;
  void OnFrameClientPropertyChanged(uint32_t frame_id,
                                    const mojo::String& name,
                                    mojo::Array<uint8_t> new_value) override;
  void OnPostMessageEvent(uint32_t source_frame_id,
                          uint32_t target_frame_id,
                          web_view::mojom::HTMLMessageEventPtr event) override;
  void OnWillNavigate(const mojo::String& origin,
                      const OnWillNavigateCallback& callback) override;
  void OnFrameLoadingStateChanged(uint32_t frame_id, bool loading) override;
  void OnDispatchFrameLoadEvent(uint32_t frame_id) override;
  void Find(int32_t request_id,
            const mojo::String& search_text,
            web_view::mojom::FindOptionsPtr options,
            bool wrap_within_frame,
            const FindCallback& callback) override;
  void StopFinding(bool clear_selection) override;
  void HighlightFindResults(int32_t request_id,
                            const mojo::String& search_test,
                            web_view::mojom::FindOptionsPtr options,
                            bool reset) override;
  void StopHighlightingFindResults() override;

  // ViewObserver:
  void OnViewViewportMetricsChanged(
      mus::View* view,
      const mojo::ViewportMetrics& old_metrics,
      const mojo::ViewportMetrics& new_metrics) override;
  void OnViewDestroyed(mus::View* view) override;

  // HTMLFrameTreeManagerObserver:
  void OnHTMLFrameTreeManagerChangeIdAdvanced() override;
  void OnHTMLFrameTreeManagerDestroyed() override;

  GlobalState* global_state_;
  HTMLDocument* document_;
  mojo::URLResponsePtr response_;
  mus::View* root_;
  web_view::mojom::FramePtr frame_;
  mojo::Array<web_view::mojom::FrameDataPtr> frame_data_;
  uint32_t change_id_;
  uint32_t view_id_;
  web_view::mojom::ViewConnectType view_connect_type_;
  OnConnectCallback on_connect_callback_;

  // Once we get OnConnect() we unbind |frame_client_binding_| and put it here.
  mojo::InterfaceRequest<web_view::mojom::FrameClient> frame_client_request_;
  mojo::Binding<web_view::mojom::FrameClient> frame_client_binding_;

  bool is_ready_;

  // See comments in UpdateIsReady() for details of this.
  //
  // While |waiting_for_change_id_| is true DocumentResourceWaiter is an
  // HTMLFrameTreeManagerObserver on |target_frame_tree_|.
  bool waiting_for_change_id_;

  HTMLFrameTreeManager* target_frame_tree_;

  DISALLOW_COPY_AND_ASSIGN(DocumentResourceWaiter);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_DOCUMENT_RESOURCE_WAITER_H_
