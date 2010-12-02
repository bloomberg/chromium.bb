// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// IE toolband implementation.
#include "ceee/ie/plugin/toolband/tool_band.h"

#include <atlsafe.h>
#include <atlstr.h>
#include <shlguid.h>

#include "base/debug/trace_event.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "ceee/common/com_utils.h"
#include "ceee/common/window_utils.h"
#include "ceee/common/windows_constants.h"
#include "ceee/ie/common/extension_manifest.h"
#include "ceee/ie/common/ceee_module_util.h"
#include "ceee/ie/plugin/bho/browser_helper_object.h"
#include "ceee/ie/plugin/bho/tool_band_visibility.h"
#include "chrome/common/automation_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome_frame/com_message_event.h"

namespace {

// TODO(cindylau@chromium.org): The dimensions should not be hardcoded here.
// We should receive UI size info from extensions in the future, if possible.
const int kToolBandMinWidth = 64;
const int kToolBandHeight = 31;

// Define the type of SHGetPropertyStoreForWindow is SHGPSFW.
typedef DECLSPEC_IMPORT HRESULT (STDAPICALLTYPE *SHGPSFW)(HWND hwnd,
                                                          REFIID riid,
                                                          void** ppv);
}  // namespace

_ATL_FUNC_INFO ToolBand::handler_type_idispatch_ =
    { CC_STDCALL, VT_EMPTY, 1, { VT_DISPATCH } };
_ATL_FUNC_INFO ToolBand::handler_type_long_ =
    { CC_STDCALL, VT_EMPTY, 1, { VT_I4 } };
_ATL_FUNC_INFO ToolBand::handler_type_idispatch_bstr_ =
    { CC_STDCALL, VT_EMPTY, 2, { VT_DISPATCH, VT_BSTR } };
_ATL_FUNC_INFO ToolBand::handler_type_bstr_i4_=
    { CC_STDCALL, VT_EMPTY, 2, { VT_BSTR, VT_I4 } };
_ATL_FUNC_INFO ToolBand::handler_type_bstrarray_=
    { CC_STDCALL, VT_EMPTY, 1, { VT_ARRAY | VT_BSTR } };
_ATL_FUNC_INFO ToolBand::handler_type_idispatch_variantref_ =
    { CC_STDCALL, VT_EMPTY, 2, { VT_DISPATCH, VT_VARIANT | VT_BYREF } };

ToolBand::ToolBand()
    : already_tried_installing_(false),
      own_line_flag_(false),
      already_checked_own_line_flag_(false),
      listening_to_browser_events_(false),
      band_id_(0),
      is_quitting_(false),
      already_sent_id_to_bho_(false),
      current_width_(kToolBandMinWidth),
      current_height_(kToolBandHeight) {
  TRACE_EVENT_BEGIN("ceee.toolband", this, "");
}

ToolBand::~ToolBand() {
  TRACE_EVENT_END("ceee.toolband", this, "");
}

HRESULT ToolBand::FinalConstruct() {
  return S_OK;
}

void ToolBand::FinalRelease() {
}

STDMETHODIMP ToolBand::SetSite(IUnknown* site) {
  typedef IObjectWithSiteImpl<ToolBand> SuperSite;

  // From experience we know the site may be set multiple times.
  // Let's ignore second and subsequent set or unset.
  if (NULL != site && NULL != m_spUnkSite.p ||
      NULL == site && NULL == m_spUnkSite.p) {
    // TODO(siggi@chromium.org) log this.
    return S_OK;
  }

  if (NULL == site) {
    // We're being torn down.
    Teardown();
  }

  HRESULT hr = SuperSite::SetSite(site);
  if (FAILED(hr))
    return hr;

  if (NULL != site) {
    // We're being initialized.
    hr = Initialize(site);

    // Release the site in case of failure.
    if (FAILED(hr))
      SuperSite::SetSite(NULL);
  }

  return hr;
}

STDMETHODIMP ToolBand::ShowDW(BOOL show) {
  ShowWindow(show ? SW_SHOW : SW_HIDE);
  if (show) {
    // Report that the toolband is being shown, so that the BHO
    // knows it doesn't need to explicitly make it visible.
    ToolBandVisibility::ReportToolBandVisible(web_browser_);
  }
  // Unless ShowDW changes are explicitly being ignored (e.g. if the
  // BHO is forcing the toolband to be visible via
  // ShowBrowserBar), or unless the toolband is closing on quit, then we assume
  // a call to ShowDW reflects the user's toolband visibility choice, modifiable
  // through the View -> Toolbars menu in IE. We track this choice here.
  if (!ceee_module_util::GetIgnoreShowDWChanges() && !is_quitting_) {
    ceee_module_util::SetOptionToolbandIsHidden(show == FALSE);
  }
  return S_OK;
}

STDMETHODIMP ToolBand::CloseDW(DWORD reserved) {
  // Indicates to ShowDW() that the tool band is being closed, as opposed to
  // being explicitly hidden by the user.
  is_quitting_ = true;
  return ShowDW(FALSE);
}

STDMETHODIMP ToolBand::ResizeBorderDW(LPCRECT border,
                                      IUnknown* toolband_site,
                                      BOOL reserved) {
  DCHECK(FALSE);  // Not used for toolbands.
  return E_NOTIMPL;
}

STDMETHODIMP ToolBand::GetBandInfo(DWORD band_id,
                                   DWORD view_mode,
                                   DESKBANDINFO* deskband_info) {
  band_id_ = band_id;

  // We're only registered as a horizontal band.
  DCHECK(view_mode == DBIF_VIEWMODE_NORMAL);

  if (!deskband_info)
    return E_POINTER;

  if (deskband_info->dwMask & DBIM_MINSIZE) {
    deskband_info->ptMinSize.x = current_width_;
    deskband_info->ptMinSize.y = current_height_;
  }

  if (deskband_info->dwMask & DBIM_MAXSIZE) {
    deskband_info->ptMaxSize.x = -1;
    deskband_info->ptMaxSize.y = -1;
  }

  if (deskband_info->dwMask & DBIM_INTEGRAL) {
    deskband_info->ptIntegral.x = 1;
    deskband_info->ptIntegral.y = 1;
  }

  if (deskband_info->dwMask & DBIM_ACTUAL) {
    // By not setting, we just use the default.
    // deskband_info->ptActual.x = 7000;
    deskband_info->ptActual.y = current_height_;
  }

  if (deskband_info->dwMask & DBIM_TITLE) {
    // Title is empty.
    deskband_info->wszTitle[0] = 0;
  }

  if (deskband_info->dwMask & DBIM_MODEFLAGS) {
    deskband_info->dwModeFlags = DBIMF_NORMAL /* | DBIMF_TOPALIGN */;

    if (ShouldForceOwnLine()) {
      deskband_info->dwModeFlags |= DBIMF_BREAK;
    }
  }

  if (deskband_info->dwMask & DBIM_BKCOLOR) {
    // Use the default background color by removing this flag.
    deskband_info->dwMask &= ~DBIM_BKCOLOR;
  }
  return S_OK;
}

STDMETHODIMP ToolBand::GetWindow(HWND* window) {
  *window = m_hWnd;
  return S_OK;
}

STDMETHODIMP ToolBand::ContextSensitiveHelp(BOOL enter_mode) {
  LOG(INFO) << "ContextSensitiveHelp";
  return E_NOTIMPL;
}

STDMETHODIMP ToolBand::GetClassID(CLSID* clsid) {
  *clsid = GetObjectCLSID();
  return S_OK;
}

STDMETHODIMP ToolBand::IsDirty() {
  return S_FALSE;  // Never dirty for now.
}

STDMETHODIMP ToolBand::Load(IStream* stream) {
  return S_OK;  // Loading is no-op.
}

STDMETHODIMP ToolBand::Save(IStream* stream, BOOL clear_dirty) {
  return S_OK;  // Saving is no-op.
}

STDMETHODIMP ToolBand::GetSizeMax(ULARGE_INTEGER* size) {
  size->QuadPart = 0ULL;  // We're frugal.
  return S_OK;
}

STDMETHODIMP ToolBand::GetWantsPrivileged(boolean* wants_privileged) {
  *wants_privileged = true;
  return S_OK;
}

STDMETHODIMP ToolBand::GetChromeProfileName(BSTR* profile_name) {
  *profile_name = ::SysAllocString(
      ceee_module_util::GetBrokerProfileNameForIe());
  return S_OK;
}

STDMETHODIMP ToolBand::GetExtensionApisToAutomate(BSTR* functions_enabled) {
  *functions_enabled = NULL;
  return S_FALSE;
}

STDMETHODIMP ToolBand::ShouldShowVersionMismatchDialog() {
  // Only the toolband shows the warning dialog, meaning it gets shown once
  // per tab.
  return S_OK;
}

HRESULT ToolBand::Initialize(IUnknown* site) {
  TRACE_EVENT_INSTANT("ceee.toolband.initialize", this, "");

  CComQIPtr<IServiceProvider> service_provider = site;
  DCHECK(service_provider);
  if (service_provider == NULL) {
    return E_FAIL;
  }

  HRESULT hr = InitializeAndShowWindow(site);

  if (FAILED(hr)) {
    LOG(ERROR) << "Toolband failed to initalize its site window: " <<
                  com::LogHr(hr);
    return hr;
  }

  // Store the web browser, used to report toolband visibility to
  // the BHO. Also required to get navigate2 notification.
  hr = service_provider->QueryService(
      SID_SWebBrowserApp, IID_IWebBrowser2,
      reinterpret_cast<void**>(web_browser_.Receive()));

  DCHECK(SUCCEEDED(hr));

  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get web browser: 0x" << std::hex << hr;
    return hr;
  } else if (ShouldForceOwnLine()) {
    // This may seem odd, but event subscription is required
    // only to clear 'own line' flag later (see OnIeNavigateComplete2)
    hr = HostingBrowserEvents::DispEventAdvise(web_browser_,
                                               &DIID_DWebBrowserEvents2);
    listening_to_browser_events_ = SUCCEEDED(hr);
    DCHECK(SUCCEEDED(hr)) <<
      "DispEventAdvise on web browser failed. Error: " << hr;
    // Non-critical functionality. If fails in the field, just move on.
  }

  return S_OK;
}

HRESULT ToolBand::InitializeAndShowWindow(IUnknown* site) {
  ScopedComPtr<IOleWindow> site_window;
  HRESULT hr = site_window.QueryFrom(site);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get site window: " << com::LogHr(hr);
    return hr;
  }

  DCHECK(NULL != site_window.get());
  hr = site_window->GetWindow(&parent_window_.m_hWnd);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get parent window handle: " << com::LogHr(hr);
    return hr;
  }
  DCHECK(parent_window_);
  if (!parent_window_)
    return E_FAIL;

  if (NULL == Create(parent_window_))
    return E_FAIL;

  BOOL shown = ShowWindow(SW_SHOW);
  DCHECK(shown);

  return hr;
}

HRESULT ToolBand::Teardown() {
  TRACE_EVENT_INSTANT("ceee.toolband.teardown", this, "");

  if (IsWindow()) {
    // Teardown the ActiveX host window.
    CAxWindow host(m_hWnd);
    ScopedComPtr<IObjectWithSite> host_with_site;
    HRESULT hr = host.QueryHost(host_with_site.Receive());
    if (SUCCEEDED(hr))
      host_with_site->SetSite(NULL);

    DestroyWindow();
  }

  if (chrome_frame_) {
    ChromeFrameEvents::DispEventUnadvise(chrome_frame_);
  }
  chrome_frame_window_ = NULL;

  if (web_browser_ && listening_to_browser_events_) {
    HostingBrowserEvents::DispEventUnadvise(web_browser_,
                                            &DIID_DWebBrowserEvents2);
  }
  listening_to_browser_events_ = false;

  return S_OK;
}

void ToolBand::OnFinalMessage(HWND window) {
  GetUnknown()->Release();
}

LRESULT ToolBand::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  // Grab a self-reference.
  GetUnknown()->AddRef();

  // Create a host window instance.
  ScopedComPtr<IAxWinHostWindow> host;
  HRESULT hr = CAxHostWindow::CreateInstance(host.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create ActiveX host window. " << com::LogHr(hr);
    return 1;
  }

  // We're the site for the host window, this needs to be in place
  // before we attach ChromeFrame to the ActiveX control window, so
  // as to allow it to probe our service provider.
  hr = SetChildSite(host);
  DCHECK(SUCCEEDED(hr));

  // Create the chrome frame instance.
  hr = chrome_frame_.CoCreateInstance(L"ChromeTab.ChromeFrame");
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create the Chrome Frame instance. " <<
        com::LogHr(hr);
    return 1;
  }

  // And attach it to our window.
  hr = host->AttachControl(chrome_frame_, m_hWnd);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to attach Chrome Frame to the host. " <<
        com::LogHr(hr);
    return 1;
  }

  // Get the GCF window and hide it for now.
  CComQIPtr<IOleWindow> ole_window(chrome_frame_);
  DCHECK(ole_window != NULL);
  if (SUCCEEDED(ole_window->GetWindow(&chrome_frame_window_.m_hWnd))) {
    // We hide the chrome frame window until onload in order to avoid
    // seeing the "Aw Snap" that sometimes otherwise occurs during Chrome
    // initialization.
    chrome_frame_window_.ShowWindow(SW_HIDE);
  }

  // Hook up the chrome frame event listener.
  hr = ChromeFrameEvents::DispEventAdvise(chrome_frame_);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to hook up event sink. " << com::LogHr(hr);
  }

  return 0;
}

void ToolBand::OnPaint(CDCHandle dc) {
  RECT rc = {};
  if (GetUpdateRect(&rc, FALSE)) {
    PAINTSTRUCT ps = {};
    BeginPaint(&ps);

    BOOL ret = GetClientRect(&rc);
    DCHECK(ret);
    CString text;
    text.Format(L"Google CEEE. No Chrome Frame found. Instance: 0x%p. ID: %d!)",
                this, band_id_);
    ::DrawText(ps.hdc, text, -1, &rc, DT_SINGLELINE | DT_BOTTOM | DT_CENTER);

    EndPaint(&ps);
  }
}

void ToolBand::OnSize(UINT type, CSize size) {
  LOG(INFO) << "ToolBand::OnSize(" << type << ", "
      << size.cx << "x" << size.cy << ")";
  CWindow chrome_window = ::GetWindow(m_hWnd, GW_CHILD);
  if (!chrome_window) {
    LOG(ERROR) << "Failed to retrieve Chrome Frame window";
    return;
  }

  BOOL resized = chrome_window.ResizeClient(size.cx, size.cy);
  DCHECK(resized);
}

STDMETHODIMP_(void) ToolBand::OnCfReadyStateChanged(LONG state) {
  DLOG(INFO) << "OnCfReadyStateChanged(" << state << ")";

  if (state == READYSTATE_COMPLETE) {
    extension_path_ = ceee_module_util::GetExtensionPath();

    if (ceee_module_util::IsCrxOrEmpty(extension_path_) &&
        ceee_module_util::NeedToInstallExtension()) {
      LOG(INFO) << "Installing extension: \"" << extension_path_ << "\"";
      chrome_frame_->installExtension(CComBSTR(extension_path_.c_str()));
    } else {
      // In the case where we don't have a CRX (or we don't need to install it),
      // we must ask for the currently enabled extension before we can decide
      // what we need to do.
      chrome_frame_->getEnabledExtensions();
    }
  }
}

STDMETHODIMP_(void) ToolBand::OnCfMessage(IDispatch* event) {
  CComVariant origin;
  HRESULT hr = event->Invoke(ComMessageEvent::DISPID_MESSAGE_EVENT_ORIGIN,
      IID_NULL, 0, DISPATCH_PROPERTYGET, 0, &origin, 0, 0);
  if (FAILED(hr) || origin.vt != VT_BSTR) {
    DLOG(WARNING) << __FUNCTION__ << ": unable to discern message origin.";
    return;
  }

  CComVariant data_bstr;
  hr = event->Invoke(ComMessageEvent::DISPID_MESSAGE_EVENT_DATA,
      IID_NULL, 0, DISPATCH_PROPERTYGET, 0, &data_bstr, 0, 0);
  if (FAILED(hr) || data_bstr.vt != VT_BSTR) {
    DLOG(INFO) << __FUNCTION__ << ": no message data. Origin:"
        << origin.bstrVal;
    return;
  }
  DLOG(INFO) << __FUNCTION__ << ": Origin: " << origin.bstrVal
      << ", Data: " << data_bstr.bstrVal;
  CString data(data_bstr);

  // Handle CEEE-specific messages.
  // TODO(skare@google.com): If we will need this for more than one
  // message, consider making responses proper JSON.

  // ceee_getCurrentWindowId: chrome.windows.getCurrent workaround.
  CString message;
  if (data == L"ceee_getCurrentWindowId") {
    HWND browser_window = 0;
    web_browser_->get_HWND(reinterpret_cast<long*>(&browser_window));
    bool is_ieframe = window_utils::IsWindowClass(browser_window,
      windows::kIeFrameWindowClass);
    if (is_ieframe) {
      message.Format(L"ceee_getCurrentWindowId %d", browser_window);
    } else {
      DCHECK(is_ieframe);
      LOG(WARNING) << "Could not find IE Frame window.";
      message = L"ceee_getCurrentWindowId -1";
    }
  }

  if (!message.IsEmpty()) {
    chrome_frame_->postMessage(CComBSTR(message), origin);
  }
}

void ToolBand::StartExtension(const wchar_t* base_dir) {
  if (!LoadManifestFile(base_dir, &extension_url_)) {
    LOG(ERROR) << "No extension found";
  } else {
    HRESULT hr = chrome_frame_->put_src(CComBSTR(extension_url_.c_str()));
    DCHECK(SUCCEEDED(hr));
    LOG_IF(WARNING, FAILED(hr)) << "IChromeFrame::put_src returned: " <<
        com::LogHr(hr);
  }
}

STDMETHODIMP_(void) ToolBand::OnCfExtensionReady(BSTR path, int response) {
  TRACE_EVENT_INSTANT("ceee.toolband.oncfextensionready", this, "");

  if (ceee_module_util::IsCrxOrEmpty(extension_path_)) {
    // If we get here, it's because we just did the first-time
    // install, so save the installation path+time for future comparison.
    ceee_module_util::SetInstalledExtensionPath(
        FilePath(extension_path_));
  }

  // Now list enabled extensions so that we can properly start it whether
  // it's a CRX file or an exploded folder.
  //
  // Note that we do this even if Chrome says installation failed,
  // as that is the error code it uses when we try to install an
  // older version of the extension than it already has, which happens
  // on overinstall when Chrome has already auto-updated.
  //
  // If it turns out no extension is installed, we will handle that
  // error in the OnCfGetEnabledExtensionsComplete callback.
  chrome_frame_->getEnabledExtensions();
}

STDMETHODIMP_(void) ToolBand::OnCfGetEnabledExtensionsComplete(
    SAFEARRAY* extension_directories) {
  CComSafeArray<BSTR> directories;
  EnsureBhoIsAvailable();  // We will require a working bho to do anything.
  directories.Attach(extension_directories);  // MUST DETACH BEFORE RETURNING

  // TODO(joi@chromium.org) Handle multiple extensions.
  if (directories.GetCount() > 0) {
    // If our extension_path is not a CRX, it MUST be the same as the installed
    // extension path which would be an exploded extension.
    // If you get this DCHECK, you may have changed your registry settings to
    // debug with an exploded extension, but you didn't uninstall the previous
    // extension, either via the Chrome UI or by simply wiping out your
    // profile folder.
    DCHECK(ceee_module_util::IsCrxOrEmpty(extension_path_) ||
           extension_path_ == std::wstring(directories.GetAt(0)));
    StartExtension(directories.GetAt(0));
  } else if (!ceee_module_util::IsCrxOrEmpty(extension_path_)) {
    // We have an extension path that isn't a CRX and we don't have any
    // enabled extension, so we must load the exploded extension from this
    // given path. WE MUST DO THIS BEFORE THE NEXT ELSE IF because it assumes
    // a CRX file.
    chrome_frame_->loadExtension(CComBSTR(extension_path_.c_str()));
  } else if (!already_tried_installing_ && !extension_path_.empty()) {
    // We attempt to install the .crx file from the CEEE folder; in the
    // default case this will happen only once after installation.
    // It may seem redundant with OnCfReadyStateChanged; this is in case the
    // user deleted the extension but the registry stayed the same.
    already_tried_installing_ = true;
    chrome_frame_->installExtension(CComBSTR(extension_path_.c_str()));
  } else {
    // Hide the browser bar as fast as we can.
    // Set the current height of the bar to 0, so that if the user manually
    // shows the bar, it will not be visible on screen.
    current_height_ = 0;

    // Ask IE to reload all info for this toolband.
    ScopedComPtr<IOleCommandTarget> cmd_target;
    HRESULT hr = GetSite(IID_IOleCommandTarget,
                         reinterpret_cast<void**>(cmd_target.Receive()));
    if (SUCCEEDED(hr)) {
      CComVariant band_id(static_cast<int>(band_id_));
      hr = cmd_target->Exec(&CGID_DeskBand, DBID_BANDINFOCHANGED,
                            OLECMDEXECOPT_DODEFAULT, &band_id, NULL);
      if (FAILED(hr)) {
        LOG(ERROR) << "Failed to Execute DBID_BANDINFOCHANGED. Error Code: "
            << com::LogHr(hr);
      }
    } else {
      LOG(ERROR) << "Failed to obtain OleCommandTarget. Error Code: "
          << com::LogHr(hr);
    }
  }
  directories.Detach();
}

STDMETHODIMP_(void) ToolBand::OnCfOnload(IDispatch* event) {
  if (chrome_frame_window_.IsWindow()) {
    VLOG(1) << "Showing the Chrome Frame window.";
    chrome_frame_window_.ShowWindow(SW_SHOW);
  }
}

STDMETHODIMP_(void) ToolBand::OnCfOnloadError(IDispatch* event) {
  // Handle error the same way as OnLoad.
  LOG(ERROR) << "Chrome Frame reports onload error";
  OnCfOnload(event);
}

STDMETHODIMP_(void) ToolBand::OnIeNavigateComplete2(IDispatch* dispatch,
                                                    VARIANT* url) {
  // The flag is cleared on navigation complete since at this point we are
  // certain the process of placing the toolband has been completed.
  // Doing it in GetBandInfo proved premature as many queries are expected.
  ClearForceOwnLineFlag();

  // Now that deferred initializations are done, unadvise.
  DCHECK(web_browser_ != NULL);
  if (web_browser_ && listening_to_browser_events_) {
    HostingBrowserEvents::DispEventUnadvise(web_browser_,
                                            &DIID_DWebBrowserEvents2);
    listening_to_browser_events_ = false;
  }
}

bool ToolBand::LoadManifestFile(const std::wstring& base_dir,
                                std::string* toolband_url) {
  DCHECK(toolband_url);
  FilePath toolband_extension_path;
  toolband_extension_path = FilePath(base_dir);

  if (toolband_extension_path.empty()) {
    // Expected case if no extensions registered/found.
    return false;
  }

  ExtensionManifest manifest;
  HRESULT hr = manifest.ReadManifestFile(toolband_extension_path, true);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to read manifest at \"" <<
        toolband_extension_path.value() << "\", error " << com::LogHr(hr);
    return false;
  }

  const std::vector<std::string>& toolstrip_names(
      manifest.GetToolstripFileNames());
  if (!toolstrip_names.empty()) {
    *toolband_url = "chrome-extension://";
    *toolband_url += manifest.extension_id();
    *toolband_url += "/";
    // TODO(mad@chromium.org): For now we only load the first one we
    // find, we may want to stack them at one point...
    *toolband_url += toolstrip_names[0];
  }

  return true;
}

bool ToolBand::ShouldForceOwnLine() {
  if (!already_checked_own_line_flag_) {
    own_line_flag_ = ceee_module_util::GetOptionToolbandForceReposition();
    already_checked_own_line_flag_ = true;
  }

  return own_line_flag_;
}

void ToolBand::ClearForceOwnLineFlag() {
  if (own_line_flag_ || !already_checked_own_line_flag_) {
    own_line_flag_ = false;
    already_checked_own_line_flag_ = true;
    ceee_module_util::SetOptionToolbandForceReposition(false);
  }
}

HRESULT ToolBand::EnsureBhoIsAvailable() {
  if (m_spUnkSite == NULL || web_browser_ == NULL) {
    NOTREACHED() << "Invalid client site";
    return E_FAIL;
  }

  CComBSTR bho_class_id_bstr(CLSID_BrowserHelperObject);
  CComVariant existing_bho;
  HRESULT hr = web_browser_->GetProperty(bho_class_id_bstr, &existing_bho);

  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to retrieve BHO through GetProperty call: 0x" <<
        std::hex << hr;
    return hr;
  }

  if (existing_bho.vt != VT_EMPTY) {
    DCHECK(existing_bho.vt == VT_UNKNOWN && existing_bho.punkVal != NULL);
#ifndef NDEBUG
    if (existing_bho.vt == VT_UNKNOWN && existing_bho.punkVal != NULL) {
      // This is a sanity / assumption check regarding what we should regard
      // as a valid BHO.
      ScopedComPtr<IPersist> bho_iid_access;
      HRESULT hr2 = bho_iid_access.QueryFrom(existing_bho.punkVal);
      DCHECK(SUCCEEDED(hr2) && bho_iid_access.get() != NULL);
      if (SUCCEEDED(hr2) && bho_iid_access.get() != NULL) {
        CLSID calling_card = {};
        hr2 = bho_iid_access->GetClassID(&calling_card);
        DCHECK(SUCCEEDED(hr2) && calling_card == CLSID_BrowserHelperObject);
      }
    }
#endif
    DVLOG(1) << "BHO already loaded";
    if (existing_bho.vt != VT_UNKNOWN || existing_bho.punkVal == NULL)
      return E_FAIL;

    hr = SendSessionIdToBho(existing_bho.punkVal);
    DCHECK(SUCCEEDED(hr)) << "Failed to send tool band session ID to the " <<
        "BHO." << com::LogHr(hr);
    return SUCCEEDED(hr) ? S_OK : hr;
  }

  ScopedComPtr<IObjectWithSite> bho;
  hr = CreateBhoInstance(bho.Receive());

  if (FAILED(hr)) {
    NOTREACHED() << "Failed to create CEEE BHO." << com::LogHr(hr);
    return hr;
  }

  hr = bho->SetSite(web_browser_);
  if (FAILED(hr)) {
    NOTREACHED() << "CEEE BHO SetSite failed.:" << com::LogHr(hr);
    return hr;
  }

  hr = web_browser_->PutProperty(bho_class_id_bstr, CComVariant(bho));
  if (FAILED(hr)) {
    LOG(ERROR) << "Assigning BHO to the web browser failed." << com::LogHr(hr);
    return hr;
  }
  LOG_IF(INFO, SUCCEEDED(hr)) << "CEEE BHO has been created by the toolband.";

  hr = SendSessionIdToBho(bho);
  DCHECK(SUCCEEDED(hr)) << "Failed to send tool band session ID to the BHO." <<
      com::LogHr(hr);
  return SUCCEEDED(hr) ? S_OK : hr;
}

HRESULT ToolBand::CreateBhoInstance(IObjectWithSite** new_bho_instance) {
  DCHECK(new_bho_instance != NULL && *new_bho_instance == NULL);
  return BrowserHelperObject::CreateInstance(new_bho_instance);
}

HRESULT ToolBand::GetSessionId(int* session_id) {
  if (chrome_frame_) {
    ScopedComPtr<IChromeFrameInternal> chrome_frame_internal_;
    chrome_frame_internal_.QueryFrom(chrome_frame_);
    if (chrome_frame_internal_) {
      return chrome_frame_internal_->getSessionId(session_id);
    }
  }
  NOTREACHED();
  return E_UNEXPECTED;
}

HRESULT ToolBand::SendSessionIdToBho(IUnknown* bho) {
  if (already_sent_id_to_bho_)
    return S_FALSE;
  // Now send the tool band's session ID to the BHO.
  ScopedComPtr<ICeeeBho> ceee_bho;
  HRESULT hr = ceee_bho.QueryFrom(bho);
  if (SUCCEEDED(hr)) {
    int session_id = 0;
    hr = GetSessionId(&session_id);
    if (SUCCEEDED(hr)) {
      hr = ceee_bho->SetToolBandSessionId(session_id);
      if (SUCCEEDED(hr))
        already_sent_id_to_bho_ = true;
      return hr;
    }
  }
  return E_FAIL;
}
