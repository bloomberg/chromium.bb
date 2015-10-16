// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_HTML_WIDGET_H_
#define COMPONENTS_HTML_VIEWER_HTML_WIDGET_H_

#include "third_party/WebKit/public/web/WebViewClient.h"
#include "third_party/WebKit/public/web/WebWidgetClient.h"

namespace blink {
class WebFrameWidget;
}

namespace mojo {
class ApplicationImpl;
}

namespace mus {
class Window;
}

namespace html_viewer {

class GlobalState;
class ImeController;
class WebLayerTreeViewImpl;

// HTMLWidget is responsible for creation of the WebWidget. Which WebWidget
// and how it is created depends upon the frame the WebWidget is created for.
class HTMLWidget {
 public:
  virtual ~HTMLWidget() {}

  virtual blink::WebWidget* GetWidget() = 0;

  virtual void OnWindowBoundsChanged(mus::Window* window) = 0;
};

// Used for the root frame when the root frame is remote.
class HTMLWidgetRootRemote : public HTMLWidget, public blink::WebViewClient {
 public:
  HTMLWidgetRootRemote();
  ~HTMLWidgetRootRemote() override;

 private:
  // WebViewClient methods:
  virtual blink::WebStorageNamespace* createSessionStorageNamespace();
  virtual bool allowsBrokenNullLayerTreeView() const;

  // HTMLWidget:
  blink::WebWidget* GetWidget() override;
  void OnWindowBoundsChanged(mus::Window* window) override;

  blink::WebView* web_view_;

  DISALLOW_COPY_AND_ASSIGN(HTMLWidgetRootRemote);
};

// Used for the root frame when the frame is local. If there is only one
// frame in the document, this is the HTMLWidget type created.
class HTMLWidgetRootLocal : public HTMLWidget, public blink::WebViewClient {
 public:
  struct CreateParams {
    CreateParams(mojo::ApplicationImpl* app,
                 GlobalState* global_state,
                 mus::Window* window);
    ~CreateParams();

    mojo::ApplicationImpl* app;
    GlobalState* global_state;
    mus::Window* window;
  };

  HTMLWidgetRootLocal(CreateParams* create_params);
  ~HTMLWidgetRootLocal() override;

  blink::WebView* web_view() { return web_view_; }

 protected:
  // WebViewClient methods:
  virtual blink::WebStorageNamespace* createSessionStorageNamespace();
  virtual void initializeLayerTreeView();
  virtual blink::WebLayerTreeView* layerTreeView();
  virtual void didFirstVisuallyNonEmptyLayout();
  virtual void resetInputMethod();
  virtual void didHandleGestureEvent(const blink::WebGestureEvent& event,
                                     bool event_cancelled);
  virtual void didUpdateTextOfFocusedElementByNonUserInput();
  virtual void showImeIfNeeded();

 private:
  // HTMLWidget:
  blink::WebWidget* GetWidget() override;
  void OnWindowBoundsChanged(mus::Window* window) override;

  mojo::ApplicationImpl* app_;
  GlobalState* global_state_;
  mus::Window* window_;
  blink::WebView* web_view_;
  scoped_ptr<WebLayerTreeViewImpl> web_layer_tree_view_impl_;
  scoped_ptr<ImeController> ime_controller_;

  DISALLOW_COPY_AND_ASSIGN(HTMLWidgetRootLocal);
};

// Used for frames other than the root that are local.
class HTMLWidgetLocalRoot : public HTMLWidget, public blink::WebWidgetClient {
 public:
  HTMLWidgetLocalRoot(mojo::ApplicationImpl* app,
                      GlobalState* global_state,
                      mus::Window* window,
                      blink::WebLocalFrame* web_local_frame);
  ~HTMLWidgetLocalRoot() override;

 private:
  // HTMLWidget:
  blink::WebWidget* GetWidget() override;
  void OnWindowBoundsChanged(mus::Window* window) override;

  // WebWidgetClient:
  virtual void initializeLayerTreeView();
  virtual blink::WebLayerTreeView* layerTreeView();
  virtual void resetInputMethod();
  virtual void didHandleGestureEvent(const blink::WebGestureEvent& event,
                                     bool event_cancelled);
  virtual void didUpdateTextOfFocusedElementByNonUserInput();
  virtual void showImeIfNeeded();

  mojo::ApplicationImpl* app_;
  GlobalState* global_state_;
  blink::WebFrameWidget* web_frame_widget_;
  scoped_ptr<WebLayerTreeViewImpl> web_layer_tree_view_impl_;
  scoped_ptr<ImeController> ime_controller_;

  DISALLOW_COPY_AND_ASSIGN(HTMLWidgetLocalRoot);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_HTML_WIDGET_H_
