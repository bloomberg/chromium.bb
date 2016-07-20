// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/tracing/aw_tracing_delegate.h"

#include "base/values.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/trace_uploader.h"

namespace android_webview {

AwTracingDelegate::AwTracingDelegate() {}
AwTracingDelegate::~AwTracingDelegate() {}

std::unique_ptr<content::TraceUploader> AwTracingDelegate::GetTraceUploader(
    net::URLRequestContextGetter* request_context) {
  NOTREACHED();
  return NULL;
}

void AwTracingDelegate::GenerateMetadataDict(
    base::DictionaryValue* metadata_dict) {
  DCHECK(metadata_dict);
  metadata_dict->SetString("revision", version_info::GetLastChange());
}

}  // namespace android_webview
