// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of ChromeActiveDocument
#include "chrome_frame/chrome_active_document.h"

#include <hlink.h>
#include <htiface.h>
#include <mshtmcid.h>
#include <shdeprecated.h>
#include <shlguid.h>
#include <shobjidl.h>
#include <tlogstg.h>
#include <urlmon.h>
#include <wininet.h>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/registry.h"
#include "base/scoped_variant_win.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "base/thread_local.h"

#include "grit/generated_resources.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/navigation_types.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome_frame/utils.h"

const wchar_t kChromeAttachExternalTabPrefix[] = L"attach_external_tab";

static const wchar_t kUseChromeNetworking[] = L"UseChromeNetworking";
static const wchar_t kHandleTopLevelRequests[] =
    L"HandleTopLevelRequests";

base::ThreadLocalPointer<ChromeActiveDocument> g_active_doc_cache;

bool g_first_launch_by_process_ = true;

ChromeActiveDocument::ChromeActiveDocument()
    : first_navigation_(true),
      is_automation_client_reused_(false) {
}

HRESULT ChromeActiveDocument::FinalConstruct() {
  // If we have a cached ChromeActiveDocument instance in TLS, then grab
  // ownership of the cached document's automation client. This is an
  // optimization to get Chrome active documents to load faster.
  ChromeActiveDocument* cached_document = g_active_doc_cache.Get();
  if (cached_document) {
    DCHECK(automation_client_.get() == NULL);
    automation_client_.reset(cached_document->automation_client_.release());
    DLOG(INFO) << "Reusing automation client instance from "
               << cached_document;
    DCHECK(automation_client_.get() != NULL);
    automation_client_->Reinitialize(this);
    is_automation_client_reused_ = true;
  } else {
    // The FinalConstruct implementation in the ChromeFrameActivexBase class
    // i.e. Base creates an instance of the ChromeFrameAutomationClient class
    // and initializes it, which would spawn a new Chrome process, etc.
    // We don't want to be doing this if we have a cached document, whose
    // automation client instance can be reused.
    HRESULT hr = Base::FinalConstruct();
    if (FAILED(hr))
      return hr;
  }

  bool chrome_network = GetConfigBool(false, kUseChromeNetworking);
  bool top_level_requests = GetConfigBool(true, kHandleTopLevelRequests);
  automation_client_->set_use_chrome_network(chrome_network);
  automation_client_->set_handle_top_level_requests(top_level_requests);

  find_dialog_.Init(automation_client_.get());

  enabled_commands_map_[OLECMDID_PRINT] = true;
  enabled_commands_map_[OLECMDID_FIND] = true;
  enabled_commands_map_[OLECMDID_CUT] = true;
  enabled_commands_map_[OLECMDID_COPY] = true;
  enabled_commands_map_[OLECMDID_PASTE] = true;
  enabled_commands_map_[OLECMDID_SELECTALL] = true;
  return S_OK;
}

ChromeActiveDocument::~ChromeActiveDocument() {
  DLOG(INFO) << __FUNCTION__;
  if (find_dialog_.IsWindow()) {
    find_dialog_.DestroyWindow();
  }
}

// Override DoVerb
STDMETHODIMP ChromeActiveDocument::DoVerb(LONG verb,
                                          LPMSG msg,
                                          IOleClientSite* active_site,
                                          LONG index,
                                          HWND parent_window,
                                          LPCRECT pos) {
  // IE will try and in-place activate us in some cases. This happens when
  // the user opens a new IE window with a URL that has us as the DocObject.
  // Here we refuse to be activated in-place and we will force IE to UIActivate
  // us.
  if (OLEIVERB_INPLACEACTIVATE == verb) {
    return E_NOTIMPL;
  }
  // Check if we should activate as a docobject or not
  // (client supports IOleDocumentSite)
  if (doc_site_) {
    switch (verb) {
    case OLEIVERB_SHOW:
    case OLEIVERB_OPEN:
    case OLEIVERB_UIACTIVATE:
      if (!m_bUIActive) {
        return doc_site_->ActivateMe(NULL);
      }
      break;
    }
  }
  return IOleObjectImpl<ChromeActiveDocument>::DoVerb(verb,
                                                      msg,
                                                      active_site,
                                                      index,
                                                      parent_window,
                                                      pos);
}

STDMETHODIMP ChromeActiveDocument::InPlaceDeactivate(void) {
  // Release the pointers we have no need for now.
  doc_site_.Release();
  in_place_frame_.Release();
  return IOleInPlaceObjectWindowlessImpl<ChromeActiveDocument>::
             InPlaceDeactivate();
}

// Override IOleInPlaceActiveObjectImpl::OnDocWindowActivate
STDMETHODIMP ChromeActiveDocument::OnDocWindowActivate(BOOL activate) {
  DLOG(INFO) << __FUNCTION__;
  return S_OK;
}

STDMETHODIMP ChromeActiveDocument::TranslateAccelerator(MSG* msg) {
  DLOG(INFO) << __FUNCTION__;
  if (msg == NULL)
    return E_POINTER;

  if (msg->message == WM_KEYDOWN && msg->wParam == VK_TAB) {
    HWND focus = ::GetFocus();
    if (focus != m_hWnd && !::IsChild(m_hWnd, focus)) {
      // The call to SetFocus triggers a WM_SETFOCUS that makes the base class
      // set focus to the correct element in Chrome.
      ::SetFocus(m_hWnd);
      return S_OK;
    }
  }

  return S_FALSE;
}
// Override IPersistStorageImpl::IsDirty
STDMETHODIMP ChromeActiveDocument::IsDirty() {
  DLOG(INFO) << __FUNCTION__;
  return S_FALSE;
}

bool ChromeActiveDocument::is_frame_busting_enabled() {
  return false;
}

STDMETHODIMP ChromeActiveDocument::Load(BOOL fully_avalable,
                                        IMoniker* moniker_name,
                                        LPBC bind_context,
                                        DWORD mode) {
  if (NULL == moniker_name) {
    return E_INVALIDARG;
  }
  CComHeapPtr<WCHAR> display_name;
  moniker_name->GetDisplayName(bind_context, NULL, &display_name);
  std::wstring url = display_name;

  bool is_chrome_protocol = StartsWith(url, kChromeProtocolPrefix, false);
  bool is_new_navigation = true;

  if (is_chrome_protocol) {
    url.erase(0, lstrlen(kChromeProtocolPrefix));
    is_new_navigation = 
        !StartsWith(url, kChromeAttachExternalTabPrefix, false);
  }

  if (!IsValidUrlScheme(url)) {
    DLOG(WARNING) << __FUNCTION__ << " Disallowing navigation to url: "
                  << url;
    return E_INVALIDARG;
  }

  if (!is_new_navigation) {
    WStringTokenizer tokenizer(url, L"&");
    // Skip over kChromeAttachExternalTabPrefix
    tokenizer.GetNext();

    intptr_t external_tab_cookie = 0;

    if (tokenizer.GetNext())
      StringToInt(tokenizer.token(),
                  reinterpret_cast<int*>(&external_tab_cookie));

    if (external_tab_cookie == 0) {
      NOTREACHED() << "invalid url for attach tab: " << url;
      return E_FAIL;
    }

    automation_client_->AttachExternalTab(external_tab_cookie);
  } 

  // Initiate navigation before launching chrome so that the url will be
  // cached and sent with launch settings.
  if (is_new_navigation) {
    url_.Reset(::SysAllocString(url.c_str()));
    if (url_.Length()) {
      std::string utf8_url;
      WideToUTF8(url_, url_.Length(), &utf8_url);
      if (!automation_client_->InitiateNavigation(utf8_url)) {
        DLOG(ERROR) << "Invalid URL: " << url;
        Error(L"Invalid URL");
        url_.Reset();
        return E_INVALIDARG;
      }

      DLOG(INFO) << "Url is " << url_;
    }
  }

  if (!is_automation_client_reused_ &&
      !InitializeAutomation(GetHostProcessName(false), L"", IsIEInPrivate())) {
    return E_FAIL;
  }

  if (!is_chrome_protocol) {
    CComObject<UrlmonUrlRequest>* new_request = NULL;
    CComObject<UrlmonUrlRequest>::CreateInstance(&new_request);
    new_request->AddRef();

    if (SUCCEEDED(new_request->ConnectToExistingMoniker(moniker_name,
                                                        bind_context,
                                                        url))) {
      base_url_request_.swap(&new_request);
      DCHECK(new_request == NULL);
    } else {
      new_request->Release();
    }
  }

  UMA_HISTOGRAM_CUSTOM_COUNTS("ChromeFrame.FullTabLaunchType",
                              is_chrome_protocol, 0, 1, 2);
  return S_OK;
}

STDMETHODIMP ChromeActiveDocument::Save(IMoniker* moniker_name,
                                        LPBC bind_context,
                                        BOOL remember) {
  return E_NOTIMPL;
}

STDMETHODIMP ChromeActiveDocument::SaveCompleted(IMoniker* moniker_name,
                                                 LPBC bind_context) {
  return E_NOTIMPL;
}

STDMETHODIMP ChromeActiveDocument::GetCurMoniker(IMoniker** moniker_name) {
  return E_NOTIMPL;
}

STDMETHODIMP ChromeActiveDocument::GetClassID(CLSID* class_id) {
  if (NULL == class_id) {
    return E_POINTER;
  }
  *class_id = GetObjectCLSID();
  return S_OK;
}

STDMETHODIMP ChromeActiveDocument::QueryStatus(const GUID* cmd_group_guid,
                                               ULONG number_of_commands,
                                               OLECMD commands[],
                                               OLECMDTEXT* command_text) {
  DLOG(INFO) << __FUNCTION__;
  for (ULONG command_index = 0; command_index < number_of_commands;
       command_index++) {
    DLOG(INFO) << "Command id = " << commands[command_index].cmdID;
    if (enabled_commands_map_.find(commands[command_index].cmdID) !=
        enabled_commands_map_.end()) {
      commands[command_index].cmdf = OLECMDF_ENABLED;
    }
  }
  return S_OK;
}

STDMETHODIMP ChromeActiveDocument::Exec(const GUID* cmd_group_guid,
                                        DWORD command_id,
                                        DWORD cmd_exec_opt,
                                        VARIANT* in_args,
                                        VARIANT* out_args) {
  DLOG(INFO) << __FUNCTION__ << " Cmd id =" << command_id;
  // Bail out if we have been uninitialized.
  if (automation_client_.get() && automation_client_->tab()) {
    return ProcessExecCommand(cmd_group_guid, command_id, cmd_exec_opt,
                              in_args, out_args);
  }
  return S_FALSE;
}

STDMETHODIMP ChromeActiveDocument::GetUrlForEvents(BSTR* url) {
  if (NULL == url) {
    return E_POINTER;
  }
  *url = ::SysAllocString(url_);
  return S_OK;
}

HRESULT ChromeActiveDocument::IOleObject_SetClientSite(
    IOleClientSite* client_site) {
  if (client_site == NULL) {
    ChromeActiveDocument* cached_document = g_active_doc_cache.Get();
    if (cached_document) {
      DCHECK(this == cached_document);
      g_active_doc_cache.Set(NULL);
      cached_document->Release();
    }
  }
  return Base::IOleObject_SetClientSite(client_site);
}


HRESULT ChromeActiveDocument::ActiveXDocActivate(LONG verb) {
  HRESULT hr = S_OK;
  m_bNegotiatedWnd = TRUE;
  if (!m_bInPlaceActive) {
    hr = m_spInPlaceSite->CanInPlaceActivate();
    if (FAILED(hr)) {
      return hr;
    }
    m_spInPlaceSite->OnInPlaceActivate();
  }
  m_bInPlaceActive = TRUE;
  // get location in the parent window,
  // as well as some information about the parent
  ScopedComPtr<IOleInPlaceUIWindow> in_place_ui_window;
  frame_info_.cb = sizeof(OLEINPLACEFRAMEINFO);
  HWND parent_window = NULL;
  if (m_spInPlaceSite->GetWindow(&parent_window) == S_OK) {
    in_place_frame_.Release();
    RECT position_rect = {0};
    RECT clip_rect = {0};
    m_spInPlaceSite->GetWindowContext(in_place_frame_.Receive(),
                                      in_place_ui_window.Receive(),
                                      &position_rect,
                                      &clip_rect,
                                      &frame_info_);
    if (!m_bWndLess) {
      if (IsWindow()) {
        ::ShowWindow(m_hWnd, SW_SHOW);
        SetFocus();
      } else {
        m_hWnd = Create(parent_window, position_rect);
      }
    }
    SetObjectRects(&position_rect, &clip_rect);
  }

  ScopedComPtr<IOleInPlaceActiveObject> in_place_active_object(this);

  // Gone active by now, take care of UIACTIVATE
  if (DoesVerbUIActivate(verb)) {
    if (!m_bUIActive) {
      m_bUIActive = TRUE;
      hr = m_spInPlaceSite->OnUIActivate();
      if (FAILED(hr)) {
        return hr;
      }
      // set ourselves up in the host
      if (in_place_active_object) {
        if (in_place_frame_) {
          in_place_frame_->SetActiveObject(in_place_active_object, NULL);
        }
        if (in_place_ui_window) {
          in_place_ui_window->SetActiveObject(in_place_active_object, NULL);
        }
      }
    }
  }
  m_spClientSite->ShowObject();
  return S_OK;
}

void ChromeActiveDocument::OnNavigationStateChanged(int tab_handle, int flags,
    const IPC::NavigationInfo& nav_info) {
  // TODO(joshia): handle INVALIDATE_TAB,INVALIDATE_LOAD etc.
  DLOG(INFO) << __FUNCTION__ << std::endl << " Flags: " << flags
      << "Url: " << nav_info.url <<
      ", Title: " << nav_info.title <<
      ", Type: " << nav_info.navigation_type << ", Relative Offset: " <<
      nav_info.relative_offset << ", Index: " << nav_info.navigation_index;;

  UpdateNavigationState(nav_info);
}

void ChromeActiveDocument::OnUpdateTargetUrl(int tab_handle,
    const std::wstring& new_target_url) {
  if (in_place_frame_) {
    in_place_frame_->SetStatusText(new_target_url.c_str());
  }
}

bool IsFindAccelerator(const MSG& msg) {
  // TODO(robertshield): This may not stand up to localization. Fix if this
  // is the case.
  return msg.message == WM_KEYDOWN && msg.wParam == 'F' &&
         win_util::IsCtrlPressed() &&
         !(win_util::IsAltPressed() || win_util::IsShiftPressed());
}

void ChromeActiveDocument::OnAcceleratorPressed(int tab_handle,
                                                const MSG& accel_message) {
  bool handled_accel = false;
  if (in_place_frame_ != NULL) {
    handled_accel = (S_OK == in_place_frame_->TranslateAcceleratorW(
        const_cast<MSG*>(&accel_message), 0));
  }

  if (!handled_accel) {
    if (IsFindAccelerator(accel_message)) {
      // Handle the showing of the find dialog explicitly.
      OnFindInPage();
    } else if (AllowFrameToTranslateAccelerator(accel_message) != S_OK) {
      DLOG(INFO) << "IE DID NOT handle accel key " << accel_message.wParam;
      TabProxy* tab = GetTabProxy();
      if (tab) {
        tab->ProcessUnhandledAccelerator(accel_message);
      }
    }
  } else {
    DLOG(INFO) << "IE handled accel key " << accel_message.wParam;
  }
}

void ChromeActiveDocument::OnTabbedOut(int tab_handle, bool reverse) {
  DLOG(INFO) << __FUNCTION__;
  if (in_place_frame_) {
    MSG msg = { NULL, WM_KEYDOWN, VK_TAB };
    in_place_frame_->TranslateAcceleratorW(&msg, 0);
  }
}

void ChromeActiveDocument::OnDidNavigate(int tab_handle,
                                         const IPC::NavigationInfo& nav_info) {
  DLOG(INFO) << __FUNCTION__ << std::endl << "Url: " << nav_info.url <<
      ", Title: " << nav_info.title <<
      ", Type: " << nav_info.navigation_type << ", Relative Offset: " <<
      nav_info.relative_offset << ", Index: " << nav_info.navigation_index;

  // This could be NULL if the active document instance is being destroyed.
  if (!m_spInPlaceSite) {
    DLOG(INFO) << __FUNCTION__ << "m_spInPlaceSite is NULL. Returning";
    return;
  }

  UpdateNavigationState(nav_info);
}

void ChromeActiveDocument::UpdateNavigationState(
    const IPC::NavigationInfo& new_navigation_info) {
  bool is_title_changed = (navigation_info_.title != new_navigation_info.title);
  bool is_url_changed = (navigation_info_.url.is_valid() &&
      (navigation_info_.url != new_navigation_info.url));
  bool is_ssl_state_changed =
      (navigation_info_.security_style != new_navigation_info.security_style) ||
      (navigation_info_.has_mixed_content !=
          new_navigation_info.has_mixed_content);

  navigation_info_ = new_navigation_info;

  if (is_title_changed) {
    ScopedVariant title(navigation_info_.title.c_str());
    IEExec(NULL, OLECMDID_SETTITLE, OLECMDEXECOPT_DONTPROMPTUSER,
           title.AsInput(), NULL);
  }

  if (is_ssl_state_changed) {
    int lock_status = SECURELOCK_SET_UNSECURE;
    switch (navigation_info_.security_style) {
      case SECURITY_STYLE_AUTHENTICATION_BROKEN:
        lock_status = SECURELOCK_SET_SECUREUNKNOWNBIT;
        break;
      case SECURITY_STYLE_AUTHENTICATED:
        lock_status = navigation_info_.has_mixed_content ?
            SECURELOCK_SET_MIXED : SECURELOCK_SET_SECUREUNKNOWNBIT;
        break;
      default:
        break;
    }

    ScopedVariant secure_lock_status(lock_status);
    IEExec(&CGID_ShellDocView, INTERNAL_CMDID_SET_SSL_LOCK,
           OLECMDEXECOPT_DODEFAULT, secure_lock_status.AsInput(), NULL);
  }

  if (navigation_info_.url.is_valid() &&
      (is_url_changed || url_.Length() == 0)) {
    url_.Allocate(UTF8ToWide(navigation_info_.url.spec()).c_str());
    // Now call the FireNavigateCompleteEvent which makes IE update the text
    // in the address-bar. We call the FireBeforeNavigateComplete2Event and
    // FireDocumentComplete event just for completeness sake. If some BHO
    // chooses to cancel the navigation in the OnBeforeNavigate2 handler
    // we will ignore the cancellation request.

    // Todo(joshia): investigate if there's a better way to set URL in the
    // address bar
    ScopedComPtr<IWebBrowserEventsService> web_browser_events_svc;
    DoQueryService(__uuidof(web_browser_events_svc), m_spClientSite,
                   web_browser_events_svc.Receive());
    if (web_browser_events_svc) {
      // TODO(joshia): maybe we should call FireBeforeNavigate2Event in
      // ChromeActiveDocument::Load and abort if cancelled.
      VARIANT_BOOL should_cancel = VARIANT_FALSE;
      web_browser_events_svc->FireBeforeNavigate2Event(&should_cancel);
      web_browser_events_svc->FireNavigateComplete2Event();
      if (VARIANT_TRUE != should_cancel) {
        web_browser_events_svc->FireDocumentCompleteEvent();
      }
    }
  }
}

void ChromeActiveDocument::OnFindInPage() {
  TabProxy* tab = GetTabProxy();
  if (tab) {
    if (!find_dialog_.IsWindow()) {
      find_dialog_.Create(m_hWnd);
    }

    find_dialog_.ShowWindow(SW_SHOW);
  }
}

void ChromeActiveDocument::OnViewSource() {
  DCHECK(navigation_info_.url.is_valid());
  std::string url_to_open = "view-source:";
  url_to_open += navigation_info_.url.spec();
  OnOpenURL(0, GURL(url_to_open), NEW_WINDOW);
}

void ChromeActiveDocument::OnOpenURL(int tab_handle, const GURL& url_to_open,
                                     int open_disposition) {
  // If the disposition indicates that we should be opening the URL in the
  // current tab, then we can reuse the ChromeFrameAutomationClient instance
  // maintained by the current ChromeActiveDocument instance. We cache this
  // instance so that it can be used by the new ChromeActiveDocument instance
  // which may be instantiated for handling the new URL.
  if (open_disposition == CURRENT_TAB) {
    // Grab a reference to ensure that the document remains valid.
    AddRef();
    g_active_doc_cache.Set(this);
  }

  Base::OnOpenURL(tab_handle, url_to_open, open_disposition);
}

void ChromeActiveDocument::OnLoad(int tab_handle, const GURL& url) {
  if (ready_state_ < READYSTATE_COMPLETE) {
    ready_state_ = READYSTATE_COMPLETE;
    FireOnChanged(DISPID_READYSTATE);
  }
}

bool ChromeActiveDocument::PreProcessContextMenu(HMENU menu) {
  ScopedComPtr<IBrowserService> browser_service;
  ScopedComPtr<ITravelLog> travel_log;

  DoQueryService(SID_SShellBrowser, m_spClientSite, browser_service.Receive());
  if (!browser_service)
    return true;

  browser_service->GetTravelLog(travel_log.Receive());
  if (!travel_log)
    return true;

  if (SUCCEEDED(travel_log->GetTravelEntry(browser_service, TLOG_BACK, NULL))) {
    EnableMenuItem(menu, IDS_CONTENT_CONTEXT_BACK, MF_BYCOMMAND | MF_ENABLED);
  } else {
    EnableMenuItem(menu, IDS_CONTENT_CONTEXT_BACK, MF_BYCOMMAND | MFS_DISABLED);
  }


  if (SUCCEEDED(travel_log->GetTravelEntry(browser_service, TLOG_FORE, NULL))) {
    EnableMenuItem(menu, IDS_CONTENT_CONTEXT_FORWARD,
                   MF_BYCOMMAND | MF_ENABLED);
  } else {
    EnableMenuItem(menu, IDS_CONTENT_CONTEXT_FORWARD,
                   MF_BYCOMMAND | MFS_DISABLED);
  }

  // Call base class (adds 'About' item)
  return Base::PreProcessContextMenu(menu);
}

bool ChromeActiveDocument::HandleContextMenuCommand(UINT cmd) {
  ScopedComPtr<IWebBrowser2> web_browser2;
  DoQueryService(SID_SWebBrowserApp, m_spClientSite, web_browser2.Receive());

  switch (cmd) {
    case IDS_CONTENT_CONTEXT_BACK:
      web_browser2->GoBack();
      break;

    case IDS_CONTENT_CONTEXT_FORWARD:
      web_browser2->GoForward();
      break;

    case IDS_CONTENT_CONTEXT_RELOAD:
      web_browser2->Refresh();
      break;

    default:
      return Base::HandleContextMenuCommand(cmd);
  }

  return true;
}

HRESULT ChromeActiveDocument::IEExec(const GUID* cmd_group_guid,
                                     DWORD command_id, DWORD cmd_exec_opt,
                                     VARIANT* in_args, VARIANT* out_args) {
  HRESULT hr = E_FAIL;
  ScopedComPtr<IOleCommandTarget> frame_cmd_target;
  if (m_spInPlaceSite)
    hr = frame_cmd_target.QueryFrom(m_spInPlaceSite);

  if (frame_cmd_target)
    hr = frame_cmd_target->Exec(cmd_group_guid, command_id, cmd_exec_opt,
                                in_args, out_args);

  return hr;
}
