// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_URLMON_BIND_STATUS_CALLBACK_H_
#define CHROME_FRAME_URLMON_BIND_STATUS_CALLBACK_H_

#include <atlbase.h>
#include <atlcom.h>

#include "base/memory/scoped_ptr.h"
#include "chrome_frame/bind_status_callback_impl.h"
#include "chrome_frame/stream_impl.h"


// A fake stream class to serve cached data to arbitrary
// IBindStatusCallback
class CacheStream : public CComObjectRoot, public StreamImpl {
 public:
  BEGIN_COM_MAP(CacheStream)
    COM_INTERFACE_ENTRY(IStream)
    COM_INTERFACE_ENTRY(ISequentialStream)
  END_COM_MAP()

  CacheStream() : cache_(NULL), size_(0), position_(0), eof_(false) {
  }
  HRESULT Initialize(const char* cache, size_t size, bool eof);
  static HRESULT BSCBFeedData(IBindStatusCallback* bscb, const char* data,
                              size_t size, CLIPFORMAT clip_format,
                              size_t flags, bool eof);

  // IStream overrides
  STDMETHOD(Read)(void* pv, ULONG cb, ULONG* read);

 protected:
  scoped_ptr<char> cache_;
  size_t size_;
  size_t position_;
  bool eof_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CacheStream);
};

// Utility class for data sniffing
class SniffData {
 public:
  SniffData() : renderer_type_(OTHER), size_(0), eof_(false) {}

  enum RendererType {
    UNDETERMINED,
    CHROME,
    OTHER
  };

  HRESULT InitializeCache(const std::wstring& url);
  HRESULT ReadIntoCache(IStream* stream, bool force_determination);
  HRESULT DrainCache(IBindStatusCallback* bscb, DWORD bscf,
                     CLIPFORMAT clip_format);
  void DetermineRendererType(bool last_chance);

  bool is_undetermined() const {
    return (UNDETERMINED == renderer_type_);
  }
  bool is_chrome() const {
    return (CHROME == renderer_type_);
  }

  RendererType renderer_type() const {
    return renderer_type_;
  }

  size_t size() const {
    return size_;
  }

  bool is_cache_valid() {
    return (size_ != 0);
  }

  base::win::ScopedComPtr<IStream> cache_;
  std::wstring url_;
  RendererType renderer_type_;
  size_t size_;

  static const size_t kMaxSniffSize = 2 * 1024;
  bool eof_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SniffData);
};

// A wrapper for bind status callback in IMoniker::BindToStorage
class BSCBStorageBind : public BSCBImpl {
 public:
  typedef BSCBImpl CallbackImpl;
  BSCBStorageBind();
  ~BSCBStorageBind();

BEGIN_COM_MAP(BSCBStorageBind)
  COM_INTERFACE_ENTRY(IBindStatusCallback)
  COM_INTERFACE_ENTRY_CHAIN(CallbackImpl)
END_COM_MAP()

  HRESULT Initialize(IMoniker* moniker, IBindCtx* bind_ctx);
  HRESULT MayPlayBack(DWORD flags);

  // IBindStatusCallback
  STDMETHOD(OnProgress)(ULONG progress, ULONG progress_max, ULONG status_code,
                        LPCWSTR status_text);
  STDMETHOD(OnDataAvailable)(DWORD flags, DWORD size, FORMATETC* format_etc,
                             STGMEDIUM* stgmed);
  STDMETHOD(OnStopBinding)(HRESULT hresult, LPCWSTR error);

 protected:
  // is it a good time to start caching progress notifications
  bool ShouldCacheProgress(ULONG status_code) const;

 protected:
  SniffData data_sniffer_;

  // A structure to cache the progress notifications.
  class Progress {
   public:
    Progress(ULONG progress, ULONG progress_max, ULONG status_code,
             const wchar_t* status_text)
        : progress_(progress),
          progress_max_(progress_max),
          status_code_(status_code),
          status_text_(NULL) {
      if (status_text) {
        int len = lstrlenW(status_text) + 1;
        status_text_.reset(new wchar_t[len]);
        if (status_text_.get()) {
          lstrcpynW(status_text_.get(), status_text, len);
        } else {
          NOTREACHED();
        }
      }
    }

    ~Progress() {
    }

    ULONG progress() const {
      return progress_;
    }

    ULONG progress_max() const {
      return progress_max_;
    }

    ULONG status_code() const {
      return status_code_;
    }

    const wchar_t* status_text() const {
      return status_text_.get();
    }

   protected:
    ULONG progress_;
    ULONG progress_max_;
    ULONG status_code_;
    // We don't use std::wstring here since we want to be able to play
    // progress notifications back exactly as we got them.  NULL and L"" are
    // not equal.
    scoped_ptr<wchar_t> status_text_;
  };

  typedef std::vector<Progress*> ProgressVector;
  ProgressVector saved_progress_;
  CLIPFORMAT clip_format_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BSCBStorageBind);
};

#endif  // CHROME_FRAME_URLMON_BIND_STATUS_CALLBACK_H_
