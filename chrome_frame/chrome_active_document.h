// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_CHROME_ACTIVE_DOCUMENT_H_
#define CHROME_FRAME_CHROME_ACTIVE_DOCUMENT_H_

#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>
#include <map>
#include <mshtmcid.h>
#include <perhist.h>

#include "base/scoped_ptr.h"
#include "base/scoped_comptr_win.h"
#include "base/thread.h"

#include "chrome_frame/chrome_frame_activex_base.h"
#include "chrome_frame/com_type_info_holder.h"
#include "chrome_frame/find_dialog.h"
#include "chrome_frame/html_private_window_impl.h"
#include "chrome_frame/html_window_impl.h"
#include "chrome_frame/in_place_menu.h"
#include "chrome_frame/ole_document_impl.h"
#include "chrome_frame/resource.h"
#include "chrome_frame/extra_system_apis.h"

class Thread;
class TabProxy;
class ChromeActiveDocument;

// A call to IOleCommandTarget::Exec on the webbrowser with this command id
// and a command group of CGID_EXPLORER causes IE to finalize the current
// travel log entry and move to a new location (pruning any forward entries
// if needed)
#define INTERNAL_CMDID_FINALIZE_TRAVEL_LOG (38)

// To set the lock icon status call IOleCommandTarget::Exec on site with
// this command id and a command group of CGID_EXPLORER  The in arg is one of
// the values: SECURELOCK_SET_UNSECURE, SECURELOCK_SET_MIXED,
// SECURELOCK_SET_SECURE128BIT etc declared in shdeprecated.h
#define INTERNAL_CMDID_SET_SSL_LOCK (37)

// A call to IOleCommandTarget::Exec on the webbrowser with this command id
// and a command group of CGID_EXPLORER causes IE to replace the URL in the
// current travel log entry
#define INTERNAL_CMDID_REPLACE_CURRENT_TRAVEL_LOG_ENTRY_URL (40)

#define INTERNAL_IE_CONTEXTMENU_VIEWSOURCE    (2139)

#ifndef SBCMDID_MIXEDZONE
// This command is sent by the frame to allow the document to return the URL
// security zone for display.
// Please refer to http://msdn.microsoft.com/en-us/library/aa770042(VS.85).aspx
// for more information.
#define SBCMDID_MIXEDZONE                   39
#endif  // SBCMDID_MIXEDZONE

// From MSDN:
// Controlling Navigation: The fact that a document can navigate on its own
// implies that it will also take care of updating the navigation history.
// In Internet Explorer 6 and later, the DocObject can indicate to the client
// site that it can navigate using CGID_DocHostCmdPriv (a privately defined
// command group GUID) and the DOCHOST_DOCCANNAVIGATE command. A pointer to
// the object that implements the IHTMLWindow2 interface is passed with the
// command in the VARIANTARG* parameter pvaIn. (Set pvaIn to NULL if the
// document cannot perform its own navigation.)
#define DOCHOST_DOCCANNAVIGATE                      (0)

// This macro should be defined in the public section of the class.
#define BEGIN_EXEC_COMMAND_MAP(theClass) \
  public: \
   HRESULT ProcessExecCommand(const GUID* cmd_group_guid, DWORD command_id, \
                              DWORD cmd_exec_opt, VARIANT* in_args, \
                              VARIANT* out_args) { \
   HRESULT hr = OLECMDERR_E_NOTSUPPORTED; \
   switch (command_id) {


#define EXEC_COMMAND_HANDLER(id, handler) \
  case id: { \
    hr = S_OK;  \
    handler(cmd_group_guid, command_id, cmd_exec_opt, in_args, out_args);  \
    break;  \
  }

#define EXEC_COMMAND_HANDLER_NO_ARGS(id, handler) \
  case id: { \
    hr = S_OK;  \
    handler();  \
    break;  \
  }

#define EXEC_COMMAND_HANDLER_GENERIC(id, code) \
  case id: { \
    hr = S_OK;  \
    code;  \
    break;  \
  }

#define END_EXEC_COMMAND_MAP()  \
    default: \
      break; \
  } \
  return hr; \
}

// ChromeActiveDocument: Implementation of the Active Document object that is
// responsible for rendering pages in Chrome. This object delegates to
// Chrome.exe (via the Chrome IPC-based automation mechanism) for the actual
// rendering
class ATL_NO_VTABLE ChromeActiveDocument
    : public ChromeFrameActivexBase<ChromeActiveDocument,
                                    CLSID_ChromeActiveDocument>,
      public IOleDocumentImpl<ChromeActiveDocument>,
      public IOleDocumentViewImpl<ChromeActiveDocument>,
      public IPersistMoniker,
      public IOleCommandTarget,
      public InPlaceMenu<ChromeActiveDocument>,
      public IWebBrowserEventsUrlService,
      public IPersistHistory,
      public HTMLWindowImpl<IHTMLWindow2>,
      public HTMLPrivateWindowImpl<IHTMLPrivateWindow> {
 public:
  typedef ChromeFrameActivexBase<ChromeActiveDocument,
      CLSID_ChromeActiveDocument> Base;
  ChromeActiveDocument();
  ~ChromeActiveDocument();

  DECLARE_REGISTRY_RESOURCEID(IDR_CHROMEACTIVEDOCUMENT)

BEGIN_COM_MAP(ChromeActiveDocument)
  COM_INTERFACE_ENTRY(IOleDocument)
  COM_INTERFACE_ENTRY(IOleDocumentView)
  COM_INTERFACE_ENTRY(IPersistMoniker)
  COM_INTERFACE_ENTRY(IOleCommandTarget)
  COM_INTERFACE_ENTRY(IWebBrowserEventsUrlService)
  COM_INTERFACE_ENTRY(IPersist)
  COM_INTERFACE_ENTRY(IPersistHistory)
  COM_INTERFACE_ENTRY(IHTMLFramesCollection2)
  COM_INTERFACE_ENTRY(IHTMLWindow2)
  COM_INTERFACE_ENTRY(IHTMLPrivateWindow)
  COM_INTERFACE_ENTRY_CHAIN(Base)
END_COM_MAP()

BEGIN_MSG_MAP(ChromeActiveDocument)
  CHAIN_MSG_MAP(Base)
END_MSG_MAP()

  HRESULT FinalConstruct();

#define FORWARD_TAB_COMMAND(id, command) \
  EXEC_COMMAND_HANDLER_GENERIC(id, GetTabProxy() ? GetTabProxy()->command() : 1)

BEGIN_EXEC_COMMAND_MAP(ChromeActiveDocument)
  EXEC_COMMAND_HANDLER_GENERIC(OLECMDID_PRINT, automation_client_->PrintTab())
  EXEC_COMMAND_HANDLER_NO_ARGS(OLECMDID_FIND, OnFindInPage)
  EXEC_COMMAND_HANDLER_NO_ARGS(IDM_FIND, OnFindInPage)
  EXEC_COMMAND_HANDLER_NO_ARGS(INTERNAL_IE_CONTEXTMENU_VIEWSOURCE, OnViewSource)
  FORWARD_TAB_COMMAND(OLECMDID_SELECTALL, SelectAll)
  FORWARD_TAB_COMMAND(OLECMDID_CUT, Cut)
  FORWARD_TAB_COMMAND(OLECMDID_COPY, Copy)
  FORWARD_TAB_COMMAND(OLECMDID_PASTE, Paste)
  FORWARD_TAB_COMMAND(OLECMDID_REFRESH, ReloadAsync)
  FORWARD_TAB_COMMAND(OLECMDID_STOP, StopAsync)
  EXEC_COMMAND_HANDLER(SBCMDID_MIXEDZONE, OnDetermineSecurityZone)
  EXEC_COMMAND_HANDLER(IDM_BASELINEFONT1, SetPageFontSize)
  EXEC_COMMAND_HANDLER(IDM_BASELINEFONT2, SetPageFontSize)
  EXEC_COMMAND_HANDLER(IDM_BASELINEFONT3, SetPageFontSize)
  EXEC_COMMAND_HANDLER(IDM_BASELINEFONT4, SetPageFontSize)
  EXEC_COMMAND_HANDLER(IDM_BASELINEFONT5, SetPageFontSize)
END_EXEC_COMMAND_MAP()

  // IPCs from automation server.
  virtual void OnNavigationStateChanged(int tab_handle, int flags,
                                        const IPC::NavigationInfo& nav_info);
  virtual void OnUpdateTargetUrl(int tab_handle,
                                 const std::wstring& new_target_url);
  virtual void OnAcceleratorPressed(int tab_handle, const MSG& accel_message);
  virtual void OnTabbedOut(int tab_handle, bool reverse);
  virtual void OnDidNavigate(int tab_handle,
                             const IPC::NavigationInfo& nav_info);
  // Override DoVerb
  STDMETHOD(DoVerb)(LONG verb,
                    LPMSG msg,
                    IOleClientSite* active_site,
                    LONG index,
                    HWND parent_window,
                    LPCRECT pos);

  // Override IOleInPlaceActiveObjectImpl::OnDocWindowActivate
  STDMETHOD(OnDocWindowActivate)(BOOL activate);
  STDMETHOD(TranslateAccelerator)(MSG* msg);

  // IPersistMoniker methods
  STDMETHOD(GetClassID)(CLSID* class_id);
  STDMETHOD(IsDirty)();
  STDMETHOD(GetCurMoniker)(IMoniker** moniker_name);
  STDMETHOD(Load)(BOOL fully_avalable,
                  IMoniker* moniker_name,
                  LPBC bind_context,
                  DWORD mode);
  STDMETHOD(Save)(IMoniker* moniker_name,
                  LPBC bind_context,
                  BOOL remember);
  STDMETHOD(SaveCompleted)(IMoniker* moniker_name,
                  LPBC bind_context);

  // IOleCommandTarget methods
  STDMETHOD(QueryStatus)(const GUID* cmd_group_guid,
                         ULONG number_of_commands,
                         OLECMD commands[],
                         OLECMDTEXT* command_text);
  STDMETHOD(Exec)(const GUID* cmd_group_guid, DWORD command_id,
                  DWORD cmd_exec_opt,
                  VARIANT* in_args,
                  VARIANT* out_args);

  // IPersistHistory
  STDMETHOD(LoadHistory)(IStream* stream, IBindCtx* bind_context);
  STDMETHOD(SaveHistory)(IStream* stream);
  STDMETHOD(SetPositionCookie)(DWORD position_cookie);
  STDMETHOD(GetPositionCookie)(DWORD* position_cookie);

  // IWebBrowserEventsUrlService methods
  STDMETHOD(GetUrlForEvents)(BSTR* url);

  // IHTMLPrivateWindow methods
  STDMETHOD(GetAddressBarUrl)(BSTR* url);

  // ChromeFrameActivexBase overrides
  HRESULT IOleObject_SetClientSite(IOleClientSite* client_site);

  HRESULT ActiveXDocActivate(LONG verb);

  // Callbacks from ChromeFramePlugin<T>
  bool PreProcessContextMenu(HMENU menu);
  bool HandleContextMenuCommand(UINT cmd);

  // Should connections initiated by this class try to block
  // responses served with the X-Frame-Options header?
  bool is_frame_busting_enabled();

  // ChromeFramePlugin overrides.
  virtual void OnAutomationServerReady();

 protected:
  // ChromeFrameActivexBase overrides
  virtual void OnOpenURL(int tab_handle, const GURL& url_to_open,
                         const GURL& referrer, int open_disposition);

  virtual void OnGoToHistoryEntryOffset(int tab_handle, int offset);

  // A helper method that updates our internal navigation state
  // as well as IE's navigation state (viz Title and current URL).
  // The navigation_flags is a TabContents::InvalidateTypes enum
  void UpdateNavigationState(const IPC::NavigationInfo& nav_info);

  TabProxy* GetTabProxy() const {
    if (automation_client_.get())
      return automation_client_->tab();
    return NULL;
  }

  // Exec command handlers
  void OnFindInPage();
  void OnViewSource();
  void OnDetermineSecurityZone(const GUID* cmd_group_guid, DWORD command_id,
                               DWORD cmd_exec_opt, VARIANT* in_args,
                               VARIANT* out_args);

  // Call exec on our site's command target
  HRESULT IEExec(const GUID* cmd_group_guid, DWORD command_id,
                 DWORD cmd_exec_opt, VARIANT* in_args, VARIANT* out_args);

  bool IsUrlZoneRestricted(const std::wstring& url);

  // Parses the URL and returns information whether it is a new navigation and
  // the actual url after stripping out the cf: prefix if any.
  // This function also checks if the url scheme is valid for navigation within
  // chrome and whether it is a restricted URL as per IE settings. In either of
  // these cases it returns false.
  bool ParseUrl(const std::wstring& url, bool* is_new_navigation,
                bool* is_chrome_protocol, std::wstring* parsed_url);

  // Initiates navigation to the URL passed in.
  // Returns true on success.
  bool LaunchUrl(const std::wstring& url, bool is_new_navigation);

  // Handler to set the page font size in Chrome.
  HRESULT SetPageFontSize(const GUID* cmd_group_guid,
                          DWORD command_id,
                          DWORD cmd_exec_opt,
                          VARIANT* in_args,
                          VARIANT* out_args);

  // Get the travel log from the client site
  HRESULT GetBrowserServiceAndTravelLog(IBrowserService** browser_service,
                                        ITravelLog** travel_log);

 protected:
  typedef std::map<int, bool> EnabledCommandsMap;

  IPC::NavigationInfo navigation_info_;
  bool is_doc_object_;

  // This indicates whether this is the first navigation in this
  // active document. It is initalize to true and it is set to false
  // after we get a navigation notification from Chrome
  bool first_navigation_;

  // Our find dialog
  CFFindDialog find_dialog_;

  // Contains the list of enabled commands ids.
  EnabledCommandsMap enabled_commands_map_;

  // Set to true if the automation_client_ member is initialized from
  // an existing ChromeActiveDocument instance which is going away and
  // a new ChromeActiveDocument instance is taking its place.
  bool is_automation_client_reused_;

  ScopedComPtr<IInternetSecurityManager> security_manager_;

 public:
  ScopedComPtr<IOleInPlaceFrame> in_place_frame_;
  OLEINPLACEFRAMEINFO frame_info_;
};

OBJECT_ENTRY_AUTO(__uuidof(ChromeActiveDocument), ChromeActiveDocument)

#endif  // CHROME_FRAME_CHROME_ACTIVE_DOCUMENT_H_
