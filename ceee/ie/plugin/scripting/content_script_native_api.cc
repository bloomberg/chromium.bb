// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// @file
// Content script native API implementation.
#include "ceee/ie/plugin/scripting/content_script_native_api.h"
#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "ceee/common/com_utils.h"

namespace {

// DISPID_VALUE is a macro defined to 0, which confuses overloaded Invoke-en.
DISPID kDispidValue = DISPID_VALUE;

}  // namespace

ContentScriptNativeApi::ContentScriptNativeApi()
    : next_local_port_id_(kFirstPortId) {
}

ContentScriptNativeApi::LocalPort::LocalPort(LocalPortId id)
    : state(PORT_UNINITIALIZED), local_id(id), remote_id(kInvalidPortId) {
}

HRESULT ContentScriptNativeApi::Initialize(
    IExtensionPortMessagingProvider *messaging_provider) {
  DCHECK(messaging_provider != NULL);
  messaging_provider_ = messaging_provider;
  return S_OK;
}

void ContentScriptNativeApi::FinalRelease() {
  DCHECK(on_load_ == NULL);
  DCHECK(on_unload_ == NULL);
  DCHECK(on_port_connect_ == NULL);
  DCHECK(on_port_disconnect_ == NULL);
  DCHECK(on_port_message_ == NULL);
}

HRESULT ContentScriptNativeApi::TearDown() {
  on_load_.Release();
  on_unload_.Release();
  on_port_connect_.Release();
  on_port_disconnect_.Release();
  on_port_message_.Release();

  // Make sure all our open and in-progress ports are closed.
  if (messaging_provider_ != NULL)
    messaging_provider_->CloseAll(this);
  messaging_provider_.Release();

  return S_OK;
}

STDMETHODIMP ContentScriptNativeApi::Log(BSTR level, BSTR message) {
  const wchar_t* str_level = com::ToString(level);
  size_t len = ::SysStringLen(level);
  if (LowerCaseEqualsASCII(str_level, str_level + len, "info")) {
    LOG(INFO) << com::ToString(message);
  } else if (LowerCaseEqualsASCII(str_level, str_level + len, "error")) {
    LOG(ERROR) << com::ToString(message);
  } else {
    LOG(WARNING) << com::ToString(message);
  }

  return S_OK;
}


STDMETHODIMP ContentScriptNativeApi::OpenChannelToExtension(BSTR source_id,
                                                            BSTR target_id,
                                                            BSTR name,
                                                            long* port_id) {
  // TODO(siggi@chromium.org): handle connecting to other extensions.
  // TODO(siggi@chromium.org): check for the correct source_id here.
  DCHECK(messaging_provider_ != NULL);

  if (0 != wcscmp(com::ToString(source_id), com::ToString(target_id)))
    return E_UNEXPECTED;

  LocalPortId id = GetNextLocalPortId();
  std::pair<LocalPortMap::iterator, bool> inserted =
        local_ports_.insert(std::make_pair(id, LocalPort(id)));
  DCHECK(inserted.second && inserted.first != local_ports_.end());
  // Get the port we just inserted.
  LocalPort& port = inserted.first->second;
  DCHECK_EQ(id, port.local_id);

  std::string extension_id;
  bool converted = WideToUTF8(com::ToString(source_id),
                   ::SysStringLen(source_id),
                   &extension_id);
  DCHECK(converted);
  std::string channel_name;
  converted = WideToUTF8(com::ToString(name),
                         ::SysStringLen(name),
                         &channel_name);
  DCHECK(converted);

  // Send off the connection request with our local port ID as cookie.
  HRESULT hr = messaging_provider_->OpenChannelToExtension(this,
                                                           extension_id,
                                                           channel_name,
                                                           port.local_id);
  DCHECK(SUCCEEDED(hr));

  port.state = PORT_CONNECTING;

  // TODO(siggi@chromium.org): Clean up on failure.

  *port_id = id;

  return S_OK;
}

STDMETHODIMP ContentScriptNativeApi::CloseChannel(long port_id) {
  // TODO(siggi@chromium.org): Writeme.
  LOG(INFO) << "CloseChannel(" << port_id << ")";
  return S_OK;
}

STDMETHODIMP ContentScriptNativeApi::PortAddRef(long port_id) {
  // TODO(siggi@chromium.org): Writeme.
  LOG(INFO) << "PortAddRef(" << port_id << ")";
  return S_OK;
}

STDMETHODIMP ContentScriptNativeApi::PortRelease(long port_id) {
  // TODO(siggi@chromium.org): Writeme.
  LOG(INFO) << "PortRelease(" << port_id << ")";
  return S_OK;
}

STDMETHODIMP ContentScriptNativeApi::PostMessage(long port_id, BSTR msg) {
    LocalPortMap::iterator it(local_ports_.find(port_id));
  DCHECK(messaging_provider_ != NULL);

  // TODO(siggi@chromium.org): should I expect to get messages to
  // defunct port ids?
  DCHECK(it != local_ports_.end());
  if (it == local_ports_.end())
    return E_UNEXPECTED;
  LocalPort& port = it->second;

  std::string msg_str(WideToUTF8(com::ToString(msg)));
  if (port.state == PORT_CONNECTED) {
    messaging_provider_->PostMessage(port.remote_id, msg_str);
  } else if (port.state == PORT_CONNECTING) {
    port.pending_messages.push_back(msg_str);
  } else {
    LOG(ERROR) << "Unexpected PostMessage for port in state " << port.state;
  }

  return S_OK;
}

STDMETHODIMP ContentScriptNativeApi::AttachEvent(BSTR event_name) {
  // TODO(siggi@chromium.org): Writeme.
  LOG(INFO) << "AttachEvent(" << com::ToString(event_name) << ")";
  return S_OK;
}

STDMETHODIMP ContentScriptNativeApi::DetachEvent(BSTR event_name) {
  // TODO(siggi@chromium.org): Writeme.
  LOG(INFO) << "DetachEvent(" << com::ToString(event_name) << ")";
  return S_OK;
}

STDMETHODIMP ContentScriptNativeApi::put_onLoad(IDispatch* callback) {
  on_load_ = callback;
  return S_OK;
}

STDMETHODIMP ContentScriptNativeApi::put_onUnload(IDispatch* callback) {
  on_unload_ = callback;
  return S_OK;
}

STDMETHODIMP ContentScriptNativeApi::put_onPortConnect(IDispatch* callback) {
  on_port_connect_ = callback;
  return S_OK;
}

STDMETHODIMP ContentScriptNativeApi::put_onPortDisconnect(IDispatch* callback) {
  on_port_disconnect_ = callback;
  return S_OK;
}

STDMETHODIMP ContentScriptNativeApi::put_onPortMessage(IDispatch* callback) {
  on_port_message_ = callback;
  return S_OK;
}

void ContentScriptNativeApi::OnChannelOpened(int cookie,
                                             int port_id) {
  // We've observed post-teardown notifications.
  // TODO(siggi): These shouldn't occur, figure out why this is happening.
  if (messaging_provider_ == NULL) {
    LOG(ERROR) << "Received OnChannelOpened after teardown";
    return;
  }

  // The cookie is our local port ID.
  LocalPortId local_id = cookie;
  LocalPortMap::iterator it(local_ports_.find(local_id));
  DCHECK(it != local_ports_.end());

  LocalPort& port = it->second;
  DCHECK_EQ(local_id, port.local_id);
  port.remote_id = port_id;
  port.state = PORT_CONNECTED;

  // Remember the mapping so that we can find this port by remote_id.
  remote_to_local_port_id_[port.remote_id] = port.local_id;

  // Flush pending messages on this port.
  if (!port.pending_messages.empty()) {
    MessageList::iterator it(port.pending_messages.begin());
    MessageList::iterator end(port.pending_messages.end());

    for (; it != end; ++it) {
      messaging_provider_->PostMessage(port.remote_id, *it);
    }
  }
}

void ContentScriptNativeApi::OnPostMessage(int port_id,
                                           const std::string& message) {
  // Translate the remote port id to a local port id.
  RemoteToLocalPortIdMap::iterator it(remote_to_local_port_id_.find(port_id));
  DCHECK(it != remote_to_local_port_id_.end());

  LocalPortId local_id = it->second;

  // And push the message to the script.
  std::wstring message_wide(UTF8ToWide(message));
  CallOnPortMessage(message_wide.c_str(), local_id);
}

HRESULT ContentScriptNativeApi::CallOnLoad(const wchar_t* extension_id) {
  if (on_load_ == NULL)
    return E_UNEXPECTED;

  return on_load_.Invoke1(kDispidValue, &CComVariant(extension_id));
}

HRESULT ContentScriptNativeApi::CallOnUnload() {
  if (on_unload_ == NULL)
    return E_UNEXPECTED;

  return on_unload_.Invoke0(kDispidValue);
}

HRESULT ContentScriptNativeApi::CallOnPortConnect(
    long port_id, const wchar_t* channel_name, const wchar_t* tab,
    const wchar_t* source_extension_id, const wchar_t* target_extension_id) {
  if (on_port_connect_ == NULL)
    return E_UNEXPECTED;

  // Note args go in reverse order of declaration for Invoke.
  CComVariant args[] = {
      target_extension_id,
      source_extension_id,
      tab,
      channel_name,
      port_id};

  return on_port_connect_.InvokeN(kDispidValue, args, arraysize(args));
}

HRESULT ContentScriptNativeApi::CallOnPortDisconnect(long port_id) {
  if (on_port_disconnect_ == NULL)
    return E_UNEXPECTED;

  return on_port_disconnect_.Invoke1(kDispidValue, &CComVariant(port_id));
}

HRESULT ContentScriptNativeApi::CallOnPortMessage(const wchar_t* msg,
                                                  long port_id) {
  if (on_port_message_ == NULL)
    return E_UNEXPECTED;

  // Note args go in reverse order of declaration for Invoke.
  CComVariant args[] = { port_id, msg };

  return on_port_message_.InvokeN(kDispidValue, args, arraysize(args));
}
