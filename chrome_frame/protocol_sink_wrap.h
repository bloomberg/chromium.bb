// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_PROTOCOL_SINK_WRAP_H_
#define CHROME_FRAME_PROTOCOL_SINK_WRAP_H_

#include <exdisp.h>
#include <urlmon.h>
#include <atlbase.h>
#include <atlcom.h>

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_comptr_win.h"
#include "base/scoped_bstr_win.h"
#include "googleurl/src/gurl.h"
#include "chrome_frame/chrome_frame_delegate.h"
#include "chrome_frame/http_negotiate.h"
#include "chrome_frame/ie8_types.h"
#include "chrome_frame/utils.h"
#include "chrome_frame/vtable_patch_manager.h"

// Typedefs for IInternetProtocol and related methods that we patch.
typedef HRESULT (STDMETHODCALLTYPE* InternetProtocol_Start_Fn)(
    IInternetProtocol* this_object, LPCWSTR url,
    IInternetProtocolSink* prot_sink, IInternetBindInfo* bind_info,
    DWORD flags, HANDLE_PTR reserved);
typedef HRESULT (STDMETHODCALLTYPE* InternetProtocol_Read_Fn)(
    IInternetProtocol* this_object, void* buffer, ULONG size,
    ULONG* size_read);
typedef HRESULT (STDMETHODCALLTYPE* InternetProtocol_StartEx_Fn)(
    IInternetProtocolEx* this_object, IUri* uri,
    IInternetProtocolSink* prot_sink, IInternetBindInfo* bind_info,
    DWORD flags, HANDLE_PTR reserved);
typedef HRESULT (STDMETHODCALLTYPE* InternetProtocol_LockRequest_Fn)(
    IInternetProtocol* this_object, DWORD options);
typedef HRESULT (STDMETHODCALLTYPE* InternetProtocol_UnlockRequest_Fn)(
    IInternetProtocol* this_object);


class ProtData;

// A class to wrap protocol sink in IInternetProtocol::Start[Ex] for
// HTTP and HTTPS protocols.
//
// This is an alternative to a mime filter and we have to do this in order
// to inspect initial portion of HTML for 'chrome' meta tag and report
// a different mime type in that case.
//
// We implement several documented interfaces
// supported by the original sink provided by urlmon. There are a few
// undocumented interfaces that we have chosen not to implement
// but delegate simply the QI.
class ProtocolSinkWrap
    : public CComObjectRootEx<CComMultiThreadModel>,
      public IServiceProvider,
      public UserAgentAddOn,  // implements IHttpNegotiate
      public IInternetProtocolSink {
 public:

BEGIN_COM_MAP(ProtocolSinkWrap)
  COM_INTERFACE_ENTRY(IServiceProvider)
  COM_INTERFACE_ENTRY(IInternetProtocolSink)
  COM_INTERFACE_BLIND_DELEGATE()
END_COM_MAP()

  static ScopedComPtr<IInternetProtocolSink> CreateNewSink(
      IInternetProtocolSink* sink, ProtData* prot_data);

  // Apparently this has to be public, to satisfy COM_INTERFACE_BLIND_DELEGATE
  IInternetProtocolSink* delegate() {
    return delegate_;
  }

 protected:
  ProtocolSinkWrap();
  ~ProtocolSinkWrap();

 private:
  // IInternetProtocolSink methods
  STDMETHOD(Switch)(PROTOCOLDATA* protocol_data);
  STDMETHOD(ReportProgress)(ULONG status_code, LPCWSTR status_text);
  STDMETHOD(ReportData)(DWORD flags, ULONG progress, ULONG max_progress);
  STDMETHOD(ReportResult)(HRESULT result, DWORD error, LPCWSTR result_text);

  // IServiceProvider - return our HttpNegotiate or forward to delegate
  STDMETHOD(QueryService)(REFGUID guidService, REFIID riid, void** ppvObject);

  // Helpers.
  HRESULT ObtainHttpNegotiate();
  HRESULT ObtainServiceProvider();

  // Remember original sink
  ScopedComPtr<IInternetProtocolSink> delegate_;
  ScopedComPtr<IServiceProvider> delegate_service_provider_;
  scoped_refptr<ProtData> prot_data_;
  DISALLOW_COPY_AND_ASSIGN(ProtocolSinkWrap);
};

class ProtData : public base::RefCounted<ProtData> {
 public:
  ProtData(IInternetProtocol* protocol, InternetProtocol_Read_Fn read_fun,
           const wchar_t* url);
  ~ProtData();
  HRESULT Read(void* buffer, ULONG size, ULONG* size_read);
  HRESULT ReportProgress(IInternetProtocolSink* delegate,
                         ULONG status_code,
                         LPCWSTR status_text);
  HRESULT ReportData(IInternetProtocolSink* delegate,
                     DWORD flags, ULONG progress, ULONG max_progress);
  HRESULT ReportResult(IInternetProtocolSink* delegate, HRESULT result,
                       DWORD error, LPCWSTR result_text);
  void UpdateUrl(const wchar_t* url);
  static scoped_refptr<ProtData> DataFromProtocol(IInternetProtocol* protocol);

  RendererType renderer_type() {
    return renderer_type_;
  }

  // Valid only if renderer_type_ is CHROME.
  const std::string& referrer() const {
    return referrer_;
  }

  bool is_attach_external_tab_request() const {
    return read_fun_ == NULL;
  }

  // Removes the mapping between the protocol and the ProtData.
  void Invalidate();

 private:
  typedef std::map<IInternetProtocol*, ProtData*> ProtocolDataMap;
  static ProtocolDataMap datamap_;
  static Lock datamap_lock_;

  // Url we are retrieving. Used for RendererTypeForUrl() only.
  std::wstring url_;
  // HTTP "Referrer" header if we detect are going to switch.
  // We have to save and pass it to Chrome, so scripts can read it via DOM.
  std::string referrer_;

  // Our gate to IInternetProtocol::Read()
  IInternetProtocol* protocol_;
  InternetProtocol_Read_Fn read_fun_;

  // What BINDSTATUS_MIMETYPEAVAILABLE and Co. tells us.
  ScopedBstr suggested_mime_type_;
  // At least one of the following has been received:
  // BINDSTATUS_MIMETYPEAVAILABLE,
  // MIMESTATUS_VERIFIEDMIMETYPEAVAILABLE
  // BINDSTATUS_SERVER_MIMETYPEAVAILABLE
  bool has_suggested_mime_type_;
  // BINDSTATUS_SERVER_MIMETYPEAVAILABLE received, so we shall fire one.
  bool has_server_mime_type_;

  RendererType renderer_type_;

  // Buffer for accumulated data including 1 extra for NULL-terminator
  static const size_t kMaxContentSniffLength = 2 * 1024;
  char buffer_[kMaxContentSniffLength + 1];
  unsigned long buffer_size_;  // NOLINT
  unsigned long buffer_pos_;  // NOLINT

  HRESULT FillBuffer();
  void SaveSuggestedMimeType(LPCWSTR status_text);
  void FireSuggestedMimeType(IInternetProtocolSink* delegate);
  void SaveReferrer(IInternetProtocolSink* delegate);
};

struct TransactionHooks {
  void InstallHooks();
  void RevertHooks();
};

DECLSPEC_SELECTANY struct TransactionHooks g_trans_hooks;

#endif  // CHROME_FRAME_PROTOCOL_SINK_WRAP_H_

