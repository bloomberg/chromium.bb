// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of ChromeProtocol
#include "chrome_frame/chrome_protocol.h"

#include "base/logging.h"
#include "chrome_frame/utils.h"

// ChromeProtocol

// Starts the class associated with the asynchronous pluggable protocol.
STDMETHODIMP ChromeProtocol::Start(LPCWSTR url,
                                   IInternetProtocolSink* prot_sink,
                                   IInternetBindInfo* bind_info,
                                   DWORD flags,
                                   DWORD reserved) {
  DVLOG(1) << __FUNCTION__ << ": URL = " << url;
  prot_sink->ReportProgress(BINDSTATUS_VERIFIEDMIMETYPEAVAILABLE,
                            kChromeMimeType);
  prot_sink->ReportData(
      BSCF_FIRSTDATANOTIFICATION | BSCF_LASTDATANOTIFICATION |
      BSCF_DATAFULLYAVAILABLE,
      0,
      0);
  return S_OK;
}

// Allows the pluggable protocol handler to continue processing data on the
// apartment (or user interface) thread. This method is called in response
// to a call to IInternetProtocolSink::Switch.
STDMETHODIMP ChromeProtocol::Continue(PROTOCOLDATA* protocol_data) {
  DVLOG(1) << __FUNCTION__;
  return S_OK;
}

// Aborts an operation in progress.
STDMETHODIMP ChromeProtocol::Abort(HRESULT reason, DWORD options) {
  DVLOG(1) << __FUNCTION__;
  return S_OK;
}

STDMETHODIMP ChromeProtocol::Terminate(DWORD options) {
  DVLOG(1) << __FUNCTION__;
  return S_OK;
}

STDMETHODIMP ChromeProtocol::Suspend() {
  return E_NOTIMPL;
}
STDMETHODIMP ChromeProtocol::Resume() {
  return E_NOTIMPL;
}

// Reads data retrieved by the pluggable protocol handler.
STDMETHODIMP ChromeProtocol::Read(void* buffer,
                                  ULONG buffer_size_in_bytes,
                                  ULONG* bytes_read) {
  DVLOG(1) << __FUNCTION__;
  return S_FALSE;
}

// Moves the current seek offset.
STDMETHODIMP ChromeProtocol::Seek(LARGE_INTEGER move_by,
                              DWORD origin,
                              ULARGE_INTEGER* new_position) {
  DVLOG(1) << __FUNCTION__;
  return E_NOTIMPL;
}

// Locks the request so that IInternetProtocolRoot::Terminate ()
// can be called and the remaining data can be read.
STDMETHODIMP ChromeProtocol::LockRequest(DWORD options) {
  DVLOG(1) << __FUNCTION__;
  return S_OK;
}

// Frees any resources associated with a lock. Called only if
// IInternetProtocol::LockRequest () was called.
STDMETHODIMP ChromeProtocol::UnlockRequest() {
  DVLOG(1) << __FUNCTION__;
  return S_OK;
}
