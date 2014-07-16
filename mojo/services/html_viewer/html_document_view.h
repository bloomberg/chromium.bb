// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_HTML_VIEWER_HTML_DOCUMENT_VIEW_H_
#define MOJO_SERVICES_HTML_VIEWER_HTML_DOCUMENT_VIEW_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/application/lazy_interface_ptr.h"
#include "mojo/public/interfaces/service_provider/service_provider.mojom.h"
#include "mojo/services/public/cpp/view_manager/node_observer.h"
#include "mojo/services/public/cpp/view_manager/view_observer.h"
#include "mojo/services/public/interfaces/navigation/navigation.mojom.h"
#include "mojo/services/public/interfaces/network/url_loader.mojom.h"
#include "third_party/WebKit/public/web/WebFrameClient.h"
#include "third_party/WebKit/public/web/WebViewClient.h"

namespace mojo {

namespace view_manager {
class Node;
class ViewManager;
class View;
}

// A view for a single HTML document.
class HTMLDocumentView : public blink::WebViewClient,
                         public blink::WebFrameClient,
                         public view_manager::ViewObserver,
                         public view_manager::NodeObserver {
 public:
  HTMLDocumentView(ServiceProvider* service_provider,
                   view_manager::ViewManager* view_manager);
  virtual ~HTMLDocumentView();

  void AttachToNode(view_manager::Node* node);

  void Load(URLResponsePtr response);

 private:
  // WebViewClient methods:
  virtual blink::WebStorageNamespace* createSessionStorageNamespace();

  // WebWidgetClient methods:
  virtual void didInvalidateRect(const blink::WebRect& rect);
  virtual bool allowsBrokenNullLayerTreeView() const;

  // WebFrameClient methods:
  virtual blink::WebNavigationPolicy decidePolicyForNavigation(
      blink::WebLocalFrame* frame, blink::WebDataSource::ExtraData* data,
      const blink::WebURLRequest& request, blink::WebNavigationType nav_type,
      blink::WebNavigationPolicy default_policy, bool isRedirect) OVERRIDE;
  virtual void didAddMessageToConsole(
      const blink::WebConsoleMessage& message,
      const blink::WebString& source_name,
      unsigned source_line,
      const blink::WebString& stack_trace) OVERRIDE;
  virtual void didNavigateWithinPage(
      blink::WebLocalFrame* frame,
      const blink::WebHistoryItem& history_item,
      blink::WebHistoryCommitType commit_type) OVERRIDE;

  // ViewObserver methods:
  virtual void OnViewInputEvent(view_manager::View* view,
                                const EventPtr& event) OVERRIDE;

  // NodeObserver methods:
  virtual void OnNodeBoundsChanged(view_manager::Node* node,
                                   const gfx::Rect& old_bounds,
                                   const gfx::Rect& new_bounds) OVERRIDE;
  virtual void OnNodeDestroyed(view_manager::Node* node) OVERRIDE;

  void Repaint();

  view_manager::ViewManager* view_manager_;
  view_manager::View* view_;
  blink::WebView* web_view_;
  view_manager::Node* root_;
  bool repaint_pending_;
  LazyInterfacePtr<navigation::NavigatorHost> navigator_host_;

  base::WeakPtrFactory<HTMLDocumentView> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(HTMLDocumentView);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_HTML_VIEWER_HTML_DOCUMENT_VIEW_H_
