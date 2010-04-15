// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/bind_context_info.h"
#include "chrome_frame/utils.h"

// This is non const due to API expectations
static wchar_t* kBindContextInfo = L"_CHROMEFRAME_BIND_CONTEXT_INFO_";

// BindContextInfo member definitions.
BindContextInfo::BindContextInfo()
    : no_cache_(false),
      chrome_request_(false),
      is_switching_(false) {
}

BindContextInfo* BindContextInfo::FromBindContext(IBindCtx* bind_context) {
  if (!bind_context) {
    NOTREACHED();
    return NULL;
  }

  ScopedComPtr<IUnknown> context;
  bind_context->GetObjectParam(kBindContextInfo, context.Receive());
  if (context) {
    return static_cast<BindContextInfo*>(context.get());
  }

  CComObject<BindContextInfo>* bind_context_info = NULL;
  CComObject<BindContextInfo>::CreateInstance(&bind_context_info);
  DCHECK(bind_context_info != NULL);

  bind_context->RegisterObjectParam(kBindContextInfo, bind_context_info);
  return bind_context_info;
}

void BindContextInfo::SetToSwitch(IStream* cache) {
  is_switching_ = true;
  if  (!no_cache_) {
    cache_ = cache;
    RewindStream(cache_);
  }
}

