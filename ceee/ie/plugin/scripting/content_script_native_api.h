// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// @file
// Content script native API class declaration.

#ifndef CEEE_IE_PLUGIN_SCRIPTING_CONTENT_SCRIPT_NATIVE_API_H_
#define CEEE_IE_PLUGIN_SCRIPTING_CONTENT_SCRIPT_NATIVE_API_H_

#include <map>
#include <string>
#include <vector>

#include "ceee/common/initializing_coclass.h"
#include "toolband.h"  // NOLINT

// Fwd.
class IContentScriptNativeApi;

class IExtensionPortMessagingProvider: public IUnknown {
 public:
  // Close all ports opening and opened for this instance.
  virtual void CloseAll(IContentScriptNativeApi* instance) = 0;

  // Initiates opening a channel to an extension.
  // @param instance the PageApi instance requesting the channel.
  // @param extension the id of the extension to open the channel to.
  // @param cookie a caller-provided cookie associated with this request.
  // @note in the fullness of time, the manager should call
  //        ChannelOpened, supplying the callback and the assigned port_id.
  virtual HRESULT OpenChannelToExtension(IContentScriptNativeApi* instance,
                                         const std::string& extension,
                                         const std::string& channel_name,
                                         int cookie) = 0;

  // Posts a message from the page to the previously assigned channel
  // corresponding to port_id.
  virtual HRESULT PostMessage(int port_id, const std::string& message) = 0;
};

class IContentScriptNativeApi: public IUnknown {
 public:
  // Called by host to complete a channel open request.
  // @param cookie the cookie previously passed to the host on an
  //        OpenChannelToExtension invocation.
  // @param port_id the port ID assigned to the port by the host.
  virtual void OnChannelOpened(int cookie, int port_id) = 0;

  // Called by host on an incoming postMessage.
  // @param port_id the host port ID of the destination port.
  // @param message the message.
  virtual void OnPostMessage(int port_id, const std::string& message) = 0;
};

// This class implements the native API provided to content scripts through
// the ceee_bootstrap.js script. The functionality provided here has to be
// safe for any page content to invoke.
// For safety's sake, do not expose any IDispatch-derived interfaces on this
// object other than ICeeeContentScriptNativeApi.
class ContentScriptNativeApi
    : public CComObjectRootEx<CComSingleThreadModel>,
      public InitializingCoClass<ContentScriptNativeApi>,
      public IContentScriptNativeApi,
      public IDispatchImpl<ICeeeContentScriptNativeApi,
                           &IID_ICeeeContentScriptNativeApi,
                           &LIBID_ToolbandLib,
                           0xFFFF,    // Magic ATL incantation to load
                           0xFFFF> {  // typelib from our resource.
 public:
  BEGIN_COM_MAP(ContentScriptNativeApi)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(ICeeeContentScriptNativeApi)
  END_COM_MAP()

  ContentScriptNativeApi();

  HRESULT Initialize(IExtensionPortMessagingProvider* messaging_provider);
  void FinalRelease();

  // @name ICeeeContentScriptNativeApi implementation
  // This is the interface presented to the JavaScript
  // code in ceee_bootstrap.js.
  // @{
  STDMETHOD(Log)(BSTR level, BSTR message);
  STDMETHOD(OpenChannelToExtension)(BSTR source_id,
                                    BSTR target_id,
                                    BSTR name,
                                    long* port_id);
  STDMETHOD(CloseChannel)(long port_id);
  STDMETHOD(PortAddRef)(long port_id);
  STDMETHOD(PortRelease)(long port_id);
  STDMETHOD(PostMessage)(long port_id, BSTR msg);
  STDMETHOD(AttachEvent)(BSTR event_name);
  STDMETHOD(DetachEvent)(BSTR event_name);
  STDMETHOD(put_onLoad)(IDispatch* callback);
  STDMETHOD(put_onUnload)(IDispatch* callback);
  STDMETHOD(put_onPortConnect)(IDispatch* callback);
  STDMETHOD(put_onPortDisconnect)(IDispatch* callback);
  STDMETHOD(put_onPortMessage)(IDispatch* callback);
  // @}


  // @name IContentScriptNativeApi implementation
  // @{
  virtual void OnChannelOpened(int cookie, int port_id);
  virtual void OnPostMessage(int port_id, const std::string& message);
  // @}

  // Typed wrapper functions to call on the respective callbacks.
  // @{
  HRESULT CallOnLoad(const wchar_t* extension_id);
  HRESULT CallOnUnload();
  HRESULT CallOnPortConnect(long port_id,
                            const wchar_t* channel_name,
                            const wchar_t* tab,
                            const wchar_t* source_extension_id,
                            const wchar_t* target_extension_id);
  HRESULT CallOnPortDisconnect(long port_id);
  HRESULT CallOnPortMessage(const wchar_t* msg, long port_id);
  // @}

  // Release all resources.
  HRESULT TearDown();

 private:
  // Storage for our callback properties.
  CComDispatchDriver on_load_;
  CComDispatchDriver on_unload_;
  CComDispatchDriver on_port_connect_;
  CComDispatchDriver on_port_disconnect_;
  CComDispatchDriver on_port_message_;

  // The messaging provider takes care of communication transport for us.
  CComPtr<IExtensionPortMessagingProvider> messaging_provider_;

  typedef int PortId;
  typedef PortId LocalPortId;
  typedef PortId RemotePortId;
  static const int kInvalidPortId = -1;
  static const int kFirstPortId = 2;

  enum LocalPortState {
    PORT_UNINITIALIZED,
    PORT_CONNECTING,
    PORT_CONNECTED,
    PORT_CLOSING,
    PORT_CLOSED,
  };

  typedef std::vector<std::string> MessageList;

  // State we maintain per port.
  class LocalPort {
   public:
    explicit LocalPort(LocalPortId id);

    LocalPortState state;
    LocalPortId local_id;
    RemotePortId remote_id;

    // Messages waiting to be posted.
    MessageList pending_messages;
  };

  LocalPortId GetNextLocalPortId() {
    LocalPortId id = next_local_port_id_;
    next_local_port_id_ += 2;
    return id;
  }

  typedef std::map<LocalPortId, LocalPort> LocalPortMap;
  typedef std::map<RemotePortId, LocalPortId> RemoteToLocalPortIdMap;

  // Local state for the ports we handle.
  LocalPortMap local_ports_;
  // Maps
  RemoteToLocalPortIdMap remote_to_local_port_id_;

  LocalPortId next_local_port_id_;
};

#endif  // CEEE_IE_PLUGIN_SCRIPTING_CONTENT_SCRIPT_NATIVE_API_H_
