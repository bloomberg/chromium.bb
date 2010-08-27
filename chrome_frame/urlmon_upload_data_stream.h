// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_URLMON_UPLOAD_DATA_STREAM_H_
#define CHROME_FRAME_URLMON_UPLOAD_DATA_STREAM_H_

#include <urlmon.h>
#include <atlbase.h>
#include <atlcom.h>

#include "base/ref_counted.h"
#include "chrome_frame/stream_impl.h"
#include "net/base/upload_data.h"
#include "net/base/upload_data_stream.h"

// Provides an IStream interface to the very different UploadDataStream
// implementation.
class UrlmonUploadDataStream : public CComObjectRootEx<CComMultiThreadModel>,
                               public StreamImpl {
 public:
  UrlmonUploadDataStream() {}

  BEGIN_COM_MAP(UrlmonUploadDataStream)
    COM_INTERFACE_ENTRY(ISequentialStream)
    COM_INTERFACE_ENTRY(IStream)
  END_COM_MAP()

  void Initialize(net::UploadData* upload_data);

  // Partial implementation of IStream.
  STDMETHOD(Read)(void* pv, ULONG cb, ULONG* read);
  STDMETHOD(Seek)(LARGE_INTEGER move, DWORD origin, ULARGE_INTEGER* new_pos);
  STDMETHOD(Stat)(STATSTG *pstatstg, DWORD grfStatFlag);

 private:
  scoped_refptr<net::UploadData> upload_data_;
  scoped_ptr<net::UploadDataStream> request_body_stream_;
};

#endif  // CHROME_FRAME_URLMON_UPLOAD_DATA_STREAM_H_
