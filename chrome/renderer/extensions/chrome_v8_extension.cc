// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "chrome/renderer/extensions/dispatcher.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/resource/resource_bundle.h"

using WebKit::WebDocument;
using WebKit::WebFrame;
using WebKit::WebView;

namespace extensions {

namespace {

static base::LazyInstance<ChromeV8Extension::InstanceSet> g_instances =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

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

ChromeV8Extension::ChromeV8Extension(Dispatcher* dispatcher)
    // TODO(svenpanne) It would be nice to remove the GetCurrent() call and use
    // an additional constructor parameter instead, but this would involve too
    // many changes for now.
    : NativeHandler(v8::Isolate::GetCurrent()),
      dispatcher_(dispatcher) {
  g_instances.Get().insert(this);
}

ChromeV8Extension::~ChromeV8Extension() {
  g_instances.Get().erase(this);
}

// static
const ChromeV8Extension::InstanceSet& ChromeV8Extension::GetAll() {
  return g_instances.Get();
}

const Extension* ChromeV8Extension::GetExtensionForCurrentRenderView() const {
  content::RenderView* renderview = GetCurrentRenderView();
  if (!renderview)
    return NULL;  // this can happen as a tab is closing.

  WebDocument document = renderview->GetWebView()->mainFrame()->document();
  GURL url = document.url();
  const ExtensionSet* extensions = dispatcher_->extensions();
  if (!extensions->ExtensionBindingsAllowed(
      ExtensionURLInfo(document.securityOrigin(), url)))
    return NULL;

  return extensions->GetExtensionOrAppByURL(
      ExtensionURLInfo(document.securityOrigin(), url));
}

}  // namespace extensions
