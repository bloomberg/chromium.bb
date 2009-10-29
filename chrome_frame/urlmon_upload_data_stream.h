// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_URLMON_UPLOAD_DATA_STREAM_H_
#define CHROME_FRAME_URLMON_UPLOAD_DATA_STREAM_H_

#include <urlmon.h>
#include <atlbase.h>
#include <atlcom.h>

#include "base/logging.h"
#include "base/ref_counted.h"

#include "net/base/upload_data.h"
#include "net/base/upload_data_stream.h"

// Provides an IStream interface to the very different UploadDataStream
// implementation.
class UrlmonUploadDataStream : public CComObjectRootEx<CComMultiThreadModel>,
                               public IStream {
 public:
  UrlmonUploadDataStream() {}

  BEGIN_COM_MAP(UrlmonUploadDataStream)
    COM_INTERFACE_ENTRY(ISequentialStream)
    COM_INTERFACE_ENTRY(IStream)
  END_COM_MAP()

  void Initialize(net::UploadData* upload_data);

  // Partial implementation of IStream.
  STDMETHOD(Read)(void* pv, ULONG cb, ULONG* read);

  // E_NOTIMPL the rest and DCHECK if they get called (could also use
  // IStreamImpl but we'd lose the DCHECKS().
  STDMETHOD(Write)(const void * buffer, ULONG size, ULONG* size_written) {
    DCHECK(false) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(CopyTo)(IStream* stream, ULARGE_INTEGER cb, ULARGE_INTEGER* read,
                    ULARGE_INTEGER* written) {
    DCHECK(false) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(Seek)(LARGE_INTEGER move, DWORD origin, ULARGE_INTEGER* new_pos);

  STDMETHOD(SetSize)(ULARGE_INTEGER new_size) {
    DCHECK(false) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(Commit)(DWORD flags) {
    DCHECK(false) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(Revert)() {
    DCHECK(false) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(LockRegion)(ULARGE_INTEGER offset, ULARGE_INTEGER cb,
                        DWORD type) {
    DCHECK(false) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(UnlockRegion)(ULARGE_INTEGER offset, ULARGE_INTEGER cb,
                          DWORD type) {
    DCHECK(false) << __FUNCTION__;
    return E_NOTIMPL;
  }

  STDMETHOD(Stat)(STATSTG *pstatstg, DWORD grfStatFlag);

  STDMETHOD(Clone)(IStream** stream) {
    DCHECK(false) << __FUNCTION__;
    return E_NOTIMPL;
  }

 private:
  scoped_refptr<net::UploadData> upload_data_;
  scoped_ptr<net::UploadDataStream> request_body_stream_;
};

#endif  // CHROME_FRAME_URLMON_UPLOAD_DATA_STREAM_H_
