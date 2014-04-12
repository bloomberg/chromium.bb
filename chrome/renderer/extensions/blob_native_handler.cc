// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/blob_native_handler.h"

#include "base/bind.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebBlob.h"

namespace {

// Expects a single Blob argument. Returns the Blob's UUID.
void GetBlobUuid(const v8::FunctionCallbackInfo<v8::Value>& args) {
  DCHECK_EQ(1, args.Length());
  blink::WebBlob blob = blink::WebBlob::fromV8Value(args[0]);
  args.GetReturnValue().Set(
      v8::String::NewFromUtf8(args.GetIsolate(), blob.uuid().utf8().data()));
}

}  // namespace

namespace extensions {

BlobNativeHandler::BlobNativeHandler(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {
  RouteFunction("GetBlobUuid", base::Bind(&GetBlobUuid));
}

}  // namespace extensions
