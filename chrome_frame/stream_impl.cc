// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/stream_impl.h"

#include "base/logging.h"

void StreamImpl::Initialize(IStream* delegate) {
  delegate_ = delegate;
}

STDMETHODIMP StreamImpl::Write(const void * buffer, ULONG size,
                               ULONG* size_written) {
  HRESULT hr = E_NOTIMPL;
  if (delegate_)
    hr = delegate_->Write(buffer, size, size_written);
  return hr;
}

STDMETHODIMP StreamImpl::Read(void* pv, ULONG cb, ULONG* read) {
  HRESULT hr = E_NOTIMPL;
  if (delegate_)
    hr = delegate_->Read(pv, cb, read);
  return hr;
}

STDMETHODIMP StreamImpl::Seek(LARGE_INTEGER move, DWORD origin,
                              ULARGE_INTEGER* new_pos) {
  HRESULT hr = E_NOTIMPL;
  if (delegate_)
    hr = delegate_->Seek(move, origin, new_pos);
  return hr;
}

STDMETHODIMP StreamImpl::SetSize(ULARGE_INTEGER new_size) {
  HRESULT hr = E_NOTIMPL;
  if (delegate_)
    hr = delegate_->SetSize(new_size);
  return hr;
}

STDMETHODIMP StreamImpl::CopyTo(IStream* stream, ULARGE_INTEGER cb,
                                ULARGE_INTEGER* read,
                                ULARGE_INTEGER* written) {
  HRESULT hr = E_NOTIMPL;
  if (delegate_)
    hr = delegate_->CopyTo(stream, cb, read, written);
  return hr;
}

STDMETHODIMP StreamImpl::Commit(DWORD flags) {
  HRESULT hr = E_NOTIMPL;
  if (delegate_)
    hr = delegate_->Commit(flags);
  return hr;
}

STDMETHODIMP StreamImpl::Revert() {
  HRESULT hr = E_NOTIMPL;
  if (delegate_)
    hr = delegate_->Revert();
  return hr;
}

STDMETHODIMP StreamImpl::LockRegion(ULARGE_INTEGER offset, ULARGE_INTEGER cb,
                                    DWORD type) {
  HRESULT hr = E_NOTIMPL;
  if (delegate_)
    hr = delegate_->LockRegion(offset, cb, type);
  return hr;
}

STDMETHODIMP StreamImpl::UnlockRegion(ULARGE_INTEGER offset, ULARGE_INTEGER cb,
                                      DWORD type) {
  HRESULT hr = E_NOTIMPL;
  if (delegate_)
    hr = delegate_->UnlockRegion(offset, cb, type);
  return hr;
}

STDMETHODIMP StreamImpl::Stat(STATSTG* statstg, DWORD flag) {
  HRESULT hr = E_NOTIMPL;
  if (delegate_)
    hr = delegate_->Stat(statstg, flag);
  return hr;
}

STDMETHODIMP StreamImpl::Clone(IStream** stream) {
  HRESULT hr = E_NOTIMPL;
  if (delegate_)
    hr = delegate_->Clone(stream);
  return hr;
}

