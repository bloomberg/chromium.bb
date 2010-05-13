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

BindContextInfo::~BindContextInfo() {
}

HRESULT BindContextInfo::Initialize(IBindCtx* bind_ctx) {
  DCHECK(bind_ctx);
  DCHECK(!ftm_);
  HRESULT hr = CoCreateFreeThreadedMarshaler(GetUnknown(), ftm_.Receive());
  DCHECK(ftm_);
  if (SUCCEEDED(hr)) {
    hr = bind_ctx->RegisterObjectParam(kBindContextInfo, GetUnknown());
  }

  DCHECK(SUCCEEDED(hr)) << "Failed to initialize BindContextInfo";
  return hr;
}

BindContextInfo* BindContextInfo::FromBindContext(IBindCtx* bind_context) {
  if (!bind_context) {
    NOTREACHED();
    return NULL;
  }

  ScopedComPtr<IUnknown> context;
  bind_context->GetObjectParam(kBindContextInfo, context.Receive());
  if (context) {
    ScopedComPtr<IBindContextInfoInternal> internal;
    HRESULT hr = internal.QueryFrom(context);
    DCHECK(SUCCEEDED(hr));
    if (SUCCEEDED(hr)) {
      BindContextInfo* ret = NULL;
      internal->GetCppObject(reinterpret_cast<void**>(&ret));
      DCHECK(ret);
      DLOG_IF(ERROR, reinterpret_cast<void*>(ret) !=
                     reinterpret_cast<void*>(internal.get()))
          << "marshalling took place!";
      return ret;
    }
  }

  CComObject<BindContextInfo>* bind_context_info = NULL;
  HRESULT hr = CComObject<BindContextInfo>::CreateInstance(&bind_context_info);
  DCHECK(bind_context_info != NULL);
  if (bind_context_info) {
    bind_context_info->AddRef();
    hr = bind_context_info->Initialize(bind_context);
    bind_context_info->Release();
    if (FAILED(hr))
      bind_context_info = NULL;
  }

  return bind_context_info;
}

void BindContextInfo::SetToSwitch(IStream* cache) {
  is_switching_ = true;
  if  (!no_cache_) {
    cache_ = cache;
    RewindStream(cache_);
  }
}

