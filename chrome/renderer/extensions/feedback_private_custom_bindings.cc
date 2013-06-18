// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/feedback_private_custom_bindings.h"

#include "base/bind.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebBlob.h"

namespace {

void GetBlobUrl(const v8::FunctionCallbackInfo<v8::Value> &args) {
  DCHECK(args.Length() == 1);
  WebKit::WebBlob blob = WebKit::WebBlob::fromV8Value(args[0]);
  args.GetReturnValue().Set(v8::String::New(blob.url().spec().data()));
}

}  // namespace

namespace extensions {

FeedbackPrivateCustomBindings::FeedbackPrivateCustomBindings(
    Dispatcher* dispatcher,
    ChromeV8Context* context) : ChromeV8Extension(dispatcher, context) {
  RouteFunction("GetBlobUrl", base::Bind(&GetBlobUrl));
}

}  // namespace extensions
