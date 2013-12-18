// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of ChromeActiveDocument
#include "chrome_frame/chrome_active_document.h"

#include <hlink.h>
#include <htiface.h>
#include <initguid.h>
#include <mshtmcid.h>
#include <shdeprecated.h>
#include <shlguid.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <tlogstg.h>
#include <urlmon.h>
#include <wininet.h>

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_variant.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/automation_messages.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome_frame/bho.h"
#include "chrome_frame/bind_context_info.h"
#include "chrome_frame/buggy_bho_handling.h"
#include "chrome_frame/crash_reporting/crash_metrics.h"
#include "chrome_frame/utils.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_zoom.h"
#include "grit/generated_resources.h"

DEFINE_GUID(CGID_DocHostCmdPriv, 0x000214D4L, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0,
            0x46);

bool g_first_launch_by_process_ = true;

const DWORD kIEEncodingIdArray[] = {
#define DEFINE_ENCODING_ID_ARRAY(encoding_name, id, chrome_name) encoding_name,
  INTERNAL_IE_ENCODINGMENU_IDS(DEFINE_ENCODING_ID_ARRAY)
#undef DEFINE_ENCODING_ID_ARRAY
  0  // The Last data must be 0 to indicate the end of the encoding id array.
};

ChromeActiveDocument::ChromeActiveDocument()
    : first_navigation_(true),
      is_automation_client_reused_(false),
      popup_allowed_(false),
      accelerator_table_(NULL) {
  TRACE_EVENT_BEGIN_ETW("chromeframe.createactivedocument", this, "");

  url_fetcher_->set_frame_busting(false);
}

HRESULT ChromeActiveDocument::FinalConstruct() {
  // The FinalConstruct implementation in the ChromeFrameActivexBase class
  // i.e. Base creates an instance of the ChromeFrameAutomationClient class
  // and initializes it, which would spawn a new Chrome process, etc.
  // We don't want to be doing this if we have a cached document, whose
  // automation client instance can be reused.
  HRESULT hr = BaseActiveX::FinalConstruct();
  if (FAILED(hr))
    return hr;

  InitializeAutomationSettings();

  find_dialog_.Init(automation_client_.get());

  OLECMDF flags = static_cast<OLECMDF>(OLECMDF_ENABLED | OLECMDF_SUPPORTED);

  null_group_commands_map_[OLECMDID_PRINT] = flags;
  null_group_commands_map_[OLECMDID_FIND] = flags;
  null_group_commands_map_[OLECMDID_CUT] = flags;
  null_group_commands_map_[OLECMDID_COPY] = flags;
  null_group_commands_map_[OLECMDID_PASTE] = flags;
  null_group_commands_map_[OLECMDID_SELECTALL] = flags;
  null_group_commands_map_[OLECMDID_SAVEAS] = flags;

  mshtml_group_commands_map_[IDM_BASELINEFONT1] = flags;
  mshtml_group_commands_map_[IDM_BASELINEFONT2] = flags;
  mshtml_group_commands_map_[IDM_BASELINEFONT3] = flags;
  mshtml_group_commands_map_[IDM_BASELINEFONT4] = flags;
  mshtml_group_commands_map_[IDM_BASELINEFONT5] = flags;
  mshtml_group_commands_map_[IDM_VIEWSOURCE] = flags;

  HMODULE this_module = reinterpret_cast<HMODULE>(&__ImageBase);
  accelerator_table_ =
    LoadAccelerators(this_module,
                     MAKEINTRESOURCE(IDR_CHROME_FRAME_IE_FULL_TAB));
  DCHECK(accelerator_table_ != NULL);
  return S_OK;
}

ChromeActiveDocument::~ChromeActiveDocument() {
  DVLOG(1) << __FUNCTION__;
  if (find_dialog_.IsWindow())
    find_dialog_.DestroyWindow();
  // ChromeFramePlugin
  BaseActiveX::Uninitialize();

  TRACE_EVENT_END_ETW("chromeframe.createactivedocument", this, "");
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
  if (OLEIVERB_INPLACEACTIVATE == verb)
    return OLEOBJ_E_INVALIDVERB;
  // Check if we should activate as a docobject or not
  // (client supports IOleDocumentSite)
  if (doc_site_) {
    switch (verb) {
    case OLEIVERB_SHOW: {
      base::win::ScopedComPtr<IDocHostUIHandler> doc_host_handler;
      doc_host_handler.QueryFrom(doc_site_);
      if (doc_host_handler.get())
        doc_host_handler->ShowUI(DOCHOSTUITYPE_BROWSE, this, this, NULL, NULL);
    }
    case OLEIVERB_OPEN:
    case OLEIVERB_UIACTIVATE:
      if (!m_bUIActive)
        return doc_site_->ActivateMe(NULL);
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

// Override IOleInPlaceActiveObjectImpl::OnDocWindowActivate
STDMETHODIMP ChromeActiveDocument::OnDocWindowActivate(BOOL activate) {
  DVLOG(1) << __FUNCTION__;
  return S_OK;
}

STDMETHODIMP ChromeActiveDocument::TranslateAccelerator(MSG* msg) {
  DVLOG(1) << __FUNCTION__;
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
  DVLOG(1) << __FUNCTION__;
  return S_FALSE;
}

void ChromeActiveDocument::OnAutomationServerReady() {
  BaseActiveX::OnAutomationServerReady();
}

STDMETHODIMP ChromeActiveDocument::Load(BOOL fully_avalable,
                                        IMoniker* moniker_name,
                                        LPBC bind_context,
                                        DWORD mode) {
  if (NULL == moniker_name)
    return E_INVALIDARG;

  base::win::ScopedComPtr<IOleClientSite> client_site;
  if (bind_context) {
    base::win::ScopedComPtr<IUnknown> site;
    bind_context->GetObjectParam(SZ_HTML_CLIENTSITE_OBJECTPARAM,
                                 site.Receive());
    if (site)
      client_site.QueryFrom(site);
  }

  if (client_site) {
    SetClientSite(client_site);
    DoQueryService(IID_INewWindowManager, client_site,
                   popup_manager_.Receive());

    // See if mshtml parsed the html header for us.  If so, we need to
    // clear the browser service flag that we use to indicate that this
    // browser instance is navigating to a CF document.
    base::win::ScopedComPtr<IBrowserService> browser_service;
    DoQueryService(SID_SShellBrowser, client_site, browser_service.Receive());
    if (browser_service) {
      bool flagged = CheckForCFNavigation(browser_service, true);
      DVLOG_IF(1, flagged) << "Cleared flagged browser service";
    }
  }

  NavigationManager* mgr = NavigationManager::GetThreadInstance();
  DLOG_IF(ERROR, !mgr) << "Couldn't get instance of NavigationManager";

  std::wstring url;

  base::win::ScopedComPtr<BindContextInfo> info;
  BindContextInfo::FromBindContext(bind_context, info.Receive());
  DCHECK(info);
  if (info && !info->GetUrl().empty()) {
    url = info->GetUrl();
    if (mgr) {
      // If the original URL contains an anchor, then the URL queried
      // from the protocol sink wrapper does not contain the anchor. To
      // workaround this we retrieve the anchor from the navigation manager
      // and append it to the url retrieved from the protocol sink wrapper.
      GURL url_for_anchor(mgr->url());
      if (url_for_anchor.has_ref()) {
        url += L"#";
        url += UTF8ToWide(url_for_anchor.ref());
      }
    }
  } else {
    // If the original URL contains an anchor, then the URL queried
    // from the moniker does not contain the anchor. To workaround
    // this we retrieve the URL from our BHO.
    url = GetActualUrlFromMoniker(moniker_name, bind_context,
                                  mgr ? mgr->url(): std::wstring());
  }

  ChromeFrameUrl cf_url;
  if (!cf_url.Parse(url)) {
    DLOG(WARNING) << __FUNCTION__ << " Failed to parse url:" << url;
    return E_INVALIDARG;
  }

  std::string referrer(mgr ? mgr->referrer() : std::string());
  RendererType renderer_type = cf_url.is_chrome_protocol() ?
      RENDERER_TYPE_CHROME_GCF_PROTOCOL : RENDERER_TYPE_UNDETERMINED;

  // With CTransaction patch we have more robust way to grab the referrer for
  // each top-level-switch-to-CF request by peeking at our sniffing data
  // object that lives inside the bind context.  We also remember the reason
  // we're rendering the document in Chrome.
  if (g_patch_helper.state() == PatchHelper::PATCH_PROTOCOL && info) {
    scoped_refptr<ProtData> prot_data = info->get_prot_data();
    if (prot_data) {
      referrer = prot_data->referrer();
      renderer_type = prot_data->renderer_type();
    }
  }

  // For gcf: URLs allow only about and view-source schemes to pass through for
  // further inspection.
  bool is_safe_scheme = cf_url.gurl().SchemeIs(chrome::kAboutScheme) ||
      cf_url.gurl().SchemeIs(content::kViewSourceScheme);
  if (cf_url.is_chrome_protocol() && !is_safe_scheme &&
      !GetConfigBool(false, kAllowUnsafeURLs)) {
    DLOG(ERROR) << __FUNCTION__ << " gcf: not allowed:" << url;
    return E_INVALIDARG;
  }

  if (!LaunchUrl(cf_url, referrer)) {
    DLOG(ERROR) << __FUNCTION__ << " Failed to launch url:" << url;
    return E_INVALIDARG;
  }

  if (!cf_url.is_chrome_protocol() && !cf_url.attach_to_external_tab())
    url_fetcher_->SetInfoForUrl(url.c_str(), moniker_name, bind_context);

  // Log a metric indicating why GCF is rendering in Chrome.
  // (Note: we only track the renderer type when using the CTransaction patch.
  // When the code for the browser service patch and for the moniker patch is
  // removed, this conditional can also go away.)
  if (RENDERER_TYPE_UNDETERMINED != renderer_type) {
    UMA_LAUNCH_TYPE_COUNT(renderer_type);
  }

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
  if (NULL == class_id)
    return E_POINTER;
  *class_id = GetObjectCLSID();
  return S_OK;
}

STDMETHODIMP ChromeActiveDocument::QueryStatus(const GUID* cmd_group_guid,
                                               ULONG number_of_commands,
                                               OLECMD commands[],
                                               OLECMDTEXT* command_text) {
  DVLOG(1) << __FUNCTION__;

  CommandStatusMap* command_map = NULL;

  if (cmd_group_guid) {
    if (IsEqualGUID(*cmd_group_guid, GUID_NULL)) {
      command_map = &null_group_commands_map_;
    } else if (IsEqualGUID(*cmd_group_guid, CGID_MSHTML)) {
      command_map = &mshtml_group_commands_map_;
    } else if (IsEqualGUID(*cmd_group_guid, CGID_Explorer)) {
      command_map = &explorer_group_commands_map_;
    } else if (IsEqualGUID(*cmd_group_guid, CGID_ShellDocView)) {
      command_map = &shdoc_view_group_commands_map_;
    }
  } else {
    command_map = &null_group_commands_map_;
  }

  if (!command_map) {
    DVLOG(1) << "unsupported command group: " << GuidToString(*cmd_group_guid);
    return OLECMDERR_E_NOTSUPPORTED;
  }

  for (ULONG command_index = 0; command_index < number_of_commands;
       command_index++) {
    DVLOG(1) << "Command id = " << commands[command_index].cmdID;
    CommandStatusMap::iterator index =
        command_map->find(commands[command_index].cmdID);
    if (index != command_map->end())
      commands[command_index].cmdf = index->second;
  }
  return S_OK;
}

STDMETHODIMP ChromeActiveDocument::Exec(const GUID* cmd_group_guid,
                                        DWORD command_id,
                                        DWORD cmd_exec_opt,
                                        VARIANT* in_args,
                                        VARIANT* out_args) {
  DVLOG(1) << __FUNCTION__ << " Cmd id =" << command_id;
  // Bail out if we have been uninitialized.
  if (automation_client_.get() && automation_client_->tab()) {
    return ProcessExecCommand(cmd_group_guid, command_id, cmd_exec_opt,
                              in_args, out_args);
  } else if (command_id == OLECMDID_REFRESH && cmd_group_guid == NULL) {
    // If the automation server has crashed and the user is refreshing the
    // page, let OnRefreshPage attempt to recover.
    OnRefreshPage(cmd_group_guid, command_id, cmd_exec_opt, in_args, out_args);
  }

  return OLECMDERR_E_NOTSUPPORTED;
}

STDMETHODIMP ChromeActiveDocument::LoadHistory(IStream* stream,
                                               IBindCtx* bind_context) {
  // Read notes in ChromeActiveDocument::SaveHistory
  DCHECK(stream);
  LARGE_INTEGER offset = {0};
  ULARGE_INTEGER cur_pos = {0};
  STATSTG statstg = {0};

  stream->Seek(offset, STREAM_SEEK_CUR, &cur_pos);
  stream->Stat(&statstg, STATFLAG_NONAME);

  DWORD url_size = statstg.cbSize.LowPart - cur_pos.LowPart;
  base::win::ScopedBstr url_bstr;
  DWORD bytes_read = 0;
  stream->Read(url_bstr.AllocateBytes(url_size), url_size, &bytes_read);
  std::wstring url(url_bstr);

  ChromeFrameUrl cf_url;
  if (!cf_url.Parse(url)) {
    DLOG(WARNING) << __FUNCTION__ << " Failed to parse url:" << url;
    return E_INVALIDARG;
  }

  if (!LaunchUrl(cf_url, std::string())) {
    NOTREACHED() << __FUNCTION__ << " Failed to launch url:" << url;
    return E_INVALIDARG;
  }
  return S_OK;
}

STDMETHODIMP ChromeActiveDocument::SaveHistory(IStream* stream) {
  return E_INVALIDARG;
}

STDMETHODIMP ChromeActiveDocument::SetPositionCookie(DWORD position_cookie) {
  return S_OK;
}

STDMETHODIMP ChromeActiveDocument::GetPositionCookie(DWORD* position_cookie) {
  return S_OK;
}

STDMETHODIMP ChromeActiveDocument::GetUrlForEvents(BSTR* url) {
  if (NULL == url)
    return E_POINTER;
  *url = ::SysAllocString(url_);
  return S_OK;
}

STDMETHODIMP ChromeActiveDocument::GetAddressBarUrl(BSTR* url) {
  return GetUrlForEvents(url);
}

STDMETHODIMP ChromeActiveDocument::Reset() {
  next_privacy_record_ = privacy_info_.privacy_records.begin();
  return S_OK;
}

STDMETHODIMP ChromeActiveDocument::GetSize(DWORD* size) {
  if (!size)
    return E_POINTER;

  *size = privacy_info_.privacy_records.size();
  return S_OK;
}

STDMETHODIMP ChromeActiveDocument::GetPrivacyImpacted(BOOL* privacy_impacted) {
  if (!privacy_impacted)
    return E_POINTER;

  *privacy_impacted = privacy_info_.privacy_impacted;
  return S_OK;
}

STDMETHODIMP ChromeActiveDocument::Next(BSTR* url, BSTR* policy,
                                        LONG* reserved, DWORD* flags) {
  if (!url || !policy || !flags)
    return E_POINTER;

  if (next_privacy_record_ == privacy_info_.privacy_records.end())
    return HRESULT_FROM_WIN32(ERROR_NO_MORE_ITEMS);

  *url = SysAllocString(next_privacy_record_->first.c_str());
  *policy = SysAllocString(next_privacy_record_->second.policy_ref.c_str());
  *flags = next_privacy_record_->second.flags;

  next_privacy_record_++;
  return S_OK;
}

bool ChromeActiveDocument::IsSchemeAllowed(const GURL& url) {
  bool allowed = BaseActiveX::IsSchemeAllowed(url);
  if (allowed)
    return true;

  if (url.SchemeIs(chrome::kAboutScheme)) {
    if (LowerCaseEqualsASCII(url.spec(), chrome::kAboutPluginsURL))
      return true;
  }
  return false;
}

HRESULT ChromeActiveDocument::GetInPlaceFrame(
    IOleInPlaceFrame** in_place_frame) {
  DCHECK(in_place_frame);
  if (in_place_frame_) {
    *in_place_frame = in_place_frame_.get();
    (*in_place_frame)->AddRef();
    return S_OK;
  } else {
    return S_FALSE;
  }
}

HRESULT ChromeActiveDocument::IOleObject_SetClientSite(
    IOleClientSite* client_site) {
  if (client_site == NULL) {
    base::win::ScopedComPtr<IDocHostUIHandler> doc_host_handler;
    if (doc_site_)
      doc_host_handler.QueryFrom(doc_site_);

    if (doc_host_handler.get())
      doc_host_handler->HideUI();

    doc_site_.Release();
  }

  if (client_site != m_spClientSite)
    return BaseActiveX::IOleObject_SetClientSite(client_site);

  return S_OK;
}

HRESULT ChromeActiveDocument::ActiveXDocActivate(LONG verb) {
  HRESULT hr = S_OK;
  m_bNegotiatedWnd = TRUE;
  if (!m_bInPlaceActive) {
    hr = m_spInPlaceSite->CanInPlaceActivate();
    if (FAILED(hr))
      return hr;
    m_spInPlaceSite->OnInPlaceActivate();
  }
  m_bInPlaceActive = TRUE;
  // get location in the parent window,
  // as well as some information about the parent
  base::win::ScopedComPtr<IOleInPlaceUIWindow> in_place_ui_window;
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
        if (!IsWindow()) {
          // This might happen if the automation server couldn't be
          // instantiated.  If so, a NOTREACHED() will have already been hit.
          DLOG(ERROR) << "Failed to create Ax window";
          return AtlHresultFromLastError();
        }
      }
    }
    SetObjectRects(&position_rect, &clip_rect);
  }

  base::win::ScopedComPtr<IOleInPlaceActiveObject> in_place_active_object(this);

  // Gone active by now, take care of UIACTIVATE
  if (DoesVerbUIActivate(verb)) {
    if (!m_bUIActive) {
      m_bUIActive = TRUE;
      hr = m_spInPlaceSite->OnUIActivate();
      if (FAILED(hr))
        return hr;
      // set ourselves up in the host
      if (in_place_active_object) {
        if (in_place_frame_)
          in_place_frame_->SetActiveObject(in_place_active_object, NULL);
        if (in_place_ui_window)
          in_place_ui_window->SetActiveObject(in_place_active_object, NULL);
      }
    }
  }
  m_spClientSite->ShowObject();
  return S_OK;
}

bool IsFindAccelerator(const MSG& msg) {
  // TODO(robertshield): This may not stand up to localization. Fix if this
  // is the case.
  return msg.message == WM_KEYDOWN && msg.wParam == 'F' &&
         base::win::IsCtrlPressed() &&
         !(base::win::IsAltPressed() || base::win::IsShiftPressed());
}

void ChromeActiveDocument::OnFindInPage() {
  TabProxy* tab = GetTabProxy();
  if (tab) {
    if (!find_dialog_.IsWindow())
      find_dialog_.Create(m_hWnd);

    find_dialog_.ShowWindow(SW_SHOW);
  }
}

void ChromeActiveDocument::OnViewSource() {
}

void ChromeActiveDocument::OnDetermineSecurityZone(const GUID* cmd_group_guid,
                                                   DWORD command_id,
                                                   DWORD cmd_exec_opt,
                                                   VARIANT* in_args,
                                                   VARIANT* out_args) {
  // Always return URLZONE_INTERNET since that is the Chrome's behaviour.
  // Correct step is to use MapUrlToZone().
  if (out_args != NULL) {
    out_args->vt = VT_UI4;
    out_args->ulVal = URLZONE_INTERNET;
  }
}

void ChromeActiveDocument::OnDisplayPrivacyInfo() {
  privacy_info_ = url_fetcher_->privacy_info();
  Reset();
  DoPrivacyDlg(m_hWnd, url_, this, TRUE);
}

void ChromeActiveDocument::OnGetZoomRange(const GUID* cmd_group_guid,
                                          DWORD command_id,
                                          DWORD cmd_exec_opt,
                                          VARIANT* in_args,
                                          VARIANT* out_args) {
  if (out_args != NULL) {
    out_args->vt = VT_I4;
    out_args->lVal = 0;
  }
}

bool ChromeActiveDocument::PreProcessContextMenu(HMENU menu) {
  base::win::ScopedComPtr<IBrowserService> browser_service;
  base::win::ScopedComPtr<ITravelLog> travel_log;
  GetBrowserServiceAndTravelLog(browser_service.Receive(),
                                travel_log.Receive());
  if (!browser_service || !travel_log)
    return true;

  EnableMenuItem(menu, IDC_BACK, MF_BYCOMMAND |
      (SUCCEEDED(travel_log->GetTravelEntry(browser_service, TLOG_BACK,
                                            NULL)) ?
      MF_ENABLED : MF_DISABLED));
  EnableMenuItem(menu, IDC_FORWARD, MF_BYCOMMAND |
      (SUCCEEDED(travel_log->GetTravelEntry(browser_service, TLOG_FORE,
                                            NULL)) ?
      MF_ENABLED : MF_DISABLED));
  // Call base class (adds 'About' item)
  return BaseActiveX::PreProcessContextMenu(menu);
}

HRESULT ChromeActiveDocument::IEExec(const GUID* cmd_group_guid,
                                     DWORD command_id, DWORD cmd_exec_opt,
                                     VARIANT* in_args, VARIANT* out_args) {
  HRESULT hr = E_FAIL;

  base::win::ScopedComPtr<IOleCommandTarget> frame_cmd_target;

  base::win::ScopedComPtr<IOleInPlaceSite> in_place_site(m_spInPlaceSite);
  if (!in_place_site.get() && m_spClientSite != NULL)
    in_place_site.QueryFrom(m_spClientSite);

  if (in_place_site)
    hr = frame_cmd_target.QueryFrom(in_place_site);

  if (frame_cmd_target) {
    hr = frame_cmd_target->Exec(cmd_group_guid, command_id, cmd_exec_opt,
                                in_args, out_args);
  }

  return hr;
}

bool ChromeActiveDocument::LaunchUrl(const ChromeFrameUrl& cf_url,
                                     const std::string& referrer) {
  return false;
}

HRESULT ChromeActiveDocument::OnRefreshPage(const GUID* cmd_group_guid,
    DWORD command_id, DWORD cmd_exec_opt, VARIANT* in_args, VARIANT* out_args) {
  DVLOG(1) << __FUNCTION__;
  popup_allowed_ = false;
  if (in_args->vt == VT_I4 &&
      in_args->lVal & OLECMDIDF_REFRESH_PAGEACTION_POPUPWINDOW) {
    popup_allowed_ = true;

    // Ask the yellow security band to change the text and icon and to remain
    // visible.
    IEExec(&CGID_DocHostCommandHandler, OLECMDID_PAGEACTIONBLOCKED,
        0x80000000 | OLECMDIDF_WINDOWSTATE_USERVISIBLE_VALID, NULL, NULL);
  }

  NavigationManager* mgr = NavigationManager::GetThreadInstance();
  DLOG_IF(ERROR, !mgr) << "Couldn't get instance of NavigationManager";

  // If ChromeFrame was activated on this page as a result of a document
  // received in response to a top level post, then we ask the user for
  // permission to repost and issue a navigation with the saved post data
  // which reinitates the whole sequence, i.e. the server receives the top
  // level post and chrome frame will be reactivated in response.
  if (mgr && mgr->post_data().type() != VT_EMPTY) {
    if (MessageBox(
            SimpleResourceLoader::Get(IDS_HTTP_POST_WARNING).c_str(),
            SimpleResourceLoader::Get(IDS_HTTP_POST_WARNING_TITLE).c_str(),
            MB_YESNO | MB_ICONEXCLAMATION) == IDYES) {
      base::win::ScopedComPtr<IWebBrowser2> web_browser2;
      DoQueryService(SID_SWebBrowserApp, m_spClientSite,
                     web_browser2.Receive());
      DCHECK(web_browser2);
      VARIANT empty = base::win::ScopedVariant::kEmptyVariant;
      VARIANT flags = { VT_I4 };
      V_I4(&flags) = navNoHistory;

      return web_browser2->Navigate2(base::win::ScopedVariant(url_).AsInput(),
                                     &flags,
                                     &empty,
                                     const_cast<VARIANT*>(&mgr->post_data()),
                                     const_cast<VARIANT*>(&mgr->headers()));
    } else {
      return S_OK;
    }
  }

  TabProxy* tab_proxy = GetTabProxy();
  if (tab_proxy) {
    tab_proxy->ReloadAsync();
  }

  return S_OK;
}

HRESULT ChromeActiveDocument::SetPageFontSize(const GUID* cmd_group_guid,
                                              DWORD command_id,
                                              DWORD cmd_exec_opt,
                                              VARIANT* in_args,
                                              VARIANT* out_args) {
  if (!automation_client_.get()) {
    NOTREACHED() << "Invalid automation client";
    return E_FAIL;
  }

  switch (command_id) {
    case IDM_BASELINEFONT1:
      automation_client_->SetPageFontSize(SMALLEST_FONT);
      break;

    case IDM_BASELINEFONT2:
      automation_client_->SetPageFontSize(SMALL_FONT);
      break;

    case IDM_BASELINEFONT3:
      automation_client_->SetPageFontSize(MEDIUM_FONT);
      break;

    case IDM_BASELINEFONT4:
      automation_client_->SetPageFontSize(LARGE_FONT);
      break;

    case IDM_BASELINEFONT5:
      automation_client_->SetPageFontSize(LARGEST_FONT);
      break;

    default:
      NOTREACHED() << "Invalid font size command: "
                  << command_id;
      return E_FAIL;
  }

  // Forward the command back to IEFrame with group set to
  // CGID_ExplorerBarDoc. This is probably needed to update the menu state to
  // indicate that the font size was set. This currently fails with error
  // 0x80040104.
  // TODO(iyengar)
  // Do some investigation into why this Exec call fails.
  IEExec(&CGID_ExplorerBarDoc, command_id, cmd_exec_opt, NULL, NULL);
  return S_OK;
}

HRESULT ChromeActiveDocument::OnEncodingChange(const GUID* cmd_group_guid,
                                               DWORD command_id,
                                               DWORD cmd_exec_opt,
                                               VARIANT* in_args,
                                               VARIANT* out_args) {
  const struct EncodingMapData {
    DWORD ie_encoding_id;
    const char* chrome_encoding_name;
  } kEncodingTestDatas[] = {
#define DEFINE_ENCODING_MAP(encoding_name, id, chrome_name) \
    { encoding_name, chrome_name },
  INTERNAL_IE_ENCODINGMENU_IDS(DEFINE_ENCODING_MAP)
#undef DEFINE_ENCODING_MAP
  };

  if (!automation_client_.get()) {
    NOTREACHED() << "Invalid automtion client";
    return E_FAIL;
  }

  // Using ARRAYSIZE_UNSAFE in here is because we define the struct
  // EncodingMapData inside function.
  const char* chrome_encoding_name = NULL;
  for (int i = 0; i < ARRAYSIZE_UNSAFE(kEncodingTestDatas); ++i) {
    const struct EncodingMapData* encoding_data = &kEncodingTestDatas[i];
    if (command_id == encoding_data->ie_encoding_id) {
      chrome_encoding_name = encoding_data->chrome_encoding_name;
      break;
    }
  }
  // Return E_FAIL when encountering invalid encoding id.
  if (!chrome_encoding_name)
    return E_FAIL;

  TabProxy* tab = GetTabProxy();
  if (!tab) {
    NOTREACHED() << "Can not get TabProxy";
    return E_FAIL;
  }

  if (chrome_encoding_name)
    tab->OverrideEncoding(chrome_encoding_name);

  // Like we did on SetPageFontSize, we may forward the command back to IEFrame
  // to update the menu state to indicate that which encoding was set.
  // TODO(iyengar)
  // Do some investigation into why this Exec call fails.
  IEExec(&CGID_ExplorerBarDoc, command_id, cmd_exec_opt, NULL, NULL);
  return S_OK;
}
HRESULT ChromeActiveDocument::GetBrowserServiceAndTravelLog(
    IBrowserService** browser_service, ITravelLog** travel_log) {
  DCHECK(browser_service || travel_log);
  base::win::ScopedComPtr<IBrowserService> browser_service_local;
  HRESULT hr = DoQueryService(SID_SShellBrowser, m_spClientSite,
                              browser_service_local.Receive());
  if (!browser_service_local) {
    NOTREACHED() << "DoQueryService for IBrowserService failed: " << hr;
    return hr;
  }

  if (travel_log) {
    hr = browser_service_local->GetTravelLog(travel_log);
    DVLOG_IF(1, !travel_log) << "browser_service->GetTravelLog failed: " << hr;
  }

  if (browser_service)
    *browser_service = browser_service_local.Detach();

  return hr;
}

LRESULT ChromeActiveDocument::OnForward(WORD notify_code, WORD id,
                                        HWND control_window,
                                        BOOL& bHandled) {
  base::win::ScopedComPtr<IWebBrowser2> web_browser2;
  DoQueryService(SID_SWebBrowserApp, m_spClientSite, web_browser2.Receive());
  DCHECK(web_browser2);

  if (web_browser2)
    web_browser2->GoForward();
  return 0;
}

LRESULT ChromeActiveDocument::OnBack(WORD notify_code, WORD id,
                                     HWND control_window,
                                     BOOL& bHandled) {
  base::win::ScopedComPtr<IWebBrowser2> web_browser2;
  DoQueryService(SID_SWebBrowserApp, m_spClientSite, web_browser2.Receive());
  DCHECK(web_browser2);

  if (web_browser2)
    web_browser2->GoBack();
  return 0;
}

LRESULT ChromeActiveDocument::OnFirePrivacyChange(UINT message, WPARAM wparam,
                                                  LPARAM lparam,
                                                  BOOL& handled) {
  if (!m_spClientSite)
    return 0;

  base::win::ScopedComPtr<IWebBrowser2> web_browser2;
  DoQueryService(SID_SWebBrowserApp, m_spClientSite,
                 web_browser2.Receive());
  if (!web_browser2) {
    NOTREACHED() << "Failed to retrieve IWebBrowser2 interface.";
    return 0;
  }

  base::win::ScopedComPtr<IShellBrowser> shell_browser;
  DoQueryService(SID_STopLevelBrowser, web_browser2,
                 shell_browser.Receive());
  DCHECK(shell_browser.get() != NULL);
  base::win::ScopedComPtr<ITridentService2> trident_services;
  trident_services.QueryFrom(shell_browser);
  if (trident_services)
    trident_services->FirePrivacyImpactedStateChange(wparam);
  else
    NOTREACHED() << "Failed to retrieve IWebBrowser2 interface.";
  return 0;
}

LRESULT ChromeActiveDocument::OnShowWindow(UINT message, WPARAM wparam,
                                           LPARAM lparam,
                                           BOOL& handled) {  // NO_LINT
  if (wparam)
    SetFocus();
  return 0;
}

LRESULT ChromeActiveDocument::OnSetFocus(UINT message, WPARAM wparam,
                                         LPARAM lparam,
                                         BOOL& handled) {  // NO_LINT
  return 0;
}
