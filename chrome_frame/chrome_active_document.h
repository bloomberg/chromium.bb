// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_CHROME_ACTIVE_DOCUMENT_H_
#define CHROME_FRAME_CHROME_ACTIVE_DOCUMENT_H_

#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>
#include <htiframe.h>
#include <mshtmcid.h>
#include <perhist.h>

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "base/win/scoped_comptr.h"

#include "chrome_frame/chrome_frame_activex_base.h"
#include "chrome_frame/com_type_info_holder.h"
#include "chrome_frame/extra_system_apis.h"
#include "chrome_frame/find_dialog.h"
#include "chrome_frame/html_private_window_impl.h"
#include "chrome_frame/html_window_impl.h"
#include "chrome_frame/resource.h"
#include "chrome_frame/utils.h"
#include "third_party/active_doc/in_place_menu.h"
#include "third_party/active_doc/ole_document_impl.h"

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

// The following macro is to define the mapping of IE encoding menu item to
// corresponding available encoding name in Chrome. For each encoding
// definition, there are three fields.
// The first one is the definition name of encoding menu item in IE.
// The second one is execution id of the encoding menu item in IE, starting
// from 3609(IDM_MIMECSET__FIRST__) to 3699(IDM_MIMECSET__LAST__) end. For
// the details, please refer to mshtmcid.h.
// The last one is the available encoding name of the IE encoding menu item
// in Chrome. If the encoding menu item does not have corresponding encoding
// in Chrome, it will be "unknown".
// So far we don't support the encoding auto detect since we can not control
// the status of encoding menu, such as toggle status of encoding auto detect
// item on the encoding menu.
#define INTERNAL_IE_ENCODINGMENU_IDS(V) \
  V(INTERNAL_IE_ENCODINGMENU_ARABIC_ASMO708, 3609, "unknown") \
  V(INTERNAL_IE_ENCODINGMENU_ARABIC_DOS, 3610, "unknown") \
  V(INTERNAL_IE_ENCODINGMENU_ARABIC_ISO, 3611, "ISO-8859-6") \
  V(INTERNAL_IE_ENCODINGMENU_ARABIC_WINDOWS, 3612, "windows-1256") \
  V(INTERNAL_IE_ENCODINGMENU_BALTIC_ISO, 3614, "ISO-8859-4") \
  V(INTERNAL_IE_ENCODINGMENU_BALTIC_WINDOWS, 3615, "windows-1257") \
  V(INTERNAL_IE_ENCODINGMENU_CENTRAL_EUROPEAN_DOS, 3616, "unknown") \
  V(INTERNAL_IE_ENCODINGMENU_CENTRAL_EUROPEAN_ISO, 3617, "ISO-8859-2") \
  V(INTERNAL_IE_ENCODINGMENU_CENTRAL_EUROPEAN_WINDOWS, 3618, "windows-1250") \
  V(INTERNAL_IE_ENCODINGMENU_CHINESE_SIMP_GB18030, 3619, "gb18030") \
  V(INTERNAL_IE_ENCODINGMENU_CHINESE_SIMP_GB2312, 3620, "GBK") \
  V(INTERNAL_IE_ENCODINGMENU_CHINESE_SIMP_HZ, 3621, "unknown") \
  V(INTERNAL_IE_ENCODINGMENU_CHINESE_TRAD_BIG5, 3622, "Big5") \
  V(INTERNAL_IE_ENCODINGMENU_CYRILLIC_DOS, 3623, "unknown") \
  V(INTERNAL_IE_ENCODINGMENU_CYRILLIC_ISO, 3624, "ISO-8859-5") \
  V(INTERNAL_IE_ENCODINGMENU_CYRILLIC_KOI8R, 3625, "KOI8-R") \
  V(INTERNAL_IE_ENCODINGMENU_CYRILLIC_KOI8U, 3626, "KOI8-U") \
  V(INTERNAL_IE_ENCODINGMENU_CYRILLIC_WINDOWS, 3627, "windows-1251") \
  V(INTERNAL_IE_ENCODINGMENU_GREEK_ISO, 3628, "ISO-8859-7") \
  V(INTERNAL_IE_ENCODINGMENU_GREEK_WINDOWS, 3629, "windows-1253") \
  V(INTERNAL_IE_ENCODINGMENU_HEBREW_DOS, 3630, "unknown") \
  V(INTERNAL_IE_ENCODINGMENU_HEBREW_ISO_LOGICAL, 3631, "ISO-8859-8-I") \
  V(INTERNAL_IE_ENCODINGMENU_HEBREW_ISO_VISUAL, 3632, "ISO-8859-8") \
  V(INTERNAL_IE_ENCODINGMENU_HEBREW_WINDOWS, 3633, "windows-1255") \
  V(INTERNAL_IE_ENCODINGMENU_JAPAN_AUTOSELECT, 3634, "ISO-2022-JP") \
  V(INTERNAL_IE_ENCODINGMENU_JAPAN_EUC, 3635, "EUC-JP") \
  V(INTERNAL_IE_ENCODINGMENU_JAPAN_SHIFT_JIS, 3636, "Shift_JIS") \
  V(INTERNAL_IE_ENCODINGMENU_KOREA, 3637, "windows-949") \
  V(INTERNAL_IE_ENCODINGMENU_THAI, 3638, "windows-874") \
  V(INTERNAL_IE_ENCODINGMENU_TURKISH_ISO, 3639, "windows-1254") \
  V(INTERNAL_IE_ENCODINGMENU_TURKISH_WINDOWS, 3640, "windows-1254") \
  V(INTERNAL_IE_ENCODINGMENU_UTF8, 3641, "UTF-8") \
  V(INTERNAL_IE_ENCODINGMENU_USERDEFINED, 3642, "windows-1252") \
  V(INTERNAL_IE_ENCODINGMENU_VIETNAMESE, 3643, "windows-1258") \
  V(INTERNAL_IE_ENCODINGMENU_WEST_EUROPEAN_ISO8859_1, 3644, "ISO-8859-1") \
  V(INTERNAL_IE_ENCODINGMENU_WEST_EUROPEAN_WINDOWS, 3645, "windows-1252") \
  V(INTERNAL_IE_ENCODINGMENU_AUTODETECT, 3699, "unknown")

#define DEFINE_ENCODING_ID(encoding_name, id, chrome_name) \
    const DWORD encoding_name = id;
  INTERNAL_IE_ENCODINGMENU_IDS(DEFINE_ENCODING_ID)
#undef DEFINE_ENCODING_ID
extern const DWORD kIEEncodingIdArray[];

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

#define DOCHOST_DISPLAY_PRIVACY                      (75)

// This macro should be defined in the public section of the class.
#define BEGIN_EXEC_COMMAND_MAP(theClass) \
 public: \
  HRESULT ProcessExecCommand(const GUID* cmd_group_guid, DWORD command_id, \
                             DWORD cmd_exec_opt, VARIANT* in_args, \
                             VARIANT* out_args) { \
  HRESULT hr = OLECMDERR_E_NOTSUPPORTED; \
  do {

#define EXEC_COMMAND_HANDLER(group, id, handler)                              \
  if ((id == command_id) && ((group != NULL && cmd_group_guid != NULL && \
       IsEqualGUID(*reinterpret_cast<const GUID*>(group), *cmd_group_guid)) || \
       (group == NULL && cmd_group_guid == NULL))) {  \
    hr = S_OK;  \
    handler(cmd_group_guid, command_id, cmd_exec_opt, in_args, out_args);  \
    break;  \
  }

#define EXEC_COMMAND_HANDLER_NO_ARGS(group, id, handler) \
  if ((id == command_id) && ((group != NULL && cmd_group_guid != NULL && \
       IsEqualGUID(*reinterpret_cast<const GUID*>(group), *cmd_group_guid)) || \
       (group == NULL && cmd_group_guid == NULL))) {  \
    hr = S_OK;  \
    handler();  \
    break;  \
  }

#define EXEC_COMMAND_HANDLER_GENERIC(group, id, code) \
  if ((id == command_id) && ((group != NULL && cmd_group_guid != NULL && \
       IsEqualGUID(*reinterpret_cast<const GUID*>(group), *cmd_group_guid)) || \
       (group == NULL && cmd_group_guid == NULL))) {  \
    hr = S_OK;  \
    code;  \
    break;  \
  }

#define EXEC_GROUP_COMMANDS_HANDLER(group, group_commands, handler) \
  do { \
    const DWORD* commands = group_commands; \
    bool id_in_group_commands = false; \
    while (*commands) { \
      if (*commands == command_id) { \
        id_in_group_commands = true; \
        break; \
      } \
      commands++; \
    } \
    if (id_in_group_commands && ((group != NULL && cmd_group_guid != NULL && \
        IsEqualGUID(*reinterpret_cast<const GUID*>(group), \
                    *cmd_group_guid)) || \
        (group == NULL && cmd_group_guid == NULL))) { \
      hr = S_OK; \
      handler(cmd_group_guid, command_id, cmd_exec_opt, in_args, out_args); \
      break; \
    } \
  } while (0);

#define END_EXEC_COMMAND_MAP()  \
  } while (0); \
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
      public IEnumPrivacyRecords,
      public HTMLWindowImpl<IHTMLWindow2>,
      public HTMLPrivateWindowImpl<IHTMLPrivateWindow> {
 public:
  typedef ChromeFrameActivexBase<ChromeActiveDocument,
      CLSID_ChromeActiveDocument> BaseActiveX;

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
  COM_INTERFACE_ENTRY(IEnumPrivacyRecords)
  COM_INTERFACE_ENTRY_CHAIN(BaseActiveX)
END_COM_MAP()

BEGIN_MSG_MAP(ChromeActiveDocument)
  MESSAGE_HANDLER(WM_FIRE_PRIVACY_CHANGE_NOTIFICATION, OnFirePrivacyChange)
  COMMAND_ID_HANDLER(IDC_CHROMEFRAME_FORWARD, OnForward)
  COMMAND_ID_HANDLER(IDC_CHROMEFRAME_BACK, OnBack)
  MESSAGE_HANDLER(WM_SHOWWINDOW, OnShowWindow)
  MESSAGE_HANDLER(WM_SETFOCUS, OnSetFocus)
  CHAIN_MSG_MAP(BaseActiveX)
END_MSG_MAP()

  HRESULT FinalConstruct();

#define FORWARD_TAB_COMMAND(group, id, command) \
  EXEC_COMMAND_HANDLER_GENERIC(group, id, GetTabProxy() ? \
    GetTabProxy()->command() : 1)

BEGIN_EXEC_COMMAND_MAP(ChromeActiveDocument)
  EXEC_COMMAND_HANDLER_GENERIC(NULL, OLECMDID_PRINT,
                               automation_client_->PrintTab())
  EXEC_COMMAND_HANDLER_NO_ARGS(NULL, OLECMDID_FIND, OnFindInPage)
  EXEC_COMMAND_HANDLER_NO_ARGS(&CGID_MSHTML, IDM_FIND, OnFindInPage)
  EXEC_COMMAND_HANDLER_NO_ARGS(&CGID_MSHTML, IDM_VIEWSOURCE, OnViewSource)
  FORWARD_TAB_COMMAND(NULL, OLECMDID_SELECTALL, SelectAll)
  FORWARD_TAB_COMMAND(NULL, OLECMDID_CUT, Cut)
  FORWARD_TAB_COMMAND(NULL, OLECMDID_COPY, Copy)
  FORWARD_TAB_COMMAND(NULL, OLECMDID_PASTE, Paste)
  FORWARD_TAB_COMMAND(NULL, OLECMDID_STOP, StopAsync)
  FORWARD_TAB_COMMAND(NULL, OLECMDID_SAVEAS, SaveAsAsync)
  EXEC_COMMAND_HANDLER(NULL, OLECMDID_REFRESH, OnRefreshPage)
  EXEC_COMMAND_HANDLER(&CGID_Explorer, SBCMDID_MIXEDZONE,
                       OnDetermineSecurityZone)
  EXEC_COMMAND_HANDLER(&CGID_MSHTML, IDM_BASELINEFONT1, SetPageFontSize)
  EXEC_COMMAND_HANDLER(&CGID_MSHTML, IDM_BASELINEFONT2, SetPageFontSize)
  EXEC_COMMAND_HANDLER(&CGID_MSHTML, IDM_BASELINEFONT3, SetPageFontSize)
  EXEC_COMMAND_HANDLER(&CGID_MSHTML, IDM_BASELINEFONT4, SetPageFontSize)
  EXEC_COMMAND_HANDLER(&CGID_MSHTML, IDM_BASELINEFONT5, SetPageFontSize)

  EXEC_GROUP_COMMANDS_HANDLER(&CGID_MSHTML, kIEEncodingIdArray,
                              OnEncodingChange)

  EXEC_COMMAND_HANDLER_NO_ARGS(&CGID_ShellDocView, DOCHOST_DISPLAY_PRIVACY,
                               OnDisplayPrivacyInfo)
  EXEC_COMMAND_HANDLER(NULL, OLECMDID_OPTICAL_GETZOOMRANGE, OnGetZoomRange)
  EXEC_COMMAND_HANDLER(NULL, OLECMDID_OPTICAL_ZOOM, OnSetZoomRange)
  EXEC_COMMAND_HANDLER(NULL, OLECMDID_ONUNLOAD, OnUnload)
END_EXEC_COMMAND_MAP()

  // IPCs from automation server.
  virtual void OnNavigationStateChanged(
      int flags, const NavigationInfo& nav_info);
  virtual void OnUpdateTargetUrl(const std::wstring& new_target_url);
  virtual void OnAcceleratorPressed(const MSG& accel_message);
  virtual void OnTabbedOut(bool reverse);
  virtual void OnDidNavigate(const NavigationInfo& nav_info);
  virtual void OnCloseTab();
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
  bool HandleContextMenuCommand(UINT cmd, const MiniContextMenuParams& params);

  // ChromeFramePlugin overrides.
  virtual void OnAutomationServerReady();

  // IEnumPrivacyRecords
  STDMETHOD(Reset)();
  STDMETHOD(GetSize)(ULONG* size);
  STDMETHOD(GetPrivacyImpacted)(BOOL* privacy_impacted);
  STDMETHOD(Next)(BSTR* url, BSTR* policy, LONG* reserved, DWORD* flags);

  // NavigationConstraints overrides.
  bool IsSchemeAllowed(const GURL& url);

  // Accessor for InPlaceMenu.  Returns S_OK if set, S_FALSE if NULL.
  HRESULT GetInPlaceFrame(IOleInPlaceFrame** in_place_frame);

 protected:
  // ChromeFrameActivexBase overrides
  virtual void OnAttachExternalTab(const AttachExternalTabParams& params);
  virtual void OnGoToHistoryEntryOffset(int offset);
  virtual void OnMoveWindow(const gfx::Rect& dimensions);

  // A helper method that updates our internal navigation state
  // as well as IE's navigation state (viz Title and current URL).
  // The navigation_flags is a TabContents::InvalidateTypes enum
  void UpdateNavigationState(const NavigationInfo& nav_info, int flags);

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
  void OnDisplayPrivacyInfo();

  void OnGetZoomRange(const GUID* cmd_group_guid, DWORD command_id,
                      DWORD cmd_exec_opt, VARIANT* in_args, VARIANT* out_args);

  void OnSetZoomRange(const GUID* cmd_group_guid, DWORD command_id,
                      DWORD cmd_exec_opt, VARIANT* in_args, VARIANT* out_args);

  // This function handles the OLECMDID_ONUNLOAD command. It enables Chrome to
  // invoke before unload and unload handlers on the page if any, thereby
  // enabling a webpage to potentially cancel the operation.
  void OnUnload(const GUID* cmd_group_guid, DWORD command_id,
                DWORD cmd_exec_opt, VARIANT* in_args, VARIANT* out_args);

  // Call exec on our site's command target
  HRESULT IEExec(const GUID* cmd_group_guid, DWORD command_id,
                 DWORD cmd_exec_opt, VARIANT* in_args, VARIANT* out_args);

  // Initiates navigation to the URL passed in.
  // Returns true on success.
  bool LaunchUrl(const ChromeFrameUrl& cf_url, const std::string& referrer);

  // Handler to set the page font size in Chrome.
  HRESULT SetPageFontSize(const GUID* cmd_group_guid,
                          DWORD command_id,
                          DWORD cmd_exec_opt,
                          VARIANT* in_args,
                          VARIANT* out_args);

  // IOleCommandTarget handler for page refresh command
  HRESULT OnRefreshPage(const GUID* cmd_group_guid, DWORD command_id,
      DWORD cmd_exec_opt, VARIANT* in_args, VARIANT* out_args);

  // Handler to set the page encoding info in Chrome.
  HRESULT OnEncodingChange(const GUID* cmd_group_guid, DWORD command_id,
                           DWORD cmd_exec_opt, VARIANT* in_args,
                           VARIANT* out_args);

  // Get the travel log from the client site
  HRESULT GetBrowserServiceAndTravelLog(IBrowserService** browser_service,
                                        ITravelLog** travel_log);
  LRESULT OnForward(WORD notify_code, WORD id, HWND control_window,
                    BOOL& bHandled);
  LRESULT OnBack(WORD notify_code, WORD id, HWND control_window,
                 BOOL& bHandled);

  LRESULT OnFirePrivacyChange(UINT message, WPARAM wparam, LPARAM lparam,
                              BOOL& handled);
  LRESULT OnShowWindow(UINT message, WPARAM wparam, LPARAM lparam,
                       BOOL& handled);
  LRESULT OnSetFocus(UINT message, WPARAM wparam, LPARAM lparam,
                     BOOL& handled);

  // Returns true if the NavigationInfo object passed in represents a new
  // navigation initiated by the renderer.
  bool IsNewNavigation(const NavigationInfo& new_navigation_info,
                       int flags) const;

  // Returns true if the NavigationInfo object passed in represents a first
  // page navigation initiated as this active document was being created.
  bool IsFirstNavigation(const NavigationInfo& new_navigation_info) const;

 protected:
  typedef std::map<int, OLECMDF> CommandStatusMap;

  scoped_ptr<NavigationInfo> navigation_info_;
  bool is_doc_object_;

  // This indicates whether this is the first navigation in this
  // active document. It is initalize to true and it is set to false
  // after we get a navigation notification from Chrome
  bool first_navigation_;

  // Our find dialog
  CFFindDialog find_dialog_;

  // These members contain the status of the commands we support.
  CommandStatusMap null_group_commands_map_;
  CommandStatusMap mshtml_group_commands_map_;
  CommandStatusMap explorer_group_commands_map_;
  CommandStatusMap shdoc_view_group_commands_map_;

  // Set to true if the automation_client_ member is initialized from
  // an existing ChromeActiveDocument instance which is going away and
  // a new ChromeActiveDocument instance is taking its place.
  bool is_automation_client_reused_;

  base::win::ScopedComPtr<INewWindowManager> popup_manager_;
  bool popup_allowed_;
  HACCEL accelerator_table_;

  // Contains privacy data retrieved from the UrlmonUrlRequestManager. This
  // is used to return privacy data in response to the View->Privacy policy
  // command.
  UrlmonUrlRequestManager::PrivacyInfo privacy_info_;
  UrlmonUrlRequestManager::PrivacyInfo::PrivacyRecords::iterator
      next_privacy_record_;

 public:
  OLEINPLACEFRAMEINFO frame_info_;
};

#endif  // CHROME_FRAME_CHROME_ACTIVE_DOCUMENT_H_
