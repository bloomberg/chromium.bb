// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// @file
// ICeeeBroker implementation.

#ifndef CEEE_IE_BROKER_BROKER_H_
#define CEEE_IE_BROKER_BROKER_H_

#include <atlbase.h>
#include <atlcom.h>

#include "ceee/ie/broker/resource.h"

#include "broker_lib.h"  // NOLINT

class ExecutorsManager;
class ApiDispatcher;

// The CEEE user Broker is a COM object exposed from an executable server
// so that we can call it to execute code in destination threads of other
// processes, even if they run at a hight integrity level.

// Entry point to execute code in specific destination threads.
class ATL_NO_VTABLE CeeeBroker
    : public CComObjectRootEx<CComMultiThreadModel>,
      public CComCoClass<CeeeBroker, &CLSID_CeeeBroker>,
      public ICeeeBroker,
      public ICeeeBrokerRegistrar,
      public IExternalConnectionImpl<CeeeBroker> {
 public:
  DECLARE_REGISTRY_RESOURCEID(IDR_BROKER)

  DECLARE_NOT_AGGREGATABLE(CeeeBroker)
  BEGIN_COM_MAP(CeeeBroker)
    COM_INTERFACE_ENTRY(IExternalConnection)
    COM_INTERFACE_ENTRY(ICeeeBrokerRegistrar)
    COM_INTERFACE_ENTRY(ICeeeBroker)
  END_COM_MAP()
  DECLARE_PROTECT_FINAL_CONSTRUCT()

  // To set get our pointer to the ExecutorsManager and let tests override it.
  virtual HRESULT FinalConstruct();

  // @name ICeeeBroker implementation.
  // @{
  STDMETHOD(Execute)(BSTR function, BSTR* response);
  STDMETHOD(FireEvent)(BSTR event_name, BSTR event_args);
  // @}

  // @name ICeeeBrokerRegistrar implementation.
  // @{
  STDMETHOD(RegisterWindowExecutor)(long thread_id, IUnknown* executor);
  STDMETHOD(RegisterTabExecutor)(long thread_id, IUnknown* executor);
  STDMETHOD(UnregisterExecutor)(long thread_id);
  STDMETHOD(SetTabIdForHandle)(long tab_id, CeeeWindowHandle handle);
  // @}

  // IExternalConnectionImpl overrides
  void OnAddConnection(bool first_lock);
  void OnReleaseConnection(bool last_unlock, bool last_unlock_releases);

 protected:
  // A pointer to single instance objects, or seams set for unittests.
  ExecutorsManager * executors_manager_;
  ApiDispatcher* api_dispatcher_;
};

#endif  // CEEE_IE_BROKER_BROKER_H_
