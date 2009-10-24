// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_URLMON_URL_REQUEST_H_
#define CHROME_FRAME_URLMON_URL_REQUEST_H_

#include <urlmon.h>
#include <atlbase.h>
#include <atlcom.h>

#include <algorithm>

#include "base/lock.h"
#include "base/platform_thread.h"
#include "base/thread.h"
#include "base/scoped_comptr_win.h"
#include "chrome_frame/plugin_url_request.h"
#include "chrome_frame/chrome_frame_delegate.h"

#include "net/base/net_errors.h"
#include "net/base/upload_data.h"

class UrlmonUrlRequest
    : public CComObjectRootEx<CComSingleThreadModel>,
      public PluginUrlRequest,
      public IServiceProviderImpl<UrlmonUrlRequest>,
      public IBindStatusCallback,
      public IHttpNegotiate,
      public IAuthenticate,
      public IHttpSecurity {
 public:
  UrlmonUrlRequest();
  ~UrlmonUrlRequest();

BEGIN_COM_MAP(UrlmonUrlRequest)
  COM_INTERFACE_ENTRY(IHttpNegotiate)
  COM_INTERFACE_ENTRY(IServiceProvider)
  COM_INTERFACE_ENTRY(IBindStatusCallback)
  COM_INTERFACE_ENTRY(IWindowForBindingUI)
  COM_INTERFACE_ENTRY(IAuthenticate)
  COM_INTERFACE_ENTRY(IHttpSecurity)
END_COM_MAP()

BEGIN_SERVICE_MAP(UrlmonUrlRequest)
  SERVICE_ENTRY(IID_IHttpNegotiate);
END_SERVICE_MAP()

  // PluginUrlRequest implementation
  virtual bool Start();
  virtual void Stop();
  virtual bool Read(int bytes_to_read);

  // IBindStatusCallback implementation
  STDMETHOD(OnStartBinding)(DWORD reserved, IBinding* binding);
  STDMETHOD(GetPriority)(LONG* priority);
  STDMETHOD(OnLowResource)(DWORD reserved);
  STDMETHOD(OnProgress)(ULONG progress, ULONG max_progress,
      ULONG status_code, LPCWSTR status_text);
  STDMETHOD(OnStopBinding)(HRESULT result, LPCWSTR error);
  STDMETHOD(GetBindInfo)(DWORD* bind_flags, BINDINFO* bind_info);
  STDMETHOD(OnDataAvailable)(DWORD flags, DWORD size, FORMATETC* formatetc,
      STGMEDIUM* storage);
  STDMETHOD(OnObjectAvailable)(REFIID iid, IUnknown* object);

  // IHttpNegotiate implementation
  STDMETHOD(BeginningTransaction)(const wchar_t* url,
      const wchar_t* current_headers, DWORD reserved,
      wchar_t** additional_headers);
  STDMETHOD(OnResponse)(DWORD dwResponseCode, const wchar_t* response_headers,
      const wchar_t* request_headers, wchar_t** additional_headers);

  // IWindowForBindingUI implementation. This interface is used typically to
  // query the window handle which URLMON uses as the parent of error dialogs.
  STDMETHOD(GetWindow)(REFGUID guid_reason, HWND* parent_window);

  // IAuthenticate implementation. Used to return the parent window for the
  // dialog displayed by IE for authenticating with a proxy.
  STDMETHOD(Authenticate)(HWND* parent_window, LPWSTR* user_name,
                          LPWSTR* password);

  // IHttpSecurity implementation.
  STDMETHOD(OnSecurityProblem)(DWORD problem);

  HRESULT ConnectToExistingMoniker(IMoniker* moniker, IBindCtx* context,
                                   const std::wstring& url);

  void set_parent_window(HWND parent_window) {
    parent_window_ = parent_window;
  }

  // Needed to support PostTask.
  static bool ImplementsThreadSafeReferenceCounting() {
    return true;
  }

  // URL requests are handled on this thread.
  void set_worker_thread(base::Thread* worker_thread) {
    worker_thread_ = worker_thread;
  }

  void set_task_marshaller(TaskMarshaller* task_marshaller) {
    task_marshaller_ = task_marshaller;
  }

 protected:
  // The following functions issue and handle Urlmon requests on the dedicated
  // Urlmon thread.
  void StartAsync();
  void StopAsync();
  void ReadAsync(int bytes_to_read);

  static const size_t kCopyChunkSize = 32 * 1024;
  // URL requests are handled on this thread.
  base::Thread* worker_thread_;

  TaskMarshaller* task_marshaller_;

  // A fake stream class to make it easier to copy received data using
  // IStream::CopyTo instead of allocating temporary buffers and keeping
  // track of data copied so far.
  class SendStream
      : public CComObjectRoot,
        public IStream {
   public:
    SendStream() {
    }

  BEGIN_COM_MAP(SendStream)
    COM_INTERFACE_ENTRY(IStream)
    COM_INTERFACE_ENTRY(ISequentialStream)
  END_COM_MAP()

    void Initialize(UrlmonUrlRequest* request) {
      request_ = request;
    }

    STDMETHOD(Read)(void* pv, ULONG cb, ULONG* read) {
      DCHECK(false) << __FUNCTION__;
      return E_NOTIMPL;
    }

    STDMETHOD(Write)(const void * buffer, ULONG size, ULONG* size_written) {
      DCHECK(request_);
      int size_to_write = static_cast<int>(
          std::min(static_cast<ULONG>(MAXINT), size));
      request_->OnReadComplete(buffer, size_to_write);
      if (size_written)
        *size_written = size_to_write;
      return S_OK;
    }

    STDMETHOD(Seek)(LARGE_INTEGER move, DWORD origin, ULARGE_INTEGER* new_pos) {
      DCHECK(false) << __FUNCTION__;
      return E_NOTIMPL;
    }

    STDMETHOD(SetSize)(ULARGE_INTEGER new_size) {
      DCHECK(false) << __FUNCTION__;
      return E_NOTIMPL;
    }

    STDMETHOD(CopyTo)(IStream* stream, ULARGE_INTEGER cb, ULARGE_INTEGER* read,
                      ULARGE_INTEGER* written) {
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

    STDMETHOD(Stat)(STATSTG *pstatstg, DWORD grfStatFlag) {
      return E_NOTIMPL;
    }

    STDMETHOD(Clone)(IStream** stream) {
      DCHECK(false) << __FUNCTION__;
      return E_NOTIMPL;
    }

   protected:
    scoped_refptr<UrlmonUrlRequest> request_;
    DISALLOW_COPY_AND_ASSIGN(SendStream);
  };

  // Manage data caching. Note: this class supports cache
  // size less than 2GB
  class Cache {
   public:
    bool Create();

    // Adds data to the end of the cache.
    bool Append(IStream* source, size_t* bytes_copied);

    // Reads from the cache.
    bool Read(IStream* dest, size_t size, size_t* bytes_copied);

    size_t Size();
    size_t CurrentPos();
    size_t SizeRemaining();
    void Clear();
    bool is_valid() const {
      return (stream_ != NULL);
    }

   protected:
    ScopedComPtr<IStream> stream_;
    char read_buffer_[kCopyChunkSize];
  };

  HRESULT StartAsyncDownload();
  void EndRequest();
  // Executes in the context of the UI thread and releases the outstanding
  // reference to us. It also deletes the request mapping for this instance.
  void EndRequestInternal();
  int GetHttpResponseStatus() const;

  static net::Error HresultToNetError(HRESULT hr);

 private:
  std::wstring redirect_url_;
  int redirect_status_;
  ScopedComPtr<IBinding> binding_;
  ScopedComPtr<IMoniker> moniker_;
  ScopedComPtr<IBindCtx> bind_context_;
  Cache cached_data_;
  size_t pending_read_size_;
  URLRequestStatus status_;

  uint64 post_data_len_;

  PlatformThreadId thread_;
  static int instance_count_;
  HWND parent_window_;
  DISALLOW_COPY_AND_ASSIGN(UrlmonUrlRequest);
};

#endif  // CHROME_FRAME_URLMON_URL_REQUEST_H_

