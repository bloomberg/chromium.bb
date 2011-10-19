// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_CHROME_FRAME_ACTIVEX_H_
#define CHROME_FRAME_CHROME_FRAME_ACTIVEX_H_

#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>

#include <set>
#include <string>
#include <vector>

#include "chrome_frame/chrome_frame_activex_base.h"
#include "chrome_frame/chrome_tab.h"
#include "chrome_frame/com_type_info_holder.h"
#include "grit/chrome_frame_resources.h"

#define WM_HOST_MOVED_NOTIFICATION (WM_APP + 1)

// ChromeFrameActivex: Implementation of the ActiveX control that is
// responsible for hosting a chrome frame, i.e. an iframe like widget which
// hosts the the chrome window. This object delegates to Chrome.exe
// (via the Chrome IPC-based automation mechanism) for the actual rendering.
class ATL_NO_VTABLE ChromeFrameActivex
    : public ChromeFrameActivexBase<ChromeFrameActivex, CLSID_ChromeFrame>,
      public IObjectSafetyImpl<ChromeFrameActivex,
                                 INTERFACESAFE_FOR_UNTRUSTED_CALLER |
                                 INTERFACESAFE_FOR_UNTRUSTED_DATA>,
      public IObjectWithSiteImpl<ChromeFrameActivex>,
      public IPersistPropertyBag {
 public:
  typedef ChromeFrameActivexBase<ChromeFrameActivex, CLSID_ChromeFrame> Base;
  ChromeFrameActivex();
  ~ChromeFrameActivex();

DECLARE_REGISTRY_RESOURCEID(IDR_CHROMEFRAME_ACTIVEX)

BEGIN_COM_MAP(ChromeFrameActivex)
  COM_INTERFACE_ENTRY(IObjectWithSite)
  COM_INTERFACE_ENTRY(IObjectSafety)
  COM_INTERFACE_ENTRY(IPersist)
  COM_INTERFACE_ENTRY(IPersistPropertyBag)
  COM_INTERFACE_ENTRY_CHAIN(Base)
END_COM_MAP()

BEGIN_MSG_MAP(ChromeFrameActivex)
  MESSAGE_HANDLER(WM_CREATE, OnCreate)
  MESSAGE_HANDLER(WM_HOST_MOVED_NOTIFICATION, OnHostMoved)
  CHAIN_MSG_MAP(Base)
END_MSG_MAP()

  HRESULT FinalConstruct();

  virtual HRESULT OnDraw(ATL_DRAWINFO& draw_info);  // NOLINT

  // IPersistPropertyBag implementation
  STDMETHOD(GetClassID)(CLSID* class_id) {
    if (class_id != NULL)
      *class_id = GetObjectCLSID();
    return S_OK;
  }

  STDMETHOD(InitNew)() {
    return S_OK;
  }

  STDMETHOD(Load)(IPropertyBag* bag, IErrorLog* error_log);

  STDMETHOD(Save)(IPropertyBag* bag, BOOL clear_dirty, BOOL save_all) {
    return E_NOTIMPL;
  }

  // Used to setup the document_url_ member needed for completing navigation.
  // Create external tab (possibly in incognito mode).
  HRESULT IOleObject_SetClientSite(IOleClientSite* client_site);

  // Overridden to perform security checks.
  STDMETHOD(put_src)(BSTR src);

  // IChromeFrame
  // On a fresh install of ChromeFrame the BHO will not be loaded in existing
  // IE tabs/windows. This function instantiates the BHO and registers it
  // explicitly.
  STDMETHOD(registerBhoIfNeeded)();

 protected:
  // ChromeFrameDelegate overrides
  virtual void OnLoad(const GURL& url);
  virtual void OnMessageFromChromeFrame(const std::string& message,
                                        const std::string& origin,
                                        const std::string& target);
  virtual void OnLoadFailed(int error_code, const std::string& url);
  virtual void OnAutomationServerLaunchFailed(
      AutomationLaunchResult reason, const std::string& server_version);
  virtual void OnChannelError();

  // Separated to static function for unit testing this logic more easily.
  static bool ShouldShowVersionMismatchDialog(bool is_privileged,
                                              IOleClientSite* client_site);

 private:
  LRESULT OnCreate(UINT message, WPARAM wparam, LPARAM lparam,
                   BOOL& handled);  // NO_LINT
  LRESULT OnHostMoved(UINT message, WPARAM wparam, LPARAM lparam,
                      BOOL& handled);  // NO_LINT

  HRESULT GetContainingDocument(IHTMLDocument2** doc);
  HRESULT GetDocumentWindow(IHTMLWindow2** window);

  // Gets the value of the 'id' attribute of the object element.
  HRESULT GetObjectScriptId(IHTMLObjectElement* object_elem, BSTR* id);

  // Returns the object element in the HTML page.
  // Note that if we're not being hosted inside an HTML
  // document, then this call will fail.
  HRESULT GetObjectElement(IHTMLObjectElement** element);

  HRESULT CreateScriptBlockForEvent(IHTMLElement2* insert_after,
                                    BSTR instance_id, BSTR script,
                                    BSTR event_name);

  // Utility function that checks the size of the vector and if > 0 creates
  // a variant for the string argument and forwards the call to the other
  // FireEvent method.
  void FireEvent(const EventHandlers& handlers, const std::string& arg);

  // Invokes all registered handlers in a vector of event handlers.
  void FireEvent(const EventHandlers& handlers, IDispatch* event);

  // This variant is used for the privatemessage handler only.
  void FireEvent(const EventHandlers& handlers, IDispatch* event,
                 BSTR target);

  // Installs a hook on the top-level window hosting the control.
  HRESULT InstallTopLevelHook(IOleClientSite* client_site);

  // A hook attached to the top-level window containing the ActiveX control.
  HHOOK chrome_wndproc_hook_;
};

#endif  // CHROME_FRAME_CHROME_FRAME_ACTIVEX_H_
