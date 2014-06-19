// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/application/application.h"
#include "mojo/services/navigation/navigation.mojom.h"
#include "mojo/services/public/cpp/view_manager/node.h"
#include "mojo/services/public/cpp/view_manager/types.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"

namespace mojo {
namespace examples {

class HTMLViewer;

class NavigatorImpl : public InterfaceImpl<navigation::Navigator> {
 public:
  explicit NavigatorImpl(HTMLViewer* viewer) : viewer_(viewer) {}
  virtual ~NavigatorImpl() {}

 private:
  // Overridden from navigation::Navigator:
  virtual void Navigate(
      uint32_t node_id,
      navigation::NavigationDetailsPtr navigation_details,
      navigation::ResponseDetailsPtr response_details) OVERRIDE {
    printf("In HTMLViewer, rendering url: %s\n",
           response_details->response->url.data());
    printf("HTML: \n");
    for (;;) {
      char buf[512];
      uint32_t num_bytes = sizeof(buf);
      MojoResult result = ReadDataRaw(
          response_details->response_body_stream.get(),
          buf,
          &num_bytes,
          MOJO_READ_DATA_FLAG_NONE);
      if (result == MOJO_RESULT_SHOULD_WAIT) {
        Wait(response_details->response_body_stream.get(),
             MOJO_HANDLE_SIGNAL_READABLE,
             MOJO_DEADLINE_INDEFINITE);
      } else if (result == MOJO_RESULT_OK) {
        fwrite(buf, num_bytes, 1, stdout);
      } else {
        break;
      }
    }
    printf("\n>>>> EOF <<<<\n\n");

    UpdateView();
  }

  void UpdateView();

  HTMLViewer* viewer_;

  DISALLOW_COPY_AND_ASSIGN(NavigatorImpl);
};

class HTMLViewer : public Application,
                   public view_manager::ViewManagerDelegate  {
 public:
  HTMLViewer() : content_view_(NULL) {}
  virtual ~HTMLViewer() {}

 private:
  friend class NavigatorImpl;

  // Overridden from Application:
  virtual void Initialize() OVERRIDE {
    AddService<NavigatorImpl>(this);
    view_manager::ViewManager::Create(this, this);
  }

  // Overridden from view_manager::ViewManagerDelegate:
  virtual void OnRootAdded(view_manager::ViewManager* view_manager,
                           view_manager::Node* root) OVERRIDE {
    content_view_ = view_manager::View::Create(view_manager);
    root->SetActiveView(content_view_);
    content_view_->SetColor(SK_ColorRED);
  }

  view_manager::View* content_view_;

  DISALLOW_COPY_AND_ASSIGN(HTMLViewer);
};

void NavigatorImpl::UpdateView() {
  viewer_->content_view_->SetColor(SK_ColorGREEN);
}

}

// static
Application* Application::Create() {
  return new examples::HTMLViewer;
}

}
