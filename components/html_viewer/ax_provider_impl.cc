// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/ax_provider_impl.h"

#include "components/html_viewer/blink_basic_type_converters.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebAXObject.h"
#include "third_party/WebKit/public/web/WebSettings.h"
#include "third_party/WebKit/public/web/WebView.h"

using blink::WebAXObject;
using blink::WebURL;
using blink::WebView;

using mojo::Array;
using mojo::AxNodePtr;
using mojo::String;

namespace html_viewer {

AxProviderImpl::AxProviderImpl(
    WebView* web_view,
    mojo::InterfaceRequest<mojo::AxProvider> request)
    : web_view_(web_view),
      binding_(this, request.Pass()) {
}

AxProviderImpl::~AxProviderImpl() {
}

void AxProviderImpl::GetTree(
    const mojo::Callback<void(Array<AxNodePtr> nodes)>& callback) {
  web_view_->settings()->setAccessibilityEnabled(true);
  web_view_->settings()->setInlineTextBoxAccessibilityEnabled(true);

  Array<AxNodePtr> result;
  Populate(web_view_->accessibilityObject(), 0, 0, &result);
  callback.Run(result.Pass());
}

int AxProviderImpl::Populate(const WebAXObject& ax_object,
                             int parent_id,
                             int next_sibling_id,
                             Array<AxNodePtr>* result) {
  AxNodePtr ax_node(ConvertAxNode(ax_object, parent_id, next_sibling_id));
  int ax_node_id = ax_node->id;
  if (ax_node.is_null())
    return 0;

  result->push_back(ax_node.Pass());

  unsigned num_children = ax_object.childCount();
  next_sibling_id = 0;
  for (unsigned i = 0; i < num_children; ++i) {
    int new_id = Populate(ax_object.childAt(num_children - i - 1),
                          ax_node_id,
                          next_sibling_id,
                          result);
    if (new_id != 0)
      next_sibling_id = new_id;
  }

  return ax_node_id;
}

AxNodePtr AxProviderImpl::ConvertAxNode(const WebAXObject& ax_object,
                                        int parent_id,
                                        int next_sibling_id) {
  AxNodePtr result;
  if (!const_cast<WebAXObject&>(ax_object).updateLayoutAndCheckValidity())
    return result.Pass();

  result = mojo::AxNode::New();
  result->id = static_cast<int>(ax_object.axID());
  result->parent_id = parent_id;
  result->next_sibling_id = next_sibling_id;
  result->bounds = mojo::Rect::From(ax_object.boundingBoxRect());

  if (ax_object.isAnchor()) {
    result->link = mojo::AxLink::New();
    result->link->url = String::From(ax_object.url().string());
  } else if (ax_object.childCount() == 0 &&
             !ax_object.stringValue().isEmpty()) {
    result->text = mojo::AxText::New();
    result->text->content = String::From(ax_object.stringValue());
  }

  return result.Pass();
}

}  // namespace html_viewer
