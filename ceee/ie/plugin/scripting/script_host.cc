// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implements our scripting host.

#include "ceee/ie/plugin/scripting/script_host.h"

#include <dispex.h>
#include <mshtml.h>
#include <mshtmhst.h>
#include <objsafe.h>

#include "base/logging.h"
#include "base/string_util.h"
#include "base/stringize_macros.h"
#include "ceee/common/com_utils.h"

#include "version.h"  // NOLINT

#ifndef CMDID_SCRIPTSITE_URL

// These are documented in MSDN, but not declared in the platform SDK.
// See [http://msdn.microsoft.com/en-us/library/aa769871(VS.85).aspx].
#define CMDID_SCRIPTSITE_URL                0
#define CMDID_SCRIPTSITE_HTMLDLGTRUST       1
#define CMDID_SCRIPTSITE_SECSTATE           2
#define CMDID_SCRIPTSITE_SID                3
#define CMDID_SCRIPTSITE_TRUSTEDDOC         4
#define CMDID_SCRIPTSITE_SECURITY_WINDOW    5
#define CMDID_SCRIPTSITE_NAMESPACE          6
#define CMDID_SCRIPTSITE_IURI               7

const GUID CGID_ScriptSite = {
  0x3050F3F1, 0x98B5, 0x11CF, 0xBB, 0x82, 0x00, 0xAA, 0x00, 0xBD, 0xCE, 0x0B };
#endif  // CMDID_SCRIPTSITE_URL

namespace {
// This class is a necessary wrapper around a text string in
// IDebugDocumentHelper, as one can really only use the deferred
// text mode of the helper if one wants to satisfy the timing
// requirements on notifications imposed by script debuggers.
//
// The timing requirement is this:
//
// It appears that for a debugger to successfully set a breakpoint in
// an ActiveScript engine, the code in question has to have been parsed.
// The VisualStudio and the IE8 debugger both appear to use the events
// IDebugDocumentTextEvents as a trigger point to apply any pending
// breakpoints to the engine. The problem here is that with naive usage of
// the debug document helper, one would simply provide it with the
// text contents of the document at creation, before ParseScriptText
// is performed. This fires the text events too early, which means
// the debugger will not find any code to set breakpoints on.
// To ensure that the debugger finds something to grab on,
// one needs to set or modify the debug document text after
// ParseScriptText, but before execution of the text, such as e.g.
// on the OnEnterScript event to the ActiveScript site.
class DocHost
    : public CComObjectRootEx<CComSingleThreadModel>,
      public InitializingCoClass<DocHost>,
      public IDebugDocumentHost {
 public:
  BEGIN_COM_MAP(DocHost)
    COM_INTERFACE_ENTRY(IDebugDocumentHost)
  END_COM_MAP()

  HRESULT Initialize(const wchar_t* code) {
    code_ = code;
    return S_OK;
  }

  STDMETHOD(GetDeferredText)(DWORD cookie,
                             WCHAR* text,
                             SOURCE_TEXT_ATTR* text_attr,
                             ULONG* num_chars_returned,
                             ULONG max_chars) {
    size_t num_chars = std::min(static_cast<size_t>(max_chars), code_.length());
    *num_chars_returned = num_chars;
    memcpy(text, code_.c_str(), num_chars * sizeof(wchar_t));

    return S_OK;
  }

  STDMETHOD(GetScriptTextAttributes)(LPCOLESTR code,
                                     ULONG num_code_chars,
                                     LPCOLESTR delimiter,
                                     DWORD flags,
                                     SOURCE_TEXT_ATTR* attr) {
    return E_NOTIMPL;
  }

  STDMETHOD(OnCreateDocumentContext)(IUnknown** outer) {
    return E_NOTIMPL;
  }

  STDMETHOD(GetPathName)(BSTR *long_name, BOOL *is_original_file) {
    return E_NOTIMPL;
  }

  STDMETHOD(GetFileName)(BSTR *short_name) {
    return E_NOTIMPL;
  }
  STDMETHOD(NotifyChanged)(void) {
    return E_NOTIMPL;
  }
 private:
  std::wstring code_;
};

HRESULT GetUrlForDocument(IUnknown* unknown, VARIANT* url_out) {
  CComQIPtr<IHTMLDocument2> document(unknown);
  if (document == NULL)
    return E_NOINTERFACE;

  CComBSTR url;
  HRESULT hr = document->get_URL(&url);
  if (SUCCEEDED(hr)) {
    url_out->vt = VT_BSTR;
    url_out->bstrVal = url.Detach();
  } else {
    DLOG(ERROR) << "Failed to get security url " << com::LogHr(hr);
  }

  return hr;
}

HRESULT GetSIDForDocument(IUnknown* unknown, VARIANT* sid_out) {
  CComQIPtr<IServiceProvider> sp(unknown);
  if (sp == NULL)
    return E_NOINTERFACE;

  CComPtr<IInternetHostSecurityManager> security_manager;
  HRESULT hr = sp->QueryService(SID_SInternetHostSecurityManager,
                                &security_manager);
  if (FAILED(hr))
    return hr;

  // This is exactly mimicking observed behavior in IE.
  CComBSTR security_id(MAX_SIZE_SECURITY_ID);
  DWORD size = MAX_SIZE_SECURITY_ID;
  hr = security_manager->GetSecurityId(
      reinterpret_cast<BYTE*>(security_id.m_str), &size, 0);
  if (SUCCEEDED(hr)) {
    sid_out->vt = VT_BSTR;
    sid_out->bstrVal = security_id.Detach();
  } else {
    DLOG(ERROR) << "Failed to get security manager " << com::LogHr(hr);
  }

  return hr;
}

HRESULT GetWindowForDocument(IUnknown* unknown, VARIANT* window_out) {
  CComQIPtr<IServiceProvider> sp(unknown);
  if (sp == NULL)
    return E_NOINTERFACE;

  CComPtr<IDispatch> window;
  HRESULT hr = sp->QueryService(SID_SHTMLWindow, &window);
  if (SUCCEEDED(hr)) {
    window_out->vt = VT_DISPATCH;
    window_out->pdispVal = window.Detach();
  } else {
    DLOG(ERROR) << "Failed to get window " << com::LogHr(hr);
  }

  return hr;
}

}  // namespace

// {58E6D2A5-4868-4E49-B3E9-072C845A014A}
const GUID IID_IScriptHost =
  { 0x58ECD2A5, 0x4868, 0x4E49,
      { 0xB3, 0xE9, 0x07, 0x2C, 0x84, 0x5A, 0x01, 0x4A } };

// {f414c260-6ac0-11cf-b6d1-00aa00bbbb58}
const GUID CLSID_JS =
  { 0xF414C260, 0x6AC0, 0x11CF,
    { 0xB6, 0xD1, 0x00, 0xAA, 0x00, 0xBB, 0xBB, 0x58 } };


ScriptHost::DebugApplication* ScriptHost::default_debug_application_;


ScriptHost::ScriptHost() : debug_application_(NULL) {
}

HRESULT ScriptHost::Initialize(DebugApplication* debug_application) {
  debug_application_ = debug_application;

  HRESULT hr = CreateScriptEngine(&script_);
  if (FAILED(hr)) {
    NOTREACHED();
    return hr;
  }

  if (FAILED(hr = script_->SetScriptSite(this))) {
    NOTREACHED();
    return hr;
  }

  // Get engine's IActiveScriptParse interface, initialize it
  script_parse_ = script_;
  if (!script_parse_) {
    NOTREACHED();
    return E_NOINTERFACE;
  }

  if (FAILED(hr = script_parse_->InitNew())) {
    NOTREACHED();
    return hr;
  }

  // Set the security options of the script engine so it
  // queries us for IInternetHostSecurityManager, which
  // we delegate to our site.
  CComQIPtr<IObjectSafety> script_safety(script_);
  if (script_safety == NULL) {
    NOTREACHED() << "Script engine does not implement IObjectSafety";
    return E_NOINTERFACE;
  }

  hr = script_safety->SetInterfaceSafetyOptions(
      IID_IDispatch, INTERFACE_USES_SECURITY_MANAGER,
      INTERFACE_USES_SECURITY_MANAGER);

  // Set the script engine into a running state.
  hr = script_->SetScriptState(SCRIPTSTATE_CONNECTED);
  DCHECK(SUCCEEDED(hr));

  return hr;
}

HRESULT ScriptHost::Initialize(DebugApplication* debug_application,
                               ScriptHost** self) {
  *self = this;
  return Initialize(debug_application);
}

HRESULT ScriptHost::Initialize() {
  return Initialize(default_debug_application_);
}

void ScriptHost::FinalRelease() {
  DCHECK(script_ == NULL);
  debug_application_ = NULL;
}

HRESULT ScriptHost::RegisterScriptObject(const wchar_t* name,
                                         IDispatch* disp_obj,
                                         bool make_members_global) {
  DCHECK(name);
  DCHECK(disp_obj);
  std::wstring wname = name;

  // Check if the name already exists.
  ScriptObjectMap::iterator iter = script_objects_.find(wname);
  if (iter != script_objects_.end()) {
    return E_ACCESSDENIED;
  }

  // Add to the script object map.
  CComPtr<IDispatch> disp_obj_ptr(disp_obj);
  CAdapt<CComPtr<IDispatch>> disp_obj_adapt(disp_obj_ptr);
  script_objects_.insert(std::make_pair(wname, disp_obj_adapt));

  // Add to the script engine.
  DWORD flags = SCRIPTITEM_ISSOURCE | SCRIPTITEM_ISVISIBLE;
  if (make_members_global) {
    flags |= SCRIPTITEM_GLOBALMEMBERS;
  }
  script_->AddNamedItem(name, flags);

  return S_OK;
}

HRESULT ScriptHost::RunScript(const wchar_t* file_path,
                              const wchar_t* code) {
  DCHECK(file_path);
  DCHECK(code);
  if (!file_path || !code)
    return E_POINTER;

  DWORD source_context = 0;
  HRESULT hr = GetSourceContext(file_path, code, &source_context);
  if (FAILED(hr))
    return hr;

  ScopedExcepInfo ei;
  hr = script_parse_->ParseScriptText(
      code, NULL, NULL, NULL, source_context, 0,
      SCRIPTTEXT_HOSTMANAGESSOURCE | SCRIPTTEXT_ISVISIBLE, NULL, &ei);
  // A syntax error is not a CEEE error, so we don't log an error and we return
  // it normally so that the caller knows what happened.
  if (FAILED(hr) && hr != OLESCRIPT_E_SYNTAX) {
    LOG(ERROR) << "Non-script error occurred while parsing script. "
        << com::LogHr(hr);
    NOTREACHED();
  }

  return hr;
}

HRESULT ScriptHost::RunExpression(const wchar_t* code, VARIANT* result) {
  DCHECK(code);
  if (!code)
    return E_POINTER;

  ScopedExcepInfo ei;
  HRESULT hr = script_parse_->ParseScriptText(
      code, NULL, NULL, NULL, 0, 0, SCRIPTTEXT_ISEXPRESSION, result, &ei);
  // Ignore compilation and runtime errors in the script
  if (FAILED(hr) && hr != OLESCRIPT_E_SYNTAX) {
    LOG(ERROR) << "Non-script error occurred while parsing script. "
        << com::LogHr(hr);
    NOTREACHED();
  }

  return hr;
}

HRESULT ScriptHost::Close() {
  // Close our script host.
  HRESULT hr = S_OK;
  if (script_) {
    // Try to force garbage collection at this time so any objects holding
    // reference to native components will be released immediately and dlls
    // loaded can be unloaded quickly.
    CComQIPtr<IActiveScriptGarbageCollector> script_gc(script_);
    if (script_gc != NULL)
      script_gc->CollectGarbage(SCRIPTGCTYPE_EXHAUSTIVE);

    hr = script_->Close();
  }

  // Detach all debug documents.
  DebugDocMap::iterator iter;
  for (iter = debug_docs_.begin(); iter != debug_docs_.end(); iter++) {
    // Note that this is IDebugDocumentHelper::Detach and not
    // CComPtr::Detach
    iter->second.document->Detach();
  }
  debug_docs_.clear();

  script_.Release();

  return hr;
}

STDMETHODIMP ScriptHost::GetItemInfo(LPCOLESTR item_name, DWORD return_mask,
                                     IUnknown** item_unknown,
                                     ITypeInfo** item_itypeinfo) {
  DCHECK(!(return_mask & SCRIPTINFO_IUNKNOWN) || item_unknown);
  DCHECK(!(return_mask & SCRIPTINFO_ITYPEINFO) || item_itypeinfo);

  HRESULT hr = S_OK;

  std::wstring wname = item_name;
  ScriptObjectMap::iterator iter = script_objects_.find(wname);
  if (iter != script_objects_.end()) {
    CComPtr<IDispatch> disp_obj = iter->second.m_T;

    CComPtr<IUnknown> unknown;
    if (return_mask & SCRIPTINFO_IUNKNOWN) {
      DCHECK(item_unknown);
      hr = disp_obj.QueryInterface(&unknown);
    }

    CComPtr<ITypeInfo> typeinfo;
    if (SUCCEEDED(hr) && return_mask & SCRIPTINFO_ITYPEINFO) {
      DCHECK(item_itypeinfo);
      hr = disp_obj->GetTypeInfo(0, LANG_NEUTRAL, &typeinfo);
    }

    // We have everything ready, return the out args on success.
    if (SUCCEEDED(hr)) {
      if (return_mask & SCRIPTINFO_IUNKNOWN) {
        hr = unknown.CopyTo(item_unknown);
        DCHECK(SUCCEEDED(hr));
      }
      if (return_mask & SCRIPTINFO_ITYPEINFO) {
        hr = typeinfo.CopyTo(item_itypeinfo);
        DCHECK(SUCCEEDED(hr));
      }
    }
  } else {
    hr = TYPE_E_ELEMENTNOTFOUND;
  }

  return hr;
}

STDMETHODIMP ScriptHost::GetLCID(LCID *plcid) {
  return E_NOTIMPL;
}

STDMETHODIMP ScriptHost::GetDocVersionString(BSTR* version) {
  return E_NOTIMPL;
}

STDMETHODIMP ScriptHost::OnEnterScript() {
  DebugDocMap::iterator it(debug_docs_.begin());
  DebugDocMap::iterator end(debug_docs_.end());

  // It is necessary to defer the document size notifications
  // below until after defining all relevant script blocks,
  // in order to tickle the debugger at just the right time
  // so it can turn around and set any pending breakpoints
  // prior to first execution.
  for (; it != end; ++it) {
    DebugDocInfo& info = it->second;

    if (info.is_new) {
      info.is_new = false;
      info.document->AddDeferredText(info.len, 0);
    }
  }

  return S_OK;
}

STDMETHODIMP ScriptHost::OnLeaveScript() {
  return S_OK;
}

STDMETHODIMP ScriptHost::OnStateChange(SCRIPTSTATE state) {
  return S_OK;
}

STDMETHODIMP ScriptHost::OnScriptTerminate(const VARIANT* result,
                                           const EXCEPINFO* excep_info) {
  return S_OK;
}

STDMETHODIMP ScriptHost::OnScriptError(IActiveScriptError* script_error) {
  ScopedExcepInfo ei;
  HRESULT hr = script_error->GetExceptionInfo(&ei);
  LOG_IF(ERROR, FAILED(hr)) << "Failed to GetExceptionInfo. " <<
      com::LogHr(hr);

  CComBSTR source_line;
  hr = script_error->GetSourceLineText(&source_line);
  LOG_IF(ERROR, FAILED(hr)) << "Failed to GetSourceLineText. " <<
      com::LogHr(hr);

  DWORD context = 0;
  ULONG line_number = 0;
  LONG char_pos = 0;
  hr = script_error->GetSourcePosition(&context, &line_number, &char_pos);
  LOG_IF(ERROR, FAILED(hr)) << "Failed to GetSourcePosition. " <<
      com::LogHr(hr);

  LOG(ERROR) << "Script error occurred: " <<
      com::ToString(ei.bstrDescription) << ". Source Text: " <<
      com::ToString(source_line) << ". Context: "<< context << ", line: " <<
      line_number << ", char pos: " << char_pos;
  return S_OK;
}

STDMETHODIMP ScriptHost::GetDocumentContextFromPosition(
    DWORD source_context, ULONG char_offset, ULONG num_chars,
    IDebugDocumentContext** debug_doc_context) {
  LOG(INFO) << "GetDocumentContextFromPosition(" << source_context << ", "
      << char_offset << ", " << num_chars << ")";

  DebugDocMap::iterator iter;
  iter = debug_docs_.find(source_context);
  if (iter != debug_docs_.end()) {
    DebugDocInfo& info = iter->second;
    ULONG start_position = 0;
    HRESULT hr = info.document->GetScriptBlockInfo(source_context,
                                            NULL,
                                            &start_position,
                                            NULL);
    if (FAILED(hr))
      LOG(ERROR) << "GetScriptBlockInfo failed " << com::LogHr(hr);

    if (SUCCEEDED(hr)) {
      hr = info.document->CreateDebugDocumentContext(
          start_position + char_offset, num_chars, debug_doc_context);
      if (FAILED(hr))
        LOG(ERROR) << "GetScriptBlockInfo failed " << com::LogHr(hr);
  }

    return hr;
  }

  LOG(ERROR) << "No debug document for context " << source_context;

  return E_FAIL;
}

STDMETHODIMP ScriptHost::GetApplication(IDebugApplication** debug_app) {
  if (debug_application_)
    return debug_application_->GetDebugApplication(debug_app);

  return E_NOTIMPL;
}

STDMETHODIMP ScriptHost::GetRootApplicationNode(
    IDebugApplicationNode** debug_app_node) {
  DCHECK(debug_app_node);
  if (!debug_app_node)
    return E_POINTER;

  if (debug_application_ != NULL) {
    return debug_application_->GetRootApplicationNode(debug_app_node);
  } else {
    *debug_app_node = NULL;
    return S_OK;
  }

  NOTREACHED();
}

STDMETHODIMP ScriptHost::OnScriptErrorDebug(IActiveScriptErrorDebug* err,
    BOOL* enter_debugger, BOOL* call_on_script_err_when_continuing) {
  DCHECK(err);
  DCHECK(enter_debugger);
  DCHECK(call_on_script_err_when_continuing);
  if (!err || !enter_debugger || !call_on_script_err_when_continuing)
    return E_POINTER;

  // TODO(ericdingle@chromium.org): internationalization
  int ret = ::MessageBox(
      NULL, L"A script error occured. Do you want to debug?",
      TO_L_STRING(CEEE_PRODUCT_FULLNAME_STRING),
      MB_ICONERROR | MB_SETFOREGROUND | MB_TASKMODAL | MB_YESNO);
  *enter_debugger = (ret == IDYES);
  *call_on_script_err_when_continuing = FALSE;

  return S_OK;
}

STDMETHODIMP ScriptHost::QueryStatus(const GUID* cmd_group, ULONG num_cmds,
    OLECMD cmds[], OLECMDTEXT *cmd_text) {
  LOG(WARNING) << "QueryStatus " <<
      CComBSTR(cmd_group ? *cmd_group : GUID_NULL) << ", " << num_cmds;
  // We're practically unimplemented.
  DLOG(INFO) << "ScriptHost::QueryStatus called";
  return OLECMDERR_E_UNKNOWNGROUP;
};

STDMETHODIMP ScriptHost::Exec(const GUID* cmd_group, DWORD cmd_id,
    DWORD cmd_exec_opt, VARIANT *arg_in, VARIANT *arg_out) {
  LOG(WARNING) << "Exec " << CComBSTR(cmd_group ? *cmd_group : GUID_NULL) <<
      ", " << cmd_id;

  if (cmd_group && *cmd_group == CGID_ScriptSite) {
    switch (cmd_id) {
      case CMDID_SCRIPTSITE_URL:
        return GetUrlForDocument(m_spUnkSite, arg_out);
        break;
      case CMDID_SCRIPTSITE_HTMLDLGTRUST:
        DLOG(INFO) << "CMDID_SCRIPTSITE_HTMLDLGTRUST";
        break;
      case CMDID_SCRIPTSITE_SECSTATE:
        DLOG(INFO) << "CMDID_SCRIPTSITE_SECSTATE";
        break;
      case CMDID_SCRIPTSITE_SID:
        return GetSIDForDocument(m_spUnkSite, arg_out);
        break;
      case CMDID_SCRIPTSITE_TRUSTEDDOC:
        DLOG(INFO) << "CMDID_SCRIPTSITE_TRUSTEDDOC";
        break;
      case CMDID_SCRIPTSITE_SECURITY_WINDOW:
        return GetWindowForDocument(m_spUnkSite, arg_out);
        break;
      case CMDID_SCRIPTSITE_NAMESPACE:
        DLOG(INFO) << "CMDID_SCRIPTSITE_NAMESPACE";
        break;
      case CMDID_SCRIPTSITE_IURI:
        DLOG(INFO) << "CMDID_SCRIPTSITE_IURI";
        break;
      default:
        DLOG(INFO) << "ScriptHost::Exec unknown command " << cmd_id;
        break;
    }
  }

  return OLECMDERR_E_UNKNOWNGROUP;
}

HRESULT ScriptHost::CreateScriptEngine(IActiveScript** script) {
  return script_.CoCreateInstance(CLSID_JS, NULL, CLSCTX_INPROC_SERVER);
}

HRESULT ScriptHost::GetSourceContext(const wchar_t* file_path,
                                     const wchar_t* code,
                                     DWORD* source_context) {
  DCHECK(debug_application_ != NULL);
  HRESULT hr = S_OK;
  CComPtr<IDebugDocumentHelper> helper;
  hr = debug_application_->CreateDebugDocumentHelper(file_path,
                                                     code,
                                                     0,
                                                     &helper);
  size_t len = lstrlenW(code);
  if (SUCCEEDED(hr) && helper != NULL) {
    hr = helper->DefineScriptBlock(0, len, script_, FALSE, source_context);
    DCHECK(SUCCEEDED(hr));

    DebugDocInfo info;
    info.is_new = true;
    info.len = len;
    info.document = helper;
    debug_docs_.insert(std::make_pair(*source_context, info));
  }

  return hr;
}

HRESULT ScriptHost::AddDebugDocument(const wchar_t* file_path,
                                     const wchar_t* code,
                                     IDebugDocumentHelper** doc) {
  DCHECK(debug_application_ != NULL);
  return debug_application_->CreateDebugDocumentHelper(file_path,
                                                       code,
                                                       TEXT_DOC_ATTR_READONLY,
                                                       doc);
}

HRESULT ScriptHost::RunScriptSnippet(size_t start_offset,
                                     const wchar_t* code,
                                     IDebugDocumentHelper* doc) {
  DWORD source_context = 0;
  if (doc) {
    size_t len = lstrlenW(code);
    HRESULT hr = doc->DefineScriptBlock(start_offset,
                                        len,
                                        script_,
                                        FALSE,
                                        &source_context);

    if (SUCCEEDED(hr)) {
      DebugDocInfo info;
      info.document = doc;
      info.len = start_offset + len;
      info.is_new = true;

      debug_docs_.insert(std::make_pair(source_context, info));
    } else {
      LOG(ERROR) << "Failed to define a script block " << com::LogHr(hr);
      LOG(ERROR) << "Script: " << code;
    }
  }

  ScopedExcepInfo ei;
  HRESULT hr = script_parse_->ParseScriptText(
      code, NULL, NULL, NULL, source_context, 0,
      SCRIPTTEXT_HOSTMANAGESSOURCE | SCRIPTTEXT_ISVISIBLE, NULL, &ei);

  // Ignore compilation and runtime errors in the script
  if (FAILED(hr) && hr != OLESCRIPT_E_SYNTAX) {
    LOG(ERROR) << "Non-script error occurred while parsing script. "
        << com::LogHr(hr);
    NOTREACHED();
  }

  return hr;
}

ScriptHost::DebugApplication::DebugApplication(const wchar_t* application_name)
    : application_name_(application_name),
      debug_app_cookie_(kInvalidDebugAppCookie),
      initialization_count_(0) {
}

ScriptHost::DebugApplication::~DebugApplication() {
  DCHECK(debug_manager_ == NULL);
  DCHECK(debug_application_ == NULL);
  DCHECK_EQ(0U, initialization_count_);
}

void ScriptHost::DebugApplication::RegisterDebugApplication() {
  DCHECK(debug_manager_ == NULL);
  DCHECK(debug_application_ == NULL);
  DCHECK(debug_app_cookie_ == kInvalidDebugAppCookie);

  // Don't need to lock as this MUST be single-threaded and first use.
  CComPtr<IProcessDebugManager> manager;
  HRESULT hr = CreateProcessDebugManager(&manager);

  CComPtr<IDebugApplication> application;
  if (SUCCEEDED(hr))
    hr = manager->CreateApplication(&application);

  if (SUCCEEDED(hr))
    hr = application->SetName(application_name_);

  DWORD cookie = 0;
  if (SUCCEEDED(hr))
    hr = manager->AddApplication(application, &cookie);

  if (FAILED(hr)) {
    LOG(INFO) << "ScriptHost debug initialization failed: " << com::LogHr(hr);
    return;
  }

  debug_manager_ = manager;
  debug_application_ = application;
  debug_app_cookie_ = cookie;
}

void ScriptHost::DebugApplication::Initialize() {
  base::AutoLock lock(lock_);

  ++initialization_count_;

  if (initialization_count_ == 1)
    RegisterDebugApplication();
}

void ScriptHost::DebugApplication::Initialize(
    IUnknown* debug_application_provider) {
  base::AutoLock lock(lock_);

  ++initialization_count_;

  if (initialization_count_ == 1) {
    DCHECK(debug_manager_ == NULL);
    DCHECK(debug_application_ == NULL);
    DCHECK(debug_app_cookie_ == kInvalidDebugAppCookie);

    CComPtr<IDebugApplication> debug_app;
    HRESULT hr = debug_application_provider->QueryInterface(&debug_app);
    if (FAILED(hr) || debug_app == NULL) {
      CComPtr<IServiceProvider> sp;
      hr = debug_application_provider->QueryInterface(&sp);
      if (SUCCEEDED(hr) && sp != NULL)
        hr = sp->QueryService(IID_IDebugApplication, &debug_app);
    }

    if (debug_app != NULL)
      debug_application_ = debug_app;
    else
      RegisterDebugApplication();
  }
}

void ScriptHost::DebugApplication::Initialize(
    IProcessDebugManager* manager, IDebugApplication* app) {
  base::AutoLock lock(lock_);
  // This function is exposed for testing only.
  DCHECK_EQ(0U, initialization_count_);

  DCHECK_EQ(static_cast<IUnknown*>(NULL), debug_manager_);
  DCHECK_EQ(static_cast<IUnknown*>(NULL), debug_application_);
  DCHECK_EQ(kInvalidDebugAppCookie, debug_app_cookie_);

  ++initialization_count_;

  debug_manager_ = manager;
  debug_application_ = app;
}


void ScriptHost::DebugApplication::Terminate() {
  base::AutoLock lock(lock_);
  DCHECK_GT(initialization_count_, (size_t)0);
  --initialization_count_;

  if (initialization_count_ == 0) {
    if (debug_manager_ != NULL) {
      if (debug_app_cookie_ != kInvalidDebugAppCookie) {
        HRESULT hr = debug_manager_->RemoveApplication(debug_app_cookie_);
        DCHECK(SUCCEEDED(hr));
      }
    }

    debug_manager_.Release();
    debug_application_.Release();
    debug_app_cookie_ = kInvalidDebugAppCookie;
  }
}

HRESULT ScriptHost::DebugApplication::GetDebugApplication(
    IDebugApplication** app) {
  base::AutoLock lock(lock_);

  if (debug_application_ == NULL)
    return E_NOTIMPL;

  return debug_application_.CopyTo(app);
}

HRESULT ScriptHost::DebugApplication::GetRootApplicationNode(
    IDebugApplicationNode** debug_app_node) {
  base::AutoLock lock(lock_);

  if (debug_application_ == NULL) {
    *debug_app_node = NULL;
    return S_OK;
  }

  return debug_application_->GetRootNode(debug_app_node);
}

HRESULT ScriptHost::DebugApplication::CreateDebugDocumentHelper(
    const wchar_t* long_name, const wchar_t* code, TEXT_DOC_ATTR attributes,
    IDebugDocumentHelper** helper) {
  base::AutoLock lock(lock_);

  if (debug_application_ == NULL)
    return S_OK;

  // Find the last forward or backward slash in the long name,
  // and construct a short name from the rest - the base name.
  std::wstring name(long_name);
  size_t pos = name.find_last_of(L"\\/");
  const wchar_t* short_name = long_name;
  if (pos != name.npos && long_name[pos + 1] != '\0')
    short_name = long_name + pos + 1;

  CComPtr<IDebugDocumentHelper> doc;
  HRESULT hr = CreateDebugDocumentHelper(&doc);
  if (SUCCEEDED(hr))
    hr = doc->Init(debug_application_, short_name, long_name, attributes);

  // Wrap the text in a document host.
  CComPtr<IDebugDocumentHost> host;
  if (SUCCEEDED(hr))
    hr = DocHost::CreateInitialized(code, &host);
  if (SUCCEEDED(hr))
    hr = doc->SetDebugDocumentHost(host);
  if (SUCCEEDED(hr))
    hr = doc->Attach(NULL);

  if (SUCCEEDED(hr)) {
    DCHECK(doc != NULL);

    *helper = doc.Detach();
  }

  return hr;
}

HRESULT ScriptHost::DebugApplication::CreateDebugDocumentHelper(
    IDebugDocumentHelper** helper) {
  // Create the debug document.
  if (debug_manager_ != NULL)
    return debug_manager_->CreateDebugDocumentHelper(NULL, helper);

  // As it turns out, it's better to create a debug document
  // from the same DLL server as issued the debug manager, so let's
  // try and accomodate. Get the class object function from the
  // in-process instance.
  HMODULE pdm = ::GetModuleHandle(L"pdm.dll");
  LPFNGETCLASSOBJECT pdm_get_class_object = NULL;
  if (pdm != NULL)
    pdm_get_class_object = reinterpret_cast<LPFNGETCLASSOBJECT>(
        ::GetProcAddress(pdm, "DllGetClassObject"));

  // Fallback to plain CoCreateInstance if we didn't get the function.
  if (!pdm_get_class_object) {
    LOG(WARNING) << "CreateDebugDocumentHelper falling back to "
        "CoCreateInstance";
    return ::CoCreateInstance(CLSID_CDebugDocumentHelper,
                              NULL,
                              CLSCTX_INPROC_SERVER,
                              IID_IDebugDocumentHelper,
                              reinterpret_cast<void**>(helper));
  }

  // Create a debug helper.
  CComPtr<IClassFactory> factory;
  HRESULT hr = pdm_get_class_object(CLSID_CDebugDocumentHelper,
                                    IID_IClassFactory,
                                    reinterpret_cast<void**>(&factory));
  if (SUCCEEDED(hr)) {
    DCHECK(factory != NULL);
    hr = factory->CreateInstance(NULL,
                                 IID_IDebugDocumentHelper,
                                 reinterpret_cast<void**>(helper));
  }

  return hr;
}

HRESULT ScriptHost::DebugApplication::CreateProcessDebugManager(
    IProcessDebugManager** manager) {
  return ::CoCreateInstance(CLSID_ProcessDebugManager,
                            NULL,
                            CLSCTX_INPROC_SERVER,
                            IID_IProcessDebugManager,
                            reinterpret_cast<void**>(manager));
}
