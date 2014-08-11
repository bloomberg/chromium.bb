// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_HTML_VIEWER_HTML_DOCUMENT_VIEW_H_
#define MOJO_SERVICES_HTML_VIEWER_HTML_DOCUMENT_VIEW_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/application/lazy_interface_ptr.h"
#include "mojo/services/public/cpp/view_manager/node_observer.h"
#include "mojo/services/public/interfaces/navigation/navigation.mojom.h"
#include "mojo/services/public/interfaces/network/url_loader.mojom.h"
#include "third_party/WebKit/public/web/WebFrameClient.h"
#include "third_party/WebKit/public/web/WebViewClient.h"

namespace mojo {

class Node;
class ViewManager;
class View;

// A view for a single HTML document.
class HTMLDocumentView : public blink::WebViewClient,
                         public blink::WebFrameClient,
                         public NodeObserver {
 public:
  HTMLDocumentView(ServiceProvider* service_provider,
                   ViewManager* view_manager);
  virtual ~HTMLDocumentView();

  void AttachToNode(Node* node);

  void Load(URLResponsePtr response);

 private:
  // WebViewClient methods:
  virtual blink::WebStorageNamespace* createSessionStorageNamespace();

  // WebWidgetClient methods:
  virtual void didInvalidateRect(const blink::WebRect& rect);
  virtual bool allowsBrokenNullLayerTreeView() const;

  // WebFrameClient methods:
  virtual blink::WebCookieJar* cookieJar(blink::WebLocalFrame* frame);
  virtual blink::WebNavigationPolicy decidePolicyForNavigation(
      blink::WebLocalFrame* frame, blink::WebDataSource::ExtraData* data,
      const blink::WebURLRequest& request, blink::WebNavigationType nav_type,
      blink::WebNavigationPolicy default_policy, bool isRedirect);
  virtual void didAddMessageToConsole(
      const blink::WebConsoleMessage& message,
      const blink::WebString& source_name,
      unsigned source_line,
      const blink::WebString& stack_trace);
  virtual void didNavigateWithinPage(
      blink::WebLocalFrame* frame,
      const blink::WebHistoryItem& history_item,
      blink::WebHistoryCommitType commit_type);

  // NodeObserver methods:
  virtual void OnNodeBoundsChanged(Node* node,
                                   const gfx::Rect& old_bounds,
                                   const gfx::Rect& new_bounds) OVERRIDE;
  virtual void OnNodeDestroyed(Node* node) OVERRIDE;
  virtual void OnNodeInputEvent(Node* node, const EventPtr& event) OVERRIDE;

  void Repaint();

  ViewManager* view_manager_;
  View* view_;
  blink::WebView* web_view_;
  Node* root_;
  bool repaint_pending_;
  LazyInterfacePtr<NavigatorHost> navigator_host_;

  base::WeakPtrFactory<HTMLDocumentView> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(HTMLDocumentView);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_HTML_VIEWER_HTML_DOCUMENT_VIEW_H_
