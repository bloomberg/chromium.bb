// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_STREAM_IMPL_H_
#define CHROME_FRAME_STREAM_IMPL_H_

#include <atlbase.h>
#include <atlcom.h>

#include "base/win/scoped_comptr.h"

// A generic base class for IStream implementation. If provided
// with a delegate, it delegated calls to it otherwise can be
// used a as a base class for an IStream implementation.
class StreamImpl : public IStream {
 public:
  StreamImpl() {}

  void Initialize(IStream* delegate);

  STDMETHOD(Write)(const void * buffer, ULONG size, ULONG* size_written);
  STDMETHOD(Read)(void* pv, ULONG cb, ULONG* read);
  STDMETHOD(Seek)(LARGE_INTEGER move, DWORD origin, ULARGE_INTEGER* new_pos);
  STDMETHOD(SetSize)(ULARGE_INTEGER new_size);
  STDMETHOD(CopyTo)(IStream* stream, ULARGE_INTEGER cb, ULARGE_INTEGER* read,
                    ULARGE_INTEGER* written);
  STDMETHOD(Commit)(DWORD flags);
  STDMETHOD(Revert)();
  STDMETHOD(LockRegion)(ULARGE_INTEGER offset, ULARGE_INTEGER cb,
                        DWORD type);
  STDMETHOD(UnlockRegion)(ULARGE_INTEGER offset, ULARGE_INTEGER cb,
                          DWORD type);
  STDMETHOD(Stat)(STATSTG* statstg, DWORD flag);
  STDMETHOD(Clone)(IStream** stream);

 protected:
  base::win::ScopedComPtr<IStream> delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(StreamImpl);
};

#endif  // CHROME_FRAME_STREAM_IMPL_H_
