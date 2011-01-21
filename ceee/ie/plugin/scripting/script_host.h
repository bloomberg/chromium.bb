// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A host for Microsoft's JScript engine that we use to create a separate
// engine for our content scripts so that they do not run in the same
// context/namespace as the scripts that are part of the page.

#ifndef CEEE_IE_PLUGIN_SCRIPTING_SCRIPT_HOST_H_
#define CEEE_IE_PLUGIN_SCRIPTING_SCRIPT_HOST_H_

// The Platform SDK version of this header is newer than the
// Active Script SDK, and we use some newer interfaces.
#include <activscp.h>
#include <atlbase.h>
#include <atlcom.h>
#include <dispex.h>
#include <docobj.h>
#include <map>
#include <string>

#include "base/logging.h"
#include "base/synchronization/lock.h"
#include "third_party/activscp/activdbg.h"
#include "ceee/common/initializing_coclass.h"

#ifndef OLESCRIPT_E_SYNTAX
#define OLESCRIPT_E_SYNTAX    0x80020101
#endif

extern const GUID IID_IScriptHost;

// Interface for ScriptHost needed for unit testing.
class IScriptHost : public IUnknown {
 public:
  // Registers an IDispatch script object with the script host.
  // @param name The name the object will have inside the script host.
  // @param disp_obj The IDispatch object to register.
  // @param make_members_global Whether or not to make the object's members
  //     global.
  virtual HRESULT RegisterScriptObject(const wchar_t* name,
                                       IDispatch* disp_obj,
                                       bool make_members_global) = 0;

  // Run the specified script in the script host.
  // @param file_path The name/path to the file for debugging.
  // @param code The code to be executed.
  virtual HRESULT RunScript(const wchar_t* file_path, const wchar_t* code) = 0;

  // Run the specified expression in the script host and get its return value.
  // @param code The code to be executed.
  // @param result A variant to write the result to.
  virtual HRESULT RunExpression(const wchar_t* code, VARIANT* result) = 0;

  // Close the script host and release resources.
  virtual HRESULT Close() = 0;
};

// Implementation of ScriptHost class.
//
// This implements the requisite IActiveScript{Debug} interfaces necessary
// to host a active script engine and to integrate with script debugging.
// It also exposes IServiceProvider and IOleCommandTarget, which serve the
// purpose of declaring the script code origin to the IE DOM. When our scripts
// invoke on the IE DOM through IDispatchEx::InvokeEx, the last parameter is
// a service provider that leads back to this host. The IE DOM implementation
// will query this service provider for SID_GetScriptSite, and acquire an
// IOleCommandTarget on it, which in turn gets interrogated about the scripts
// origin and security properties.
class ATL_NO_VTABLE ScriptHost
    : public CComObjectRootEx<CComSingleThreadModel>,
      public InitializingCoClass<ScriptHost>,
      public IActiveScriptSite,
      public IActiveScriptSiteDebug,
      public IServiceProviderImpl<ScriptHost>,
      public IObjectWithSiteImpl<ScriptHost>,
      public IOleCommandTarget,
      public IScriptHost {
 public:
  BEGIN_COM_MAP(ScriptHost)
    COM_INTERFACE_ENTRY(IActiveScriptSite)
    COM_INTERFACE_ENTRY(IActiveScriptSiteDebug)
    COM_INTERFACE_ENTRY(IServiceProvider)
    COM_INTERFACE_ENTRY(IOleCommandTarget)
    COM_INTERFACE_ENTRY(IObjectWithSite)
    COM_INTERFACE_ENTRY_IID(IID_IScriptHost, IScriptHost)
  END_COM_MAP()
  BEGIN_SERVICE_MAP(ScriptHost)
    SERVICE_ENTRY(SID_GetScriptSite)
    // We delegate to our site object. This allows the site to provide
    // SID_SInternetHostSecurityManager, which can govern ActiveX control
    // creation and the like.
    SERVICE_ENTRY_CHAIN(m_spUnkSite)
  END_SERVICE_MAP()

  DECLARE_PROTECT_FINAL_CONSTRUCT();

  ScriptHost();

  // Fwd.
  class DebugApplication;

  // Sets the default debug application script host instaces
  // will use unless provided with another specific instance.
  // Note: a DebugApplication instance set here must outlive all
  //    ScriptHost instances in this process.
  static void set_default_debug_application(DebugApplication* debug) {
    default_debug_application_ = debug;
  }
  static DebugApplication* default_debug_application() {
    return default_debug_application_;
  }

  // Initialize with a debug application and return a pointer to self.
  // @param debug_application the debug application on whose behalf we're
  //    running.
  // @param self returns a referenceless pointer to new instance.
  HRESULT Initialize(DebugApplication* debug_application, ScriptHost** self);

  // Initialize with a debug application.
  // @param debug_application the debug application on whose behalf we're
  //    running.
  HRESULT Initialize(DebugApplication* debug_application);

  HRESULT Initialize();

  void FinalRelease();

  // IScriptHost methods.
  HRESULT RegisterScriptObject(const wchar_t* name, IDispatch* disp_obj,
                               bool make_members_global);
  HRESULT RunScript(const wchar_t* file_path, const wchar_t* code);
  HRESULT RunExpression(const wchar_t* code, VARIANT* result);
  HRESULT Close();

  // IActiveScriptSite methods.
  STDMETHOD(GetItemInfo)(LPCOLESTR item_name, DWORD return_mask,
      IUnknown** item_iunknown, ITypeInfo** item_itypeinfo);
  STDMETHOD(GetLCID)(LCID *plcid);
  STDMETHOD(GetDocVersionString)(BSTR* version);
  STDMETHOD(OnEnterScript)();
  STDMETHOD(OnLeaveScript)();
  STDMETHOD(OnStateChange)(SCRIPTSTATE state);
  STDMETHOD(OnScriptTerminate)(const VARIANT* result,
                               const EXCEPINFO* excep_info);
  STDMETHOD(OnScriptError)(IActiveScriptError* script_error);

  // IActiveScriptSiteDebug methods.
  STDMETHOD(GetDocumentContextFromPosition)(DWORD source_context,
      ULONG char_offset, ULONG num_chars,
      IDebugDocumentContext** debug_doc_context);
  STDMETHOD(GetApplication)(IDebugApplication** debug_app);
  STDMETHOD(GetRootApplicationNode)(IDebugApplicationNode** debug_app_node);
  STDMETHOD(OnScriptErrorDebug)(IActiveScriptErrorDebug* error_debug,
      BOOL* enter_debugger, BOOL* call_on_script_err_when_continuing);


  // @name IOleCommandTarget methods
  // @{
  STDMETHOD(QueryStatus)(const GUID* cmd_group,
                         ULONG num_cmds,
                         OLECMD cmds[],
                         OLECMDTEXT *cmd_text);
  STDMETHOD(Exec)(const GUID* cmd_group,
                  DWORD cmd_id,
                  DWORD cmd_exec_opt,
                  VARIANT* arg_in,
                  VARIANT* arg_out);
  // @}

  // @name Debug-only functionality.
  // @(
  // Create a debug document for @p code, which originates from @p file
  // and return it in @doc.
  // @param file_path a file path or URL to code.
  // @param code document code containing JavaScript snippets.
  // @param doc on success returns the new debug document.
  HRESULT AddDebugDocument(const wchar_t* file_path,
                           const wchar_t* code,
                           IDebugDocumentHelper** doc);

  // Run a JavaScript snippet from a previously declared document.
  // @param start_offset character offset from start of document to
  //    @p code.
  // @param code a code snippet from a document previously declared
  //    to AddDebugDocument.
  // @param doc a debug document previously received from AddDebugDocument.
  HRESULT RunScriptSnippet(size_t start_offset,
                           const wchar_t* code,
                           IDebugDocumentHelper* doc);
  // @}

 protected:
  // Virtual methods to inject dependencies for unit testing.
  virtual HRESULT CreateScriptEngine(IActiveScript** script);

 private:
  struct ScopedExcepInfo : public EXCEPINFO {
   public:
    ScopedExcepInfo() {
      bstrSource = NULL;
      bstrDescription = NULL;
      bstrHelpFile = NULL;
    }
    ~ScopedExcepInfo() {
      if (bstrSource) {
        ::SysFreeString(bstrSource);
        bstrSource = NULL;
      }
      if (bstrDescription) {
        ::SysFreeString(bstrDescription);
        bstrDescription = NULL;
      }
      if (bstrHelpFile) {
        ::SysFreeString(bstrHelpFile);
        bstrHelpFile = NULL;
      }
    }
  };

  HRESULT GetSourceContext(const wchar_t* file_path,
                           const wchar_t* code,
                           DWORD* source_context);

  // The JScript script engine. NULL until Initialize().
  CComPtr<IActiveScript> script_;

  // The JScript parser. NULL until Initialize().
  CComQIPtr<IActiveScriptParse> script_parse_;

  // A map of wstring to IDispatch pointers to hold registered script objects.
  // Empty on Initialize(). Filled using calls to RegisterScriptObject().
  typedef std::map<std::wstring, CAdapt<CComPtr<IDispatch> > > ScriptObjectMap;
  ScriptObjectMap script_objects_;

  // A map of source contexts to debug document helpers used for debugging.
  // Empty on Initialize(). Filled using calls to CreateDebugDoc().
  // Resources released and emptied on Close().
  struct DebugDocInfo {
    bool is_new;
    size_t len;
    CComPtr<IDebugDocumentHelper> document;
  };
  typedef std::map<DWORD, DebugDocInfo> DebugDocMap;
  DebugDocMap debug_docs_;

  // Our debug application state.
  DebugApplication* debug_application_;

  // Default debug application state, which is used if no other state is
  // provided at initialization time.
  static DebugApplication* default_debug_application_;
};

class ScriptHost::DebugApplication {
 public:
  DebugApplication(const wchar_t* application_name);
  ~DebugApplication();

  // Best-effort initialize process script debugging.
  // Every call to this function must be matched with a call to Terminate().
  void Initialize();

  // Best-effort initialize process script debugging.
  // @param manager_or_provider either implements IDebugApplication or
  //    IServiceProvider. Both methods will be tried in turn to aquire
  //    an IDebugApplication. If both methods fail, this will fall back
  //    to initialization by creating a new debug application.
  void Initialize(IUnknown* debug_application_provider);

  // Exposed for testing only, needs a corresponding call to Terminate.
  void Initialize(IProcessDebugManager* manager, IDebugApplication* app);

  // Terminate script debugging, call once for every call to Initialize().
  void Terminate();

  // Creates a debug document helper with the given long name, containing code.
  HRESULT CreateDebugDocumentHelper(const wchar_t* long_name,
                                    const wchar_t* code,
                                    TEXT_DOC_ATTR attributes,
                                    IDebugDocumentHelper** helper);
  // Retrieve the debug application.
  HRESULT GetDebugApplication(IDebugApplication** application);
  // Retrieve root application node.
  HRESULT GetRootApplicationNode(IDebugApplicationNode** debug_app_node);

 private:
  // Virtual for testing.
  virtual HRESULT CreateProcessDebugManager(IProcessDebugManager** manager);
  virtual HRESULT CreateDebugDocumentHelper(IDebugDocumentHelper** helper);

  // Creates and registers a new debug application for us.
  void RegisterDebugApplication();  // Under lock_.

  // Protects all members below.
  base::Lock lock_;

  // Number of initialization calls.
  size_t initialization_count_;

  // The debug manager, non-NULL only if we register
  // our own debug application.
  CComPtr<IProcessDebugManager> debug_manager_;

  // Registration cookie for debug_manager_ registration of debug_application_.
  DWORD debug_app_cookie_;

  // The debug application to be used for debugging. NULL until Initialize().
  CComPtr<IDebugApplication> debug_application_;

  static const DWORD kInvalidDebugAppCookie = 0;

  // The application name we register.
  const wchar_t* application_name_;
};

#endif  // CEEE_IE_PLUGIN_SCRIPTING_SCRIPT_HOST_H_
