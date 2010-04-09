// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_URLMON_URL_REQUEST_PRIVATE_H_
#define CHROME_FRAME_URLMON_URL_REQUEST_PRIVATE_H_

#include <atlbase.h>
#include <atlcom.h>
#include <string>
#include <deque>

#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class RequestData;

class UrlmonUrlRequest
  : public CComObjectRootEx<CComMultiThreadModel>,
    public PluginUrlRequest,
    public IServiceProviderImpl<UrlmonUrlRequest>,
    public IBindStatusCallback,
    public IHttpNegotiate,
    public IAuthenticate,
    public IHttpSecurity {
 public:
  virtual bool Start();
  virtual void Stop();
  virtual bool Read(int bytes_to_read);

  // Special function needed by ActiveDocument::Load()
  HRESULT UseBindCtx(IMoniker* moniker, LPBC bc);

  // Used from "DownloadRequestInHost".
  void StealMoniker(IMoniker** moniker, IBindCtx** bctx);

  // Parent Window for UrlMon error dialogs
  void set_parent_window(HWND parent_window) {
    parent_window_ = parent_window;
  }

  // This function passes information on whether ChromeFrame is running in
  // privileged mode.
  void set_privileged_mode(bool privileged_mode) {
    privileged_mode_ = privileged_mode;
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

  // Manage data caching. Note: this class supports cache
  // size less than 2GB
  FRIEND_TEST(UrlmonUrlRequestCache, ReadWrite);
  class Cache {
   public:
    Cache() : size_(0), read_offset_(0), write_offset_(0) {
    }

    ~Cache();

    // Adds data to the end of the cache.
    bool Append(IStream* source);
    // Copies up to |bytes| bytes from the cache to |dest| buffer.
    bool Read(void* dest, size_t bytes, size_t* bytes_copied);

    // Returns the size of the cache.
    size_t size() const {
      return size_;
    }

    // Returns true if the cache has valid data.
    bool is_valid() const {
      return size() != 0;
    }

   private:
    FRIEND_TEST(UrlmonUrlRequestCache, ReadWrite);
    void GetWriteBuffer(void** dest, size_t* bytes_avail);
    void BytesWritten(size_t bytes);
    void GetReadBuffer(void** src, size_t* bytes_avail);
    void BytesRead(size_t bytes);

    static const size_t BUF_SIZE = 0x8000;  // 32k
    std::deque<uint8*> cache_;
    size_t size_;
    size_t read_offset_;
    size_t write_offset_;
    std::deque<uint8*> pool_;
  };

  HRESULT StartAsyncDownload();
  void NotifyDelegateAndDie();
  int GetHttpResponseStatus() const;
  std::string GetHttpHeaders() const;
  static net::Error HresultToNetError(HRESULT hr);

 private:
  size_t SendDataToDelegate(size_t bytes);
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
  bool headers_received_;
  int calling_delegate_;  // re-entrancy protection.
  // Set to true if the ChromeFrame instance is running in privileged mode.
  bool privileged_mode_;
  DISALLOW_COPY_AND_ASSIGN(UrlmonUrlRequest);
};

#endif  // CHROME_FRAME_URLMON_URL_REQUEST_PRIVATE_H_
