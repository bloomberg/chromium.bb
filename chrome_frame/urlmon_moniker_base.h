// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_URLMON_MONIKER_BASE_H_
#define CHROME_FRAME_URLMON_MONIKER_BASE_H_

#include <atlbase.h>
#include <atlcom.h>

// An implementation of IStream (minus IUnknown) that delegates to
// another source.  Most of the methods contain a NOTREACHED() as the
// class is used by the ReadStreamCache class where we don't expect those
// calls under normal circumstances.
class DelegatingReadStream : public IStream {
 public:
  DelegatingReadStream() {
  }

  ~DelegatingReadStream() {
  }

  void SetDelegate(IStream* delegate) {
    delegate_ = delegate;
  }

  // ISequentialStream.
  STDMETHOD(Read)(void* pv, ULONG cb, ULONG* read) {
    return delegate_->Read(pv, cb, read);
  }

  STDMETHOD(Write)(const void* pv, ULONG cb, ULONG* written) {
    NOTREACHED();
    return delegate_->Write(pv, cb, written);
  }

  // IStream.
  STDMETHOD(Seek)(LARGE_INTEGER move, DWORD origin, ULARGE_INTEGER* new_pos) {
    NOTREACHED();
    return delegate_->Seek(move, origin, new_pos);
  }

  STDMETHOD(SetSize)(ULARGE_INTEGER new_size) {
    NOTREACHED();
    return delegate_->SetSize(new_size);
  }

  STDMETHOD(CopyTo)(IStream* stream, ULARGE_INTEGER cb, ULARGE_INTEGER* read,
                    ULARGE_INTEGER* written) {
    NOTREACHED();
    return delegate_->CopyTo(stream, cb, read, written);
  }

  STDMETHOD(Commit)(DWORD commit_flags) {
    NOTREACHED();
    return delegate_->Commit(commit_flags);
  }

  STDMETHOD(Revert)() {
    NOTREACHED();
    return delegate_->Revert();
  }

  STDMETHOD(LockRegion)(ULARGE_INTEGER offset, ULARGE_INTEGER cb,
                        DWORD lock_type) {
    NOTREACHED();
    return delegate_->LockRegion(offset, cb, lock_type);
  }

  STDMETHOD(UnlockRegion)(ULARGE_INTEGER offset, ULARGE_INTEGER cb,
                          DWORD lock_type) {
    NOTREACHED();
    return delegate_->UnlockRegion(offset, cb, lock_type);
  }

  STDMETHOD(Stat)(STATSTG* stat_stg, DWORD stat_flag) {
    DCHECK(delegate_);
    return delegate_->Stat(stat_stg, stat_flag);
  }

  STDMETHOD(Clone)(IStream** stream) {
    NOTREACHED();
    return delegate_->Clone(stream);
  }

 protected:
  ScopedComPtr<IStream> delegate_;
};

// A very basic implementation of IBinding that forwards all calls
// to a delegate.
class DelegatingBinding : public IBinding {
 public:
  DelegatingBinding() {
  }

  ~DelegatingBinding() {
  }

  IBinding* delegate() const {
    return delegate_;
  }

  void SetDelegate(IBinding* delegate) {
    delegate_ = delegate;
  }

  STDMETHOD(Abort)() {
    DCHECK(delegate_);
    if (!delegate_)
      return S_OK;
    return delegate_->Abort();
  }

  STDMETHOD(Suspend)() {
    DCHECK(delegate_);
    if (!delegate_)
      return E_FAIL;
    return delegate_->Suspend();
  }

  STDMETHOD(Resume)() {
    DCHECK(delegate_);
    if (!delegate_)
      return E_FAIL;
    return delegate_->Resume();
  }

  STDMETHOD(SetPriority)(LONG priority) {
    DCHECK(delegate_);
    if (!delegate_)
      return E_FAIL;
    return delegate_->SetPriority(priority);
  }

  STDMETHOD(GetPriority)(LONG* priority) {
    DCHECK(delegate_);
    if (!delegate_)
      return E_FAIL;
    return delegate_->GetPriority(priority);
  }

  STDMETHOD(GetBindResult)(CLSID* protocol, DWORD* result_code,
                           LPOLESTR* result, DWORD* reserved) {
    DCHECK(delegate_);
    if (!delegate_)
      return E_FAIL;
    return delegate_->GetBindResult(protocol, result_code, result, reserved);
  }

 protected:
  ScopedComPtr<IBinding> delegate_;
};

#endif  // CHROME_FRAME_URLMON_MONIKER_BASE_H_

