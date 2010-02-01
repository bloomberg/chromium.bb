// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_URLMON_URL_REQUEST_PRIVATE_H_
#define CHROME_FRAME_URLMON_URL_REQUEST_PRIVATE_H_

#include <atlbase.h>
#include <atlcom.h>
#include <string>
#include <vector>

#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_status.h"

class UrlmonUrlRequest
  : public CComObjectRootEx<CComMultiThreadModel>,
    public PluginUrlRequest,
    public IServiceProviderImpl<UrlmonUrlRequest>,
    public IBindStatusCallback,
    public IHttpNegotiate,
    public IAuthenticate,
    public IHttpSecurity {
 public:
  static int instance_count_;
  virtual bool Start();
  virtual void Stop();
  virtual bool Read(int bytes_to_read);

  // Special function needed by ActiveDocument::Load()
  HRESULT ConnectToExistingMoniker(IMoniker* moniker, IBindCtx* context,
                                   const std::wstring& url);

  // Used from "OnDownloadRequestInHost".
  void StealMoniker(IMoniker** moniker);

  // Parent Window for UrlMon error dialogs
  void set_parent_window(HWND parent_window) {
    parent_window_ = parent_window;
  }

 protected:
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

 protected:
  void ReleaseBindings();

  static const size_t kCopyChunkSize = 32 * 1024;
  // A fake stream class to make it easier to copy received data using
  // IStream::CopyTo instead of allocating temporary buffers and keeping
  // track of data copied so far.
  class SendStream : public CComObjectRoot, public IStream {
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

    STDMETHOD(Write)(const void * buffer, ULONG size, ULONG* size_written);
    STDMETHOD(Read)(void* pv, ULONG cb, ULONG* read) {
      DCHECK(false) << __FUNCTION__;
      return E_NOTIMPL;
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
    // Adds data to the end of the cache.
    bool Append(IStream* source, size_t* bytes_copied);

    // Reads from the cache.
    bool Read(IStream* dest, size_t size, size_t* bytes_copied);

    // Returns the size of the cache.
    size_t Size() const;

    // Returns true if the cache has valid data.
    bool is_valid() const {
      return Size() != 0;
    }

  protected:
    std::vector<byte> cache_;
    char read_buffer_[kCopyChunkSize];
  };

  HRESULT StartAsyncDownload();
  void NotifyDelegateAndDie();
  int GetHttpResponseStatus() const;
  std::string GetHttpHeaders() const;
  static net::Error HresultToNetError(HRESULT hr);

 private:
  // This class simplifies tracking the progress of operation. We have 3 main
  // states: DONE, WORKING and ABORTING.
  // When in [DONE] or [ABORTING] state, there is additional information
  // about the result of operation.
  // Start(), SetRedirected(), Cancel() and Done() methods trigger the state
  // change. See comments bellow.
  class Status {
   public:
    enum State {DONE, ABORTING, WORKING};
    struct Redirection {
      Redirection() : http_code(0) { }
      int http_code;
      std::string utf8_url;
    };

    Status() : state_(Status::DONE) {
    }

    State get_state() const {
      return state_;
    }

    // Switch from [DONE] to [WORKING].
    void Start() {
      DCHECK_EQ(state_, DONE);
      state_ = WORKING;
    }

    // Save redirection information and switch to [ABORTING] state.
    // Assumes binding_->Abort() will be called!
    void SetRedirected(int http_code, const std::string& utf8_url) {
      DCHECK_EQ(state_, WORKING);
      DCHECK_EQ(result_.status(), URLRequestStatus::SUCCESS);
      redirect_.utf8_url = utf8_url;

      // At times we receive invalid redirect codes like 0, 200, etc. We
      // default to 302 in this case.
      redirect_.http_code = http_code;
      if (!net::HttpResponseHeaders::IsRedirectResponseCode(http_code))
        redirect_.http_code = 302;

      state_ = ABORTING;
    }

    // Set the result as URLRequestStatus::CANCELED.
    // Switch to [ABORTING] state (if not already in that state).
    void Cancel() {
      if (state_ == DONE)
        return;

      if (state_ == WORKING) {
        state_ =  ABORTING;
      } else {
        // state_ == ABORTING
        redirect_.http_code = 0;
        redirect_.utf8_url.clear();
      }

      set_result(URLRequestStatus::CANCELED, 0);
    }

    void Done() {
      state_ = DONE;
    }

    bool was_redirected() const {
      return redirect_.http_code != 0;
    }

    const Redirection& get_redirection() const {
      return redirect_;
    }

    const URLRequestStatus& get_result() const {
      return result_;
    }

    void set_result(URLRequestStatus::Status status, int os_error) {
      result_.set_status(status);
      result_.set_os_error(os_error);
    }

    void set_result(HRESULT hr) {
      result_.set_status(FAILED(hr)? URLRequestStatus::FAILED:
                                     URLRequestStatus::SUCCESS);
      result_.set_os_error(HresultToNetError(hr));
    }

   private:
    Redirection redirect_;
    State state_;
    URLRequestStatus result_;
  };

  Status status_;
  ScopedComPtr<IBinding> binding_;
  ScopedComPtr<IMoniker> moniker_;
  ScopedComPtr<IBindCtx> bind_context_;
  Cache cached_data_;
  size_t pending_read_size_;
  PlatformThreadId thread_;
  HWND parent_window_;

  DISALLOW_COPY_AND_ASSIGN(UrlmonUrlRequest);
};

#endif  // CHROME_FRAME_URLMON_URL_REQUEST_PRIVATE_H_
