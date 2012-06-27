// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/app_window_custom_bindings.h"

#include <string>

#include "base/string_util.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/extensions/extension_dispatcher.h"
#include "chrome/renderer/extensions/extension_helper.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNavigationPolicy.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWindowFeatures.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLRequest.h"
#include "v8/include/v8.h"
#include "webkit/glue/webkit_glue.h"

#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_view_visitor.h"

namespace extensions {

AppWindowCustomBindings::AppWindowCustomBindings(
    ExtensionDispatcher* extension_dispatcher)
    : ChromeV8Extension(extension_dispatcher) {
  RouteStaticFunction("GetView", &GetView);
}

namespace {
class FindViewByID : public content::RenderViewVisitor {
 public:
  explicit FindViewByID(int route_id) : route_id_(route_id), view_(NULL) {
  }

  content::RenderView* view() { return view_; }

  // Returns false to terminate the iteration.
  virtual bool Visit(content::RenderView* render_view) {
    if (render_view->GetRoutingID() == route_id_) {
      view_ = render_view;
      return false;
    }
    return true;
  }

 private:
  int route_id_;
  content::RenderView* view_;
};
}  // namespace

v8::Handle<v8::Value> AppWindowCustomBindings::GetView(
    const v8::Arguments& args) {
  // TODO(jeremya): convert this to IDL nocompile to get validation, and turn
  // these argument checks into CHECK().
  if (args.Length() != 1)
    return v8::Undefined();

  if (!args[0]->IsInt32())
    return v8::Undefined();

  int view_id = args[0]->Int32Value();

  if (view_id == MSG_ROUTING_NONE)
    return v8::Undefined();

  // TODO(jeremya): there exists a direct map of routing_id -> RenderView as
  // ChildThread::current()->ResolveRoute(), but ResolveRoute returns an
  // IPC::Listener*, which RenderView doesn't inherit from (RenderViewImpl
  // does, indirectly, via RenderWidget, but the link isn't exposed outside of
  // content/.)
  FindViewByID view_finder(view_id);
  content::RenderView::ForEach(&view_finder);
  content::RenderView* view = view_finder.view();
  if (!view)
    return v8::Undefined();

  // TODO(jeremya): it doesn't really make sense to set the opener here, but we
  // need to make sure the security origin is set up before returning the DOM
  // reference. A better way to do this would be to have the browser pass the
  // opener through so opener_id is set in RenderViewImpl's constructor.
  content::RenderView* render_view = GetCurrentRenderView();
  if (!render_view)
    return v8::Undefined();
  WebKit::WebFrame* opener = render_view->GetWebView()->mainFrame();
  WebKit::WebFrame* frame = view->GetWebView()->mainFrame();
  frame->setOpener(opener);
  content::RenderThread::Get()->Send(
      new ExtensionHostMsg_ResumeRequests(view->GetRoutingID()));

  v8::Local<v8::Value> window = frame->mainWorldScriptContext()->Global();
  return window;
}

}  // namespace extensions
