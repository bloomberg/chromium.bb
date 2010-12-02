// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// IE toolband implementation.
#ifndef CEEE_IE_PLUGIN_TOOLBAND_TOOL_BAND_H_
#define CEEE_IE_PLUGIN_TOOLBAND_TOOL_BAND_H_

#include <atlbase.h>
#include <atlapp.h>  // Must be included AFTER base.
#include <atlcom.h>
#include <atlcrack.h>
#include <atlgdi.h>
#include <atlwin.h>
#include <atlmisc.h>
#include <exdispid.h>
#include <shobjidl.h>
#include <list>
#include <string>

#include "base/basictypes.h"
#include "base/scoped_comptr_win.h"
#include "base/scoped_ptr.h"
#include "base/win/rgs_helper.h"
#include "ceee/ie/plugin/toolband/resource.h"

#include "chrome_tab.h"  // NOLINT
#include "toolband.h"  // NOLINT

class DictionaryValue;
class PageApi;
class ToolBand;
class DictionaryValue;
class Value;

typedef IDispEventSimpleImpl<0, ToolBand, &DIID_DIChromeFrameEvents>
    ChromeFrameEvents;

typedef IDispEventSimpleImpl<1, ToolBand, &DIID_DWebBrowserEvents2>
    HostingBrowserEvents;

// Implements an IE toolband which gets instantiated for every IE browser tab
// and renders by hosting chrome frame as an ActiveX control.
class ATL_NO_VTABLE ToolBand : public CComObjectRootEx<CComSingleThreadModel>,
    public CComCoClass<ToolBand, &CLSID_ToolBand>,
    public IObjectWithSiteImpl<ToolBand>,
    public IServiceProviderImpl<ToolBand>,
    public IChromeFramePrivileged,
    public IDeskBand,
    public IPersistStream,
    public ChromeFrameEvents,
    public HostingBrowserEvents,
    public CWindowImpl<ToolBand> {
 public:
  ToolBand();
  ~ToolBand();

  DECLARE_REGISTRY_RESOURCEID_EX(IDR_TOOL_BAND)
  BEGIN_REGISTRY_MAP(ToolBand)
    REGMAP_UUID("CLSID", CLSID_ToolBand)
    REGMAP_RESOURCE("NAME", IDS_CEEE_NAME)
  END_REGISTRY_MAP()

  BEGIN_COM_MAP(ToolBand)
    COM_INTERFACE_ENTRY(IDeskBand)
    COM_INTERFACE_ENTRY(IDockingWindow)
    COM_INTERFACE_ENTRY(IOleWindow)
    COM_INTERFACE_ENTRY(IPersist)
    COM_INTERFACE_ENTRY(IPersistStream)
    COM_INTERFACE_ENTRY(IObjectWithSite)
    COM_INTERFACE_ENTRY(IServiceProvider)
    COM_INTERFACE_ENTRY(IChromeFramePrivileged)
  END_COM_MAP()

  BEGIN_SERVICE_MAP(ToolBand)
    SERVICE_ENTRY(SID_ChromeFramePrivileged)
    SERVICE_ENTRY_CHAIN(m_spUnkSite)
  END_SERVICE_MAP()


  BEGIN_SINK_MAP(ToolBand)
    SINK_ENTRY_INFO(0, DIID_DIChromeFrameEvents,
                    CF_EVENT_DISPID_ONREADYSTATECHANGED,
                    OnCfReadyStateChanged, &handler_type_long_)
    SINK_ENTRY_INFO(0, DIID_DIChromeFrameEvents,
                    CF_EVENT_DISPID_ONEXTENSIONREADY,
                    OnCfExtensionReady, &handler_type_bstr_i4_)
    SINK_ENTRY_INFO(0, DIID_DIChromeFrameEvents,
                    CF_EVENT_DISPID_ONGETENABLEDEXTENSIONSCOMPLETE,
                    OnCfGetEnabledExtensionsComplete, &handler_type_bstrarray_)
    SINK_ENTRY_INFO(0, DIID_DIChromeFrameEvents,
                    CF_EVENT_DISPID_ONMESSAGE,
                    OnCfMessage, &handler_type_idispatch_)
    SINK_ENTRY_INFO(0, DIID_DIChromeFrameEvents,
                    CF_EVENT_DISPID_ONLOAD,
                    OnCfOnload, &handler_type_idispatch_)
    SINK_ENTRY_INFO(0, DIID_DIChromeFrameEvents,
                    CF_EVENT_DISPID_ONLOADERROR,
                    OnCfOnloadError, &handler_type_idispatch_)
    SINK_ENTRY_INFO(1, DIID_DWebBrowserEvents2,
                    DISPID_NAVIGATECOMPLETE2,
                    OnIeNavigateComplete2, &handler_type_idispatch_variantref_)
  END_SINK_MAP()

  DECLARE_PROTECT_FINAL_CONSTRUCT()

  HRESULT FinalConstruct();
  void FinalRelease();

  BEGIN_MSG_MAP(ToolBand)
    MSG_WM_CREATE(OnCreate)
    MSG_WM_SIZE(OnSize)
  END_MSG_MAP()

  // @name IObjectWithSite overrides.
  STDMETHOD(SetSite)(IUnknown *site);

  // @name IDockingWindow implementation.
  // @{
  STDMETHOD(ShowDW)(BOOL show);
  STDMETHOD(CloseDW)(DWORD reserved);
  STDMETHOD(ResizeBorderDW)(LPCRECT border, IUnknown *toolband_site,
      BOOL reserved);
  // @}

  // @name IDeskBand implementation.
  STDMETHOD(GetBandInfo)(DWORD band_id, DWORD view_mode,
                         DESKBANDINFO *deskband_info);

  // @name IOleWindow implementation.
  // @{
  STDMETHOD(GetWindow)(HWND *window);
  STDMETHOD(ContextSensitiveHelp)(BOOL enter_mode);
  // @}

  // @name IPersist implementation.
  STDMETHOD(GetClassID)(CLSID *clsid);

  // @name IPersistStream implementation.
  // @{
  STDMETHOD(IsDirty)();
  STDMETHOD(Load)(IStream *stream);
  STDMETHOD(Save)(IStream *stream, BOOL clear_dirty);
  STDMETHOD(GetSizeMax)(ULARGE_INTEGER *size);
  // @}


  // @name IChromeFramePrivileged implementation.
  // @{
  STDMETHOD(GetWantsPrivileged)(boolean *wants_privileged);
  STDMETHOD(GetChromeProfileName)(BSTR *args);
  STDMETHOD(GetExtensionApisToAutomate)(BSTR *args);
  STDMETHOD(ShouldShowVersionMismatchDialog)();
  // @}


  // @name ChromeFrame event handlers
  // @{
  STDMETHOD_(void, OnCfReadyStateChanged)(LONG state);
  STDMETHOD_(void, OnCfExtensionReady)(BSTR path, int response);
  STDMETHOD_(void, OnCfGetEnabledExtensionsComplete)(
      SAFEARRAY* extension_directories);
  STDMETHOD_(void, OnCfMessage)(IDispatch* event);
  STDMETHOD_(void, OnCfOnload)(IDispatch* event);
  STDMETHOD_(void, OnCfOnloadError)(IDispatch* event);
  STDMETHOD_(void, OnIeNavigateComplete2)(IDispatch* dispatch, VARIANT* url);
  // @}

 protected:
  // Our window maintains a refcount on us for the duration of its lifetime.
  // The self-reference is managed with those two methods.
  virtual void OnFinalMessage(HWND window);
  LRESULT OnCreate(LPCREATESTRUCT lpCreateStruct);

  // Loads the manifest from file and retrieves the URL to the extension.
  // @returns true on success, false on failure to read the manifest or URL.
  bool LoadManifestFile(const std::wstring& base_dir,
                        std::string* toolband_url);

  // @name Message handlers.
  // @{
  void OnPaint(CDCHandle dc);
  void OnSize(UINT type, CSize size);
  // @}

  // Toolband requires a specific BHO to work. Normally, IE will create BHO
  // about the same time as the toolband. However, in some scenarios
  // (re-enabling the gadget) BHOs for the current pages won't be created,
  // even though the bar will.
  HRESULT EnsureBhoIsAvailable();

  // Gets the session ID of the Chrome Frame instance associated with the tool
  // band.
  virtual HRESULT GetSessionId(int* session_id);

  // Sends the tool band's Chrome Frame session ID to the BHO, given the BHO as
  // an IUnknown.
  virtual HRESULT SendSessionIdToBho(IUnknown* bho);

 private:
  // Initializes the toolband to the given site.
  // Called from SetSite.
  HRESULT Initialize(IUnknown *site);
  // Tears down an initialized toolband.
  // Called from SetSite.
  HRESULT Teardown();

  // Handles the dispatching of command received from the User/UI context.
  HRESULT DispatchUserCommand(const DictionaryValue& dict,
                              scoped_ptr<Value>* return_value);

  // Parses the manifest and navigates CF to the toolband URL.
  void StartExtension(const wchar_t* base_dir);

  // Subroutine of general initialization. Extracted to make testable.
  virtual HRESULT InitializeAndShowWindow(IUnknown* site);

  // The OwnLine flag indicates that the toolband should request from the IE
  // host to put it in its own space (and not behind whatever toolband might
  // have been installed first).
  bool ShouldForceOwnLine();
  void ClearForceOwnLineFlag();

  // A test seam for BHO instantiation test.
  virtual HRESULT CreateBhoInstance(IObjectWithSite** new_bho_instance);

  // The web browser that initialized this toolband.
  ScopedComPtr<IWebBrowser2> web_browser_;
  // Our parent window, yielded by our site's IOleWindow.
  CWindow parent_window_;
  // Our band id, provided by GetBandInfo.
  DWORD band_id_;

  // The minimum size the toolband should take.
  LONG current_width_;
  LONG current_height_;

  // The URL to our extension.
  std::string extension_url_;

  // Our Chrome frame instance and its window.
  CComPtr<IChromeFrame> chrome_frame_;
  CWindow chrome_frame_window_;

  // Indicates whether CloseDW() is being called on this tool band.
  bool is_quitting_;

  // True if we noticed that no extensions are enabled and requested
  // to install one.
  bool already_tried_installing_;

  // Flag purpose: see comments to ShouldForceOwnLine
  // for efficiency we read only once (thus the second flag).
  bool own_line_flag_;
  bool already_checked_own_line_flag_;

  // True if we have sent the tool band ID to the BHO.
  bool already_sent_id_to_bho_;

  // Listening to DIID_DWebBrowserEvents2 is optional. For registration
  // purposes we have to know if we are listening, though.
  bool listening_to_browser_events_;

  // Filesystem path to the .crx we will install, or the empty string, or
  // (if not ending in .crx) the path to an exploded extension directory to
  // load.
  std::wstring extension_path_;

  // Function info objects describing our message handlers.
  // Effectively const but can't make const because of silly ATL macro problem.
  static _ATL_FUNC_INFO handler_type_idispatch_;
  static _ATL_FUNC_INFO handler_type_long_;
  static _ATL_FUNC_INFO handler_type_idispatch_bstr_;
  static _ATL_FUNC_INFO handler_type_bstr_i4_;
  static _ATL_FUNC_INFO handler_type_bstrarray_;
  static _ATL_FUNC_INFO handler_type_idispatch_variantref_;

  DISALLOW_COPY_AND_ASSIGN(ToolBand);
};

#endif  // CEEE_IE_PLUGIN_TOOLBAND_TOOL_BAND_H_
