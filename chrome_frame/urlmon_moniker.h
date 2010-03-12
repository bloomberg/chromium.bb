// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_URLMON_MONIKER_H_
#define CHROME_FRAME_URLMON_MONIKER_H_

#include <atlbase.h>
#include <atlcom.h>
#include <urlmon.h>
#include <string>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/scoped_comptr_win.h"
#include "base/thread_local.h"

#include "chrome_frame/urlmon_moniker_base.h"
#include "chrome_frame/utils.h"

// This file contains classes that are used to cache the contents of a top-level
// http request (not for sub frames) while that request is parsed for the
// presence of a meta tag indicating that the page should be rendered in CF.

// Here are a few scenarios we handle and how the classes come to play.

//
// Scenario 1: Non CF url navigation through address bar (www.msn.com)
// - Bho::BeforeNavigate - top level url = www.msn.com
// - MSHTML -> MonikerPatch::BindToStorage.
//   (IEFrame starts this by calling mshtml!*SuperNavigate*)
//    - request_data is NULL
//    - check if the url is a top level url
//    - iff the url is a top level url, we switch in our own callback object
//      and hook it up to the bind context (CFUrlmonBindStatusCallback)
//      The callback object caches the document in memory.
//    - otherwise just call the original
// -  Bho::OnHttpEquiv is called with done == TRUE.  At this point we determine
//    that the page did not have the CF meta tag in it and we delete the cache.
// -  The page loads in mshtml.
//

//
// Scenario 2: CF navigation through address bar URL
// - Bho::BeforeNavigate - top level url = http://wave.google.com/
// - MSHTML -> MonikerPatch::BindToStorage.
//   (IEFrame starts this by calling mshtml!*SuperNavigate*)
//    - request_data is NULL
//    - check if the url is a top level url
//    - iff the url is a top level url (in this case, yes), we switch in our own
//      callback object and hook it up to the bind context
//      (CFUrlmonBindStatusCallback)
//    - As the document is fetched, the callback object caches the
//      document in memory.
// -  Bho::OnHttpEquiv (done == FALSE)
//    - mgr->NavigateToCurrentUrlInCF
//        - Set TLS (MarkBrowserOnThreadForCFNavigation)
//        - Create new bind context, moniker for the URL
//        - NavigateBrowserToMoniker with new moniker, bind context
//        - Our callback _may_ get an error from mshtml indicating that mshtml
//          isn't interested in the data anymore (since we started a new
//          navigation).  If that happens, our callback class
//          (CFUrlmonBindStatusCallback) will continue to cache the document
//          until all of it has been retrieved successfully.  When the data
//          is all read, we report INET_E_TERMINATE_BIND (see SimpleBindingImpl)
//          as the end result.
//        - In the case where all of the data has been downloaded before
//        - OnHttpEquiv is called, we will already have the cache but the
//          end bind status in the callback will be S_OK.
// - Bho::BeforeNavigate2 - top level url = http://wave.google.com/
// - IEFrame -> MonikerPatch::BindToStorage
//    - request_data is not NULL since we now have a cached copy of the content.
//    - We call BindToStorageFromCache.
// - HttpNegotiatePatch::ReportProgress
//    - Check TLS (CheckForCFNavigation) and report chrome mime type
//    - IEFrame does the following:
//        - Creates a new moniker
//        - Calls MonikerPatch::BindToObject
//    - We create an instance of ChromeActiveDocument and initialize it
//      with the cached document.
//    - ChromeActiveDocument gives the UrlmonUrlRequestManager the cached
//      contents (RequestData) which the manager will use as the content
//      when serving up content for the CF document.
//

//
// Scenario 3: CF navigation through mshtml link
//  Same as scenario #2.
//

//
// Scenario 4: CF navigation through link click in chrome loads non CF page
// - Link click comes to ChromeActiveDocument::OnOpenURL
//    - web_browser->Navigate with URL
// - [Scenario 1]
//

//
// Scenario 5: CF navigation through link click in chrome loads CF page
// - Link click comes to ChromeActiveDocument::OnOpenURL
//    - web_browser->Navigate with URL
// - [Scenario 2]
//

// An implementation of IStream that delegates to another source
// but caches all data that is read from the source.
class ReadStreamCache
  : public CComObjectRootEx<CComSingleThreadModel>,
    public DelegatingReadStream {
 public:
  ReadStreamCache() {
    DLOG(INFO) << __FUNCTION__;
  }

  ~ReadStreamCache() {
    DLOG(INFO) << __FUNCTION__ << " cache: " << GetCacheSize();
  }

BEGIN_COM_MAP(ReadStreamCache)
  COM_INTERFACE_ENTRY(IStream)
  COM_INTERFACE_ENTRY(ISequentialStream)
END_COM_MAP()

  IStream* cache() const {
    return cache_;
  }

  void RewindCache() const;

  HRESULT WriteToCache(const void* data, ULONG size, ULONG* written);

  // Returns how many bytes we've cached.  Used for logging.
  size_t GetCacheSize() const;

  // ISequentialStream.
  STDMETHOD(Read)(void* pv, ULONG cb, ULONG* read);

 protected:
  ScopedComPtr<IStream> cache_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ReadStreamCache);
};

// Basic implementation of IBinding with the option of offering a way
// to override the bind result error value.
class SimpleBindingImpl
  : public CComObjectRootEx<CComSingleThreadModel>,
    public DelegatingBinding {
 public:
  SimpleBindingImpl() : bind_results_(S_OK) {
  }

  ~SimpleBindingImpl() {
  }

BEGIN_COM_MAP(SimpleBindingImpl)
  COM_INTERFACE_ENTRY(IBinding)
  COM_INTERFACE_ENTRY_FUNC_BLIND(0, DelegateQI)
END_COM_MAP()

  static STDMETHODIMP DelegateQI(void* obj, REFIID iid, void** ret,
                                 DWORD cookie);

  STDMETHOD(GetBindResult)(CLSID* protocol, DWORD* result_code,
                           LPOLESTR* result, DWORD* reserved);

  void OverrideBindResults(HRESULT results) {
    bind_results_ = results;
  }

 protected:
  HRESULT bind_results_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SimpleBindingImpl);
};

class RequestHeaders
  : public base::RefCountedThreadSafe<RequestHeaders> {
 public:
  RequestHeaders() : response_code_(-1) {
  }

  ~RequestHeaders() {
  }

  void OnBeginningTransaction(const wchar_t* url, const wchar_t* headers,
                              const wchar_t* additional_headers);

  void OnResponse(DWORD response_code, const wchar_t* response_headers,
                  const wchar_t* request_headers);

  const std::wstring& request_url() const {
    return request_url_;
  }

  // Invokes BeginningTransaction and OnResponse on the |http| object
  // providing already cached headers and status values.
  // Any additional headers returned from either of the two methods are ignored.
  HRESULT FireHttpNegotiateEvents(IHttpNegotiate* http) const;

  std::string GetReferrer();

 protected:
  std::wstring request_url_;
  std::wstring begin_request_headers_;
  std::wstring additional_request_headers_;
  std::wstring request_headers_;
  std::wstring response_headers_;
  DWORD response_code_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RequestHeaders);
};

// Holds cached data for a urlmon request.
class RequestData
  : public base::RefCountedThreadSafe<RequestData> {
 public:
  RequestData();
  ~RequestData();

  void Initialize(RequestHeaders* headers);

  // Calls IBindStatusCallback::OnDataAvailable and caches any data that is
  // read during that operation.
  // We also cache the format of the data stream if available during the first
  // call to this method.
  HRESULT DelegateDataRead(IBindStatusCallback* callback, DWORD flags,
                           DWORD size, FORMATETC* format, STGMEDIUM* storage,
                           size_t* bytes_read);

  // Reads everything that's available from |data| into a cached stream.
  void CacheAll(IStream* data);

  // Returns a new stream object to read the cache.
  // The returned stream object's seek pointer is at pos 0.
  HRESULT GetResetCachedContentStream(IStream** clone);

  size_t GetCachedContentSize() const {
    return stream_delegate_->GetCacheSize();
  }

  const FORMATETC& format() const {
    return format_;
  }

  RequestHeaders* headers() const {
    return headers_;
  }

  void set_headers(RequestHeaders* headers) {
    DCHECK(headers);
    DCHECK(headers_ == NULL);
    headers_ = headers;
  }

 protected:
  ScopedComPtr<ReadStreamCache, &GUID_NULL> stream_delegate_;
  FORMATETC format_;
  scoped_refptr<RequestHeaders> headers_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RequestData);
};

// This class is the link between a few static, moniker related functions to
// the bho.  The specific services needed by those functions are abstracted into
// this interface for easier testability.
class NavigationManager {
 public:
  NavigationManager() {
  }

  // Returns the Bho instance for the current thread. This is returned from
  // TLS.  Returns NULL if no instance exists on the current thread.
  static NavigationManager* GetThreadInstance();

  void RegisterThreadInstance();
  void UnregisterThreadInstance();

  virtual ~NavigationManager() {
    DCHECK(GetThreadInstance() != this);
  }

  // Returns the url of the current top level navigation.
  const std::wstring& url() const {
    return url_;
  }

  // Called to set the current top level URL that's being navigated to.
  void set_url(const wchar_t* url) {
    DLOG(INFO) << __FUNCTION__ << " " << url;
    url_ = url;
  }

  // Returns the referrer header value of the current top level navigation.
  const std::string& referrer() const {
    return referrer_;
  }

  void set_referrer(const std::string& referrer) {
    referrer_ = referrer;
  }

  // Called when a top level navigation has finished and we don't need to
  // keep the cached content around anymore.
  virtual void ReleaseRequestData() {
    DLOG(INFO) << __FUNCTION__;
    url_.clear();
    SetActiveRequestData(NULL);
  }

  // Return true if this is a URL that represents a top-level
  // document that might have to be rendered in CF.
  virtual bool IsTopLevelUrl(const wchar_t* url) {
    return lstrcmpiW(url_.c_str(), url) == 0;
  }

  // Called from HttpNegotiatePatch::BeginningTransaction when a request is
  // being issued.  We check the url and headers and see if there is a referrer
  // header that we need to cache.
  virtual void OnBeginningTransaction(bool is_top_level, const wchar_t* url,
                                      const wchar_t* headers,
                                      const wchar_t* additional_headers);

  // Called when we've detected the http-equiv meta tag in the current page
  // and need to switch over from mshtml to CF.
  virtual HRESULT NavigateToCurrentUrlInCF(IBrowserService* browser);

  virtual void SetActiveRequestData(RequestData* request_data);

  // When BindToObject is called on a URL before BindToStorage is called,
  // the request and response headers are reported on that moniker.
  // Later BindToStorage is called to fetch the content.  We use the
  // RequestHeaders class to carry over the headers to the RequestData object
  // that will be created to hold the content.
  virtual void SetActiveRequestHeaders(RequestHeaders* request_headers) {
    request_headers_ = request_headers;
  }

  virtual RequestHeaders* GetActiveRequestHeaders() {
    return request_headers_;
  }

  virtual RequestData* GetActiveRequestData(const wchar_t* url) {
    return IsTopLevelUrl(url) ? request_data_.get() : NULL;
  }

 protected:
  std::string referrer_;
  std::wstring url_;
  scoped_refptr<RequestData> request_data_;
  scoped_refptr<RequestHeaders> request_headers_;
  static base::LazyInstance<base::ThreadLocalPointer<NavigationManager> >
      thread_singleton_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NavigationManager);
};

// static-only class that manages an IMoniker patch.
// We need this patch to stay in the loop when top-level HTML content is
// downloaded that might have the CF http-equiv meta tag.
// When we detect candidates for those requests, we add our own callback
// object (as explained at the top of this file) and use it to cache the
// original document contents in order to avoid multiple network trips
// if we need to switch the renderer over to CF.
class MonikerPatch {
  MonikerPatch() {} // no instances should be created of this class.
 public:
  // Patches two IMoniker methods, BindToObject and BindToStorage.
  static bool Initialize();

  // Nullifies the IMoniker patches.
  static void Uninitialize();

  // Typedefs for IMoniker methods.
  typedef HRESULT (STDMETHODCALLTYPE* IMoniker_BindToObject_Fn)(IMoniker* me,
      IBindCtx* bind_ctx, IMoniker* to_left, REFIID iid, void** obj);
  typedef HRESULT (STDMETHODCALLTYPE* IMoniker_BindToStorage_Fn)(IMoniker* me,
      IBindCtx* bind_ctx, IMoniker* to_left, REFIID iid, void** obj);

  static STDMETHODIMP BindToObject(IMoniker_BindToObject_Fn original,
                                   IMoniker* me, IBindCtx* bind_ctx,
                                   IMoniker* to_left, REFIID iid, void** obj);

  static STDMETHODIMP BindToStorage(IMoniker_BindToStorage_Fn original,
                                    IMoniker* me, IBindCtx* bind_ctx,
                                    IMoniker* to_left, REFIID iid, void** obj);

  // Reads content from cache (owned by RequestData) and simulates a regular
  // binding by calling the expected methods on the callback object bound to
  // the bind context.
  static HRESULT BindToStorageFromCache(IBindCtx* bind_ctx,
                                        const wchar_t* mime_type,
                                        RequestData* data,
                                        SimpleBindingImpl* binding,
                                        IStream** cache_out);
};

#endif  // CHROME_FRAME_URLMON_MONIKER_H_

