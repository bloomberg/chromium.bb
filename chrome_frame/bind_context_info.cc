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

HRESULT BindContextInfo::FromBindContext(IBindCtx* bind_context,
                                         BindContextInfo** info) {
  DCHECK(info);
  if (!bind_context) {
    NOTREACHED();
    return E_POINTER;
  }

  ScopedComPtr<IUnknown> context;
  HRESULT hr = bind_context->GetObjectParam(kBindContextInfo,
                                            context.Receive());
  if (context) {
    ScopedComPtr<IBindContextInfoInternal> internal;
    hr = internal.QueryFrom(context);
    DCHECK(SUCCEEDED(hr));
    if (SUCCEEDED(hr)) {
      hr = internal->GetCppObject(reinterpret_cast<void**>(info));
      DCHECK_EQ(hr, S_OK);
      DLOG_IF(ERROR, *info != static_cast<BindContextInfo*>(internal.get()))
          << "marshalling took place!";
    }
  } else {
    DCHECK(FAILED(hr));
    CComObject<BindContextInfo>* bind_context_info = NULL;
    hr = CComObject<BindContextInfo>::CreateInstance(&bind_context_info);
    DCHECK(bind_context_info != NULL);
    if (bind_context_info) {
      bind_context_info->AddRef();
      hr = bind_context_info->Initialize(bind_context);
      if (FAILED(hr)) {
        bind_context_info->Release();
      } else {
        *info = bind_context_info;
      }
    }
  }

  return hr;
}

void BindContextInfo::SetToSwitch(IStream* cache) {
  is_switching_ = true;
  if  (!no_cache_) {
    cache_ = cache;
    RewindStream(cache_);
  }
}

std::wstring BindContextInfo::GetUrl() {
  if (has_prot_data()) {
    return prot_data_->url();
  }
  return std::wstring();
}

