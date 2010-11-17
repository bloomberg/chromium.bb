// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// @file
// Content script manager implementation.
#include "ceee/ie/plugin/scripting/content_script_manager.h"

#include "ceee/ie/common/ceee_module_util.h"
#include "ceee/ie/plugin/bho/dom_utils.h"
#include "ceee/ie/plugin/bho/frame_event_handler.h"
#include "ceee/ie/plugin/scripting/content_script_native_api.h"
#include "base/logging.h"
#include "base/resource_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "ceee/common/com_utils.h"

#include "toolband.h"  // NOLINT

namespace {
// The list of bootstrap scripts we need to parse in a new scripting engine.
// We store our content scripts by name, in RT_HTML resources. This allows
// referring them by res: URLs, which makes debugging easier.
struct BootstrapScript {
  const wchar_t* name;
  // A named function to be called after the script is executed.
  const wchar_t* function_name;
  std::wstring url;
  std::wstring content;
};

BootstrapScript bootstrap_scripts[] = {
  { L"base.js", NULL },
  { L"json.js", NULL },
  { L"ceee_bootstrap.js", L"ceee.startInit_" },
  { L"event_bindings.js", NULL },
  { L"renderer_extension_bindings.js", L"ceee.endInit_" }
};

bool bootstrap_scripts_loaded = false;

// Load the bootstrap javascript resources to our cache.
bool EnsureBoostrapScriptsLoaded() {
  if (bootstrap_scripts_loaded)
    return true;

  ceee_module_util::AutoLock lock;
  if (bootstrap_scripts_loaded)
    return true;

  HMODULE module = _AtlBaseModule.GetResourceInstance();

  // And construct the base URL.
  std::wstring base_url(L"ceee-content://bootstrap/");

  // Retrieve the resources one by one and convert them to Unicode.
  for (int i = 0; i < arraysize(bootstrap_scripts); ++i) {
    const wchar_t* name = bootstrap_scripts[i].name;
    HRSRC hres_info = ::FindResource(module, name, MAKEINTRESOURCE(RT_HTML));
    if (hres_info == NULL)
      return false;

    DWORD data_size = ::SizeofResource(module, hres_info);
    HGLOBAL hres = ::LoadResource(module, hres_info);
    if (!hres)
      return false;

    void* resource = ::LockResource(hres);
    if (!resource)
      return false;
    bool converted = UTF8ToWide(reinterpret_cast<const char*>(resource),
                                data_size,
                                &bootstrap_scripts[i].content);
    if (!converted)
      return false;

    bootstrap_scripts[i].url = StringPrintf(L"%ls%ls", base_url.c_str(), name);
  }

  bootstrap_scripts_loaded = true;
  return true;
}

HRESULT InvokeNamedFunction(IScriptHost* script_host,
                            const wchar_t* function_name,
                            VARIANT* args,
                            size_t num_args) {
  // Get the named function.
  CComVariant function_var;
  HRESULT hr = script_host->RunExpression(function_name, &function_var);
  if (FAILED(hr))
    return hr;

  // And invoke it with the the params.
  if (V_VT(&function_var) != VT_DISPATCH)
    return E_UNEXPECTED;

  // Take over the IDispatch pointer.
  CComDispatchDriver function_disp;
  function_disp.Attach(V_DISPATCH(&function_var));
  V_VT(&function_var) = VT_EMPTY;
  V_DISPATCH(&function_var) = NULL;

  return function_disp.InvokeN(static_cast<DISPID>(DISPID_VALUE),
                               args,
                               num_args);
}

}  // namespace

ContentScriptManager::ContentScriptManager() : require_all_frames_(false) {
}

ContentScriptManager::~ContentScriptManager() {
  // TODO(siggi@chromium.org): This mandates teardown prior to
  //    deletion, is that necessary?
  DCHECK(script_host_ == NULL);
}

HRESULT ContentScriptManager::GetOrCreateScriptHost(
    IHTMLDocument2* document, IScriptHost** host) {
  if (script_host_ == NULL) {
    HRESULT hr = CreateScriptHost(&script_host_);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to create script host " << com::LogHr(hr);
      return hr;
    }

    hr = InitializeScriptHost(document, script_host_);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to initialize script host " << com::LogHr(hr);
      script_host_.Release();
      native_api_.Release();
      return hr;
    }

    CComQIPtr<IObjectWithSite> script_host_with_site(script_host_);
    // Our implementation of script host must always implement IObjectWithSite.
    DCHECK(script_host_with_site != NULL);
    hr = script_host_with_site->SetSite(document);
    DCHECK(SUCCEEDED(hr));
  }

  DCHECK(script_host_ != NULL);

  return script_host_.CopyTo(host);
}

HRESULT ContentScriptManager::CreateScriptHost(IScriptHost** script_host) {
  return ScriptHost::CreateInitializedIID(IID_IScriptHost, script_host);
}

HRESULT ContentScriptManager::InitializeScriptHost(
    IHTMLDocument2* document, IScriptHost* script_host) {
  DCHECK(document != NULL);
  DCHECK(script_host != NULL);

  CComPtr<IExtensionPortMessagingProvider> messaging_provider;
  HRESULT hr = host_->GetExtensionPortMessagingProvider(&messaging_provider);
  if (FAILED(hr))
    return hr;
  hr = ContentScriptNativeApi::CreateInitialized(messaging_provider,
                                                 &native_api_);
  if (FAILED(hr))
    return hr;

  std::wstring extension_id;
  host_->GetExtensionId(&extension_id);
  DCHECK(extension_id.size()) <<
      "Need to revisit async loading of enabled extension list.";

  // Execute the bootstrap scripts.
  hr = BootstrapScriptHost(script_host, native_api_, extension_id.c_str());
  if (FAILED(hr))
    return hr;

  // Register the window object and initialize the global namespace of the
  // script host.
  CComPtr<IHTMLWindow2> window;
  hr = document->get_parentWindow(&window);
  if (FAILED(hr))
    return hr;
  hr = script_host->RegisterScriptObject(L"unsafeWindow", window, false);
  if (FAILED(hr))
    return hr;
  hr = InvokeNamedFunction(script_host, L"ceee.initGlobals_", NULL, 0);

  return hr;
}

HRESULT ContentScriptManager::BootstrapScriptHost(IScriptHost* script_host,
                                                  IDispatch* native_api,
                                                  const wchar_t* extension_id) {
  bool loaded = EnsureBoostrapScriptsLoaded();
  if (!loaded) {
    NOTREACHED() << "Unable to load bootstrap scripts";
    return E_UNEXPECTED;
  }

  // Note args go in reverse order.
  CComVariant args[] = {
    extension_id,
    native_api
  };

  // Run the bootstrap scripts.
  for (int i = 0; i < arraysize(bootstrap_scripts); ++i) {
    const wchar_t* url = bootstrap_scripts[i].url.c_str();
    HRESULT hr = script_host->RunScript(url,
                                        bootstrap_scripts[i].content.c_str());
    if (FAILED(hr)) {
      NOTREACHED() << "Bootstrap script \"" << url << "\" failed to load";
      return hr;
    }

    // Execute the script's named function if it exists.
    const wchar_t* function_name = bootstrap_scripts[i].function_name;
    if (function_name) {
      hr = InvokeNamedFunction(script_host, function_name, args,
                               arraysize(args));
      if (FAILED(hr)) {
        NOTREACHED() << "Named function \"" << function_name << "\" not called";
        return hr;
      }
    }
  }

  return S_OK;
}

HRESULT ContentScriptManager::LoadCss(const GURL& match_url,
                                      IHTMLDocument2* document) {
  // Get the CSS content for all matching user scripts and inject it.
  std::string css_content;
  HRESULT hr = host_->GetMatchingUserScriptsCssContent(match_url,
                                                       require_all_frames_,
                                                       &css_content);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get script content " << com::LogHr(hr);
    return hr;
  }

  if (!css_content.empty())
    return InsertCss(CA2W(css_content.c_str()), document);

  return S_OK;
}

HRESULT ContentScriptManager::LoadStartScripts(const GURL& match_url,
                                               IHTMLDocument2* document) {
  // Run the document start scripts.
  return LoadScriptsImpl(match_url, document, UserScript::DOCUMENT_START);
}

HRESULT ContentScriptManager::LoadEndScripts(const GURL& match_url,
                                             IHTMLDocument2* document) {
  // Run the document end scripts.
  return LoadScriptsImpl(match_url, document, UserScript::DOCUMENT_END);
}

HRESULT ContentScriptManager::LoadScriptsImpl(const GURL& match_url,
                                              IHTMLDocument2* document,
                                              UserScript::RunLocation when) {
  // Run the document start scripts.
  UserScriptsLibrarian::JsFileList js_file_list;
  HRESULT hr = host_->GetMatchingUserScriptsJsContent(match_url,
                                                      when,
                                                      require_all_frames_,
                                                      &js_file_list);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get script content " << com::LogHr(hr);
    return hr;
  }

  // Early out to avoid initializing scripting if we don't need it.
  if (js_file_list.size() == 0)
    return S_OK;

  for (size_t i = 0; i < js_file_list.size(); ++i) {
    hr = ExecuteScript(CA2W(js_file_list[i].content.c_str()),
                       js_file_list[i].file_path.c_str(),
                       document);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to inject JS content into page " << com::LogHr(hr);
      return hr;
    }
  }

  return S_OK;
}

HRESULT ContentScriptManager::ExecuteScript(const wchar_t* code,
                                            const wchar_t* file_path,
                                            IHTMLDocument2* document) {
  CComPtr<IScriptHost> script_host;
  HRESULT hr = GetOrCreateScriptHost(document, &script_host);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to retrieve script host " << com::LogHr(hr);
    return hr;
  }

  hr = script_host->RunScript(file_path, code);
  if (FAILED(hr)) {
    if (hr == OLESCRIPT_E_SYNTAX) {
      // This function is used to execute scripts from extensions. We log
      // syntax and runtime errors but we don't return a failing HR as we are
      // executing third party code. A syntax or runtime error already causes
      // the script host to prompt the user to debug.
      LOG(ERROR) << "A syntax or runtime error occured while executing " <<
          "script " << com::LogHr(hr);
    } else {
      LOG(ERROR) << "Failed to execute script " << com::LogHr(hr);
      return hr;
    }
  }

  return S_OK;
}

HRESULT ContentScriptManager::InsertCss(const wchar_t* code,
                                        IHTMLDocument2* document) {
  CComPtr<IHTMLDOMNode> head_node;
  HRESULT hr = GetHeadNode(document, &head_node);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to retrieve document head node " << com::LogHr(hr);
    return hr;
  }

  hr = InjectStyleTag(document, head_node, code);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to inject CSS content into page "
        << com::LogHr(hr);
    return hr;
  }

  return S_OK;
}

HRESULT ContentScriptManager::GetHeadNode(IHTMLDocument* document,
                                          IHTMLDOMNode** dom_head) {
  return DomUtils::GetHeadNode(document, dom_head);
}

HRESULT ContentScriptManager::InjectStyleTag(IHTMLDocument2* document,
                                             IHTMLDOMNode* head_node,
                                             const wchar_t* code) {
  return DomUtils::InjectStyleTag(document, head_node, code);
}

HRESULT ContentScriptManager::Initialize(IFrameEventHandlerHost* host,
                                         bool require_all_frames) {
  DCHECK(host != NULL);
  DCHECK(host_ == NULL);
  host_ = host;

  require_all_frames_ = require_all_frames;

  return S_OK;
}

HRESULT ContentScriptManager::TearDown() {
  if (native_api_ != NULL) {
    CComPtr<ICeeeContentScriptNativeApi> native_api;
    native_api_.QueryInterface(&native_api);
    if (native_api != NULL) {
      ContentScriptNativeApi* implementation =
          static_cast<ContentScriptNativeApi*>(native_api.p);
      // Teardown will release references from ContentScriptNativeApi to
      // objects blocking release of BHO. Somehow ContentScriptNativeApi is
      // alive after IScriptHost::Close().
      implementation->TearDown();
    }
    native_api_.Release();
  }
  HRESULT hr = S_OK;
  if (script_host_ != NULL) {
    hr = script_host_->Close();
    LOG_IF(ERROR, FAILED(hr)) << "ScriptHost::Close failed " << com::LogHr(hr);

    CComQIPtr<IObjectWithSite> script_host_with_site(script_host_);
    DCHECK(script_host_with_site != NULL);
    hr = script_host_with_site->SetSite(NULL);
    DCHECK(SUCCEEDED(hr));
  }

  // TODO(siggi@chromium.org): Kill off open extension ports.

  script_host_.Release();

  return hr;
}
