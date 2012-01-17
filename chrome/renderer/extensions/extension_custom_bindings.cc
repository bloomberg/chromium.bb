// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/extension_custom_bindings.h"

#include <string>

#include "base/string_util.h"
#include "chrome/common/chrome_view_type.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/extensions/extension_dispatcher.h"
#include "chrome/renderer/extensions/extension_helper.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_view_visitor.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "v8/include/v8.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebFrame;
using WebKit::WebView;

namespace extensions {

namespace {

// A RenderViewVisitor class that iterates through the set of available
// views, looking for a view of the given type, in the given browser window
// and within the given extension.
// Used to accumulate the list of views associated with an extension.
class ExtensionViewAccumulator : public content::RenderViewVisitor {
 public:
  ExtensionViewAccumulator(const std::string& extension_id,
                           int browser_window_id,
                           content::ViewType view_type)
      : extension_id_(extension_id),
        browser_window_id_(browser_window_id),
        view_type_(view_type),
        views_(v8::Array::New()),
        index_(0) {
  }

  v8::Local<v8::Array> views() { return views_; }

  virtual bool Visit(content::RenderView* render_view) {
    ExtensionHelper* helper = ExtensionHelper::Get(render_view);
    if (!ViewTypeMatches(helper->view_type(), view_type_))
      return true;

    GURL url = render_view->GetWebView()->mainFrame()->document().url();
    if (!url.SchemeIs(chrome::kExtensionScheme))
      return true;
    const std::string& extension_id = url.host();
    if (extension_id != extension_id_)
      return true;

    if (browser_window_id_ != extension_misc::kUnknownWindowId &&
        helper->browser_window_id() != browser_window_id_) {
      return true;
    }

    v8::Local<v8::Context> context =
        render_view->GetWebView()->mainFrame()->mainWorldScriptContext();
    if (!context.IsEmpty()) {
      v8::Local<v8::Value> window = context->Global();
      DCHECK(!window.IsEmpty());

      if (!OnMatchedView(window))
        return false;
    }
    return true;
  }

 private:
  // Called on each view found matching the search criteria.  Returns false
  // to terminate the iteration.
  bool OnMatchedView(v8::Local<v8::Value> view_window) {
    views_->Set(v8::Integer::New(index_), view_window);
    index_++;

    if (view_type_ == chrome::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE)
      return false;  // There can be only one...

    return true;
  }

  // Returns true is |type| "isa" |match|.
  static bool ViewTypeMatches(content::ViewType type, content::ViewType match) {
    if (type == match)
      return true;

    // INVALID means match all.
    if (match == content::VIEW_TYPE_INVALID)
      return true;

    return false;
  }

  std::string extension_id_;
  int browser_window_id_;
  content::ViewType view_type_;
  v8::Local<v8::Array> views_;
  int index_;
};

}  // namespace

ExtensionCustomBindings::ExtensionCustomBindings(
    int dependency_count,
    const char** dependencies,
    ExtensionDispatcher* extension_dispatcher)
    : ChromeV8Extension(
          "extensions/extension_custom_bindings.js",
          IDR_EXTENSION_CUSTOM_BINDINGS_JS,
          dependency_count,
          dependencies,
          extension_dispatcher) {}

// static
v8::Handle<v8::Value> ExtensionCustomBindings::GetExtensionViews(
    const v8::Arguments& args) {
  if (args.Length() != 2)
    return v8::Undefined();

  if (!args[0]->IsInt32() || !args[1]->IsString())
    return v8::Undefined();

  // |browser_window_id| == extension_misc::kUnknownWindowId means getting
  // views attached to any browser window.
  int browser_window_id = args[0]->Int32Value();

  std::string view_type_string = *v8::String::Utf8Value(args[1]->ToString());
  StringToUpperASCII(&view_type_string);
  // |view_type| == content::VIEW_TYPE_INVALID means getting any type of
  // views.
  content::ViewType view_type = content::VIEW_TYPE_INVALID;
  if (view_type_string == chrome::kViewTypeBackgroundPage) {
    view_type = chrome::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE;
  } else if (view_type_string == chrome::kViewTypeInfobar) {
    view_type = chrome::VIEW_TYPE_EXTENSION_INFOBAR;
  } else if (view_type_string == chrome::kViewTypeNotification) {
    view_type = chrome::VIEW_TYPE_NOTIFICATION;
  } else if (view_type_string == chrome::kViewTypeTabContents) {
    view_type = content::VIEW_TYPE_TAB_CONTENTS;
  } else if (view_type_string == chrome::kViewTypePopup) {
    view_type = chrome::VIEW_TYPE_EXTENSION_POPUP;
  } else if (view_type_string == chrome::kViewTypeExtensionDialog) {
    view_type = chrome::VIEW_TYPE_EXTENSION_DIALOG;
  } else if (view_type_string == chrome::kViewTypeAppShell) {
    view_type = chrome::VIEW_TYPE_APP_SHELL;
  } else if (view_type_string != chrome::kViewTypeAll) {
    return v8::Undefined();
  }

  ExtensionCustomBindings* v8_extension =
      GetFromArguments<ExtensionCustomBindings>(args);
  const ::Extension* extension =
      v8_extension->GetExtensionForCurrentRenderView();
  if (!extension)
    return v8::Undefined();

  ExtensionViewAccumulator accumulator(extension->id(), browser_window_id,
                                       view_type);
  content::RenderView::ForEach(&accumulator);
  return accumulator.views();
}

v8::Handle<v8::FunctionTemplate> ExtensionCustomBindings::GetNativeFunction(
    v8::Handle<v8::String> name) {
  if (name->Equals(v8::String::New("GetExtensionViews"))) {
    return v8::FunctionTemplate::New(GetExtensionViews,
                                     v8::External::New(this));
  }

  return ChromeV8Extension::GetNativeFunction(name);
}

}  // namespace extensions
