// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/chrome_v8_extension.h"

#include "base/logging.h"
#include "base/lazy_instance.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/extensions/api/extension_api.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "chrome/renderer/extensions/extension_dispatcher.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/resource/resource_bundle.h"

using extensions::ExtensionAPI;
using WebKit::WebDocument;
using WebKit::WebFrame;
using WebKit::WebView;

namespace {

static base::LazyInstance<ChromeV8Extension::InstanceSet> g_instances =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace


// static
base::StringPiece ChromeV8Extension::GetStringResource(int resource_id) {
  return ResourceBundle::GetSharedInstance().GetRawDataResource(resource_id);
}

// static
content::RenderView* ChromeV8Extension::GetCurrentRenderView() {
  WebFrame* webframe = WebFrame::frameForCurrentContext();
  DCHECK(webframe) << "RetrieveCurrentFrame called when not in a V8 context.";
  if (!webframe)
    return NULL;

  WebView* webview = webframe->view();
  if (!webview)
    return NULL;  // can happen during closing

  content::RenderView* renderview = content::RenderView::FromWebView(webview);
  DCHECK(renderview) << "Encountered a WebView without a WebViewDelegate";
  return renderview;
}

ChromeV8Extension::ChromeV8Extension(const char* name, int resource_id,
                                     ExtensionDispatcher* extension_dispatcher)
    : v8::Extension(name,
                    GetStringResource(resource_id).data(),
                    0,     // num dependencies
                    NULL,  // dependencies array
                    GetStringResource(resource_id).size()),  // source length
      extension_dispatcher_(extension_dispatcher) {
  g_instances.Get().insert(this);
}

ChromeV8Extension::ChromeV8Extension(const char* name, int resource_id,
                                     int dependency_count,
                                     const char** dependencies,
                                     ExtensionDispatcher* extension_dispatcher)
    : v8::Extension(name,
                    GetStringResource(resource_id).data(),
                    dependency_count,
                    dependencies,
                    GetStringResource(resource_id).size()),
      extension_dispatcher_(extension_dispatcher) {
  g_instances.Get().insert(this);
}

ChromeV8Extension::~ChromeV8Extension() {
  g_instances.Get().erase(this);
}

// static
const ChromeV8Extension::InstanceSet& ChromeV8Extension::GetAll() {
  return g_instances.Get();
}

void ChromeV8Extension::ContextWillBeReleased(ChromeV8Context* context) {
  handlers_.erase(context);
}

ChromeV8ExtensionHandler* ChromeV8Extension::GetHandler(
    ChromeV8Context* context) const {
  HandlerMap::const_iterator iter = handlers_.find(context);
  if (iter == handlers_.end())
    return NULL;
  else
    return iter->second.get();
}

const Extension* ChromeV8Extension::GetExtensionForCurrentRenderView() const {
  content::RenderView* renderview = GetCurrentRenderView();
  if (!renderview)
    return NULL;  // this can happen as a tab is closing.

  WebDocument document = renderview->GetWebView()->mainFrame()->document();
  GURL url = document.url();
  const ExtensionSet* extensions = extension_dispatcher_->extensions();
  if (!extensions->ExtensionBindingsAllowed(
      ExtensionURLInfo(document.securityOrigin(), url)))
    return NULL;

  return extensions->GetByURL(
      ExtensionURLInfo(document.securityOrigin(), url));
}

bool ChromeV8Extension::CheckCurrentContextAccessToExtensionAPI(
    const std::string& function_name) const {
  ChromeV8Context* context =
      extension_dispatcher_->v8_context_set().GetCurrent();
  if (!context) {
    DLOG(ERROR) << "Not in a v8::Context";
    return false;
  }

  const ::Extension* extension = NULL;
  if (!context->extension_id().empty()) {
    extension =
        extension_dispatcher_->extensions()->GetByID(context->extension_id());
  }

  if (!extension || !extension->HasAPIPermission(function_name)) {
    static const char kMessage[] =
        "You do not have permission to use '%s'. Be sure to declare"
        " in your manifest what permissions you need.";
    std::string error_msg = base::StringPrintf(kMessage, function_name.c_str());
    v8::ThrowException(
        v8::Exception::Error(v8::String::New(error_msg.c_str())));
    return false;
  }

  if (!extension_dispatcher_->IsExtensionActive(extension->id()) &&
      ExtensionAPI::GetInstance()->IsPrivileged(function_name)) {
    static const char kMessage[] =
        "%s can only be used in an extension process.";
    std::string error_msg = base::StringPrintf(kMessage, function_name.c_str());
    v8::ThrowException(
        v8::Exception::Error(v8::String::New(error_msg.c_str())));
    return false;
  }

  return true;
}

v8::Handle<v8::FunctionTemplate>
    ChromeV8Extension::GetNativeFunction(v8::Handle<v8::String> name) {
  if (name->Equals(v8::String::New("GetChromeHidden"))) {
    return v8::FunctionTemplate::New(GetChromeHidden);
  }

  if (name->Equals(v8::String::New("Print"))) {
    return v8::FunctionTemplate::New(Print);
  }

  return v8::FunctionTemplate::New(HandleNativeFunction,
                                   v8::External::New(this));
}

// static
v8::Handle<v8::Value> ChromeV8Extension::HandleNativeFunction(
    const v8::Arguments& arguments) {
  ChromeV8Extension* self = GetFromArguments<ChromeV8Extension>(arguments);
  ChromeV8Context* context =
      self->extension_dispatcher()->v8_context_set().GetCurrent();
  CHECK(context) << "Unknown V8 context. Maybe a native function is getting "
                 << "called during parse of a v8 extension, before the context "
                 << "has been registered.";

  ChromeV8ExtensionHandler* handler = self->GetHandler(context);
  if (!handler) {
    handler = self->CreateHandler(context);
    if (handler)
      self->handlers_[context] = linked_ptr<ChromeV8ExtensionHandler>(handler);
  }
  CHECK(handler) << "No handler for v8 extension: " << self->name();

  std::string name = *v8::String::AsciiValue(arguments.Callee()->GetName());
  return handler->HandleNativeFunction(name, arguments);
}

ChromeV8ExtensionHandler* ChromeV8Extension::CreateHandler(
    ChromeV8Context* context) {
  return NULL;
}

v8::Handle<v8::Value> ChromeV8Extension::GetChromeHidden(
    const v8::Arguments& args) {
  return ChromeV8Context::GetOrCreateChromeHidden(v8::Context::GetCurrent());
}

v8::Handle<v8::Value> ChromeV8Extension::Print(const v8::Arguments& args) {
  if (args.Length() < 1)
    return v8::Undefined();

  std::vector<std::string> components;
  for (int i = 0; i < args.Length(); ++i)
    components.push_back(*v8::String::Utf8Value(args[i]->ToString()));

  LOG(ERROR) << JoinString(components, ',');
  return v8::Undefined();
}
