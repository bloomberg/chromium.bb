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
#include "googleurl/src/gurl.h"
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
typedef HRESULT (STDMETHODCALLTYPE* InternetProtocolRoot_Continue_Fn)(
    IInternetProtocolRoot* me, PROTOCOLDATA* data);

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
      public IInternetProtocolSink,
      public IInternetBindInfoEx,
      public IServiceProvider,
      public IAuthenticate,
      public IInternetProtocolEx,
      public IInternetPriority,
      public IWrappedProtocol,
      // public IPreBindingSupport, // undocumented
      // public ITransProtocolSink, // Undocumented
      // public ITransactionInternal, // undocumented
      public IUriContainer {
 public:

BEGIN_COM_MAP(ProtocolSinkWrap)
  COM_INTERFACE_ENTRY(IInternetProtocolSink)
  COM_INTERFACE_ENTRY(IInternetBindInfo)
  COM_INTERFACE_ENTRY(IInternetBindInfoEx)
  COM_INTERFACE_ENTRY(IServiceProvider)
  COM_INTERFACE_ENTRY(IAuthenticate)
  COM_INTERFACE_ENTRY(IInternetProtocolRoot)
  COM_INTERFACE_ENTRY(IInternetProtocol)
  COM_INTERFACE_ENTRY(IInternetProtocolEx)
  COM_INTERFACE_ENTRY(IInternetPriority)
  COM_INTERFACE_ENTRY(IWrappedProtocol)
  COM_INTERFACE_ENTRY_IF_DELEGATE_SUPPORTS(IUriContainer)
  COM_INTERFACE_BLIND_DELEGATE()
END_COM_MAP()

  ProtocolSinkWrap();
  virtual ~ProtocolSinkWrap();

  bool Initialize(IInternetProtocol* protocol,
      IInternetProtocolSink* original_sink, const wchar_t* url);

  // VTable patches the IInternetProtocol and IIntenetProtocolEx interface.
  // Returns true on success.
  static bool PatchProtocolHandlers();

  // Unpatches the IInternetProtocol and IInternetProtocolEx interfaces.
  static void UnpatchProtocolHandlers();

  // IInternetProtocol/Ex patches.
  static STDMETHODIMP OnStart(InternetProtocol_Start_Fn orig_start,
      IInternetProtocol* protocol, LPCWSTR url,
      IInternetProtocolSink* prot_sink, IInternetBindInfo* bind_info,
      DWORD flags, HANDLE_PTR reserved);

  static STDMETHODIMP OnStartEx(
      InternetProtocol_StartEx_Fn orig_start_ex, IInternetProtocolEx* protocol,
      IUri* uri, IInternetProtocolSink* prot_sink,
      IInternetBindInfo* bind_info, DWORD flags, HANDLE_PTR reserved);

  static STDMETHODIMP OnRead(InternetProtocol_Read_Fn orig_read,
      IInternetProtocol* protocol, void* buffer, ULONG size, ULONG* size_read);

  // IInternetProtocolSink methods
  STDMETHOD(Switch)(PROTOCOLDATA* protocol_data);
  STDMETHOD(ReportProgress)(ULONG status_code, LPCWSTR status_text);
  STDMETHOD(ReportData)(DWORD flags, ULONG progress, ULONG max_progress);
  STDMETHOD(ReportResult)(HRESULT result, DWORD error, LPCWSTR result_text);

  // IInternetBindInfoEx
  STDMETHOD(GetBindInfo)(DWORD* flags, BINDINFO* bind_info);
  STDMETHOD(GetBindString)(ULONG string_type, LPOLESTR* string_array,
      ULONG array_size, ULONG* size_returned);
  STDMETHOD(GetBindInfoEx)(DWORD *flags, BINDINFO* bind_info,
      DWORD* bindf2, DWORD *reserved);

  // IServiceProvider
  STDMETHOD(QueryService)(REFGUID service_guid, REFIID riid, void** service);

  // IAuthenticate
  STDMETHOD(Authenticate)(HWND* window, LPWSTR* user_name, LPWSTR* password);

  // IInternetProtocolEx
  STDMETHOD(Start)(LPCWSTR url, IInternetProtocolSink *protocol_sink,
      IInternetBindInfo* bind_info, DWORD flags, HANDLE_PTR reserved);
  STDMETHOD(Continue)(PROTOCOLDATA* protocol_data);
  STDMETHOD(Abort)(HRESULT reason, DWORD options);
  STDMETHOD(Terminate)(DWORD options);
  STDMETHOD(Suspend)();
  STDMETHOD(Resume)();
  STDMETHOD(Read)(void *buffer, ULONG size, ULONG* size_read);
  STDMETHOD(Seek)(LARGE_INTEGER move, DWORD origin, ULARGE_INTEGER* new_pos);
  STDMETHOD(LockRequest)(DWORD options);
  STDMETHOD(UnlockRequest)();
  STDMETHOD(StartEx)(IUri* uri, IInternetProtocolSink* protocol_sink,
      IInternetBindInfo* bind_info, DWORD flags, HANDLE_PTR reserved);

  // IInternetPriority
  STDMETHOD(SetPriority)(LONG priority);
  STDMETHOD(GetPriority)(LONG* priority);

  // IWrappedProtocol
  STDMETHOD(GetWrapperCode)(LONG *code, DWORD_PTR reserved);

  // public IUriContainer
  STDMETHOD(GetIUri)(IUri** uri);

  // IPreBindingSupport, // undocumented
  // ITransProtocolSink, // Undocumented
  // ITransactionInternal, // undocumented

  IInternetProtocolSink* delegate() const {
    return delegate_;
  }

 protected:
  enum RendererType {
    UNDETERMINED,
    CHROME,
    OTHER
  };

  typedef std::map<IInternetProtocol*, ProtocolSinkWrap*> ProtocolSinkMap;
  static const int kMaxContentSniffLength = 1024;

  static scoped_refptr<ProtocolSinkWrap> InstanceFromProtocol(
      IInternetProtocol* protocol);
  static ScopedComPtr<IInternetProtocolSink> MaybeWrapSink(
      IInternetProtocol* protocol, IInternetProtocolSink* prot_sink,
      const wchar_t* url);

  void DetermineRendererType();
  HRESULT OnReadImpl(void* buffer, ULONG size, ULONG* size_read,
      InternetProtocol_Read_Fn orig_read);

  bool is_undetermined() const {
    return (UNDETERMINED == renderer_type_);
  }
  RendererType renderer_type() const {
    return renderer_type_;
  }

  // Creates an instance of the specified protocol handler and returns the
  // IInternetProtocol interface pointer.
  // Returns S_OK on success.
  static HRESULT CreateProtocolHandlerInstance(const CLSID& clsid,
                                               IInternetProtocol** protocol);

  // Helper function for patching the VTable of the IInternetProtocol
  // interface. It instantiates the object identified by the protocol_clsid
  // parameter and patches its VTable.
  // Returns S_OK on success.
  static HRESULT PatchProtocolMethods(
      const CLSID& protocol_clsid,
      vtable_patch::MethodPatchInfo* protocol_patch_info,
      vtable_patch::MethodPatchInfo* protocol_ex_patch_info);

  // WARNING: Don't use GURL variables here.  Please see
  // http://b/issue?id=2102171 for details.

  // Remember original sink
  ScopedComPtr<IInternetProtocolSink> delegate_;

  // Cannot take a reference on the protocol.
  IInternetProtocol* protocol_;
  RendererType renderer_type_;

  // Buffer for accumulated data including 1 extra for NULL-terminator
  char buffer_[kMaxContentSniffLength + 1];
  unsigned long buffer_size_;  // NOLINT
  unsigned long buffer_pos_;  // NOLINT

  // Accumulated result
  bool is_saved_result_;
  HRESULT result_code_;
  DWORD result_error_;
  std::wstring result_text_;
  // For tracking re-entrency and preventing duplicate Read()s from
  // distorting the outcome of ReportData.
  int report_data_recursiveness_;

  static ProtocolSinkMap sink_map_;
  // TODO(joshia): Replace with Lock
  static CComAutoCriticalSection sink_map_lock_;

  std::wstring url_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProtocolSinkWrap);
};

#endif  // CHROME_FRAME_PROTOCOL_SINK_WRAP_H_
