// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// @file
// Frame event handler declaration.

#ifndef CEEE_IE_PLUGIN_BHO_FRAME_EVENT_HANDLER_H_
#define CEEE_IE_PLUGIN_BHO_FRAME_EVENT_HANDLER_H_

#include <atlbase.h>
#include <atlcom.h>
#include <mshtml.h>  // Must be before <exdisp.h>
#include <exdisp.h>
#include <ocidl.h>
#include <objidl.h>

#include <list>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "ceee/ie/plugin/scripting/content_script_manager.h"
#include "ceee/ie/plugin/scripting/userscripts_librarian.h"
#include "ceee/common/initializing_coclass.h"

#include "toolband.h"  // NOLINT

// Error code to signal that a browser has a non-MSHTML document attached.
const HRESULT E_DOCUMENT_NOT_MSHTML =
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x200);

// This is the interface the frame event handler presents to its host.
// The BHO maintains a mapping of browsers to frame event handlers,
// and takes care of notifying frame event handlers when they acquire
// a subframe.
extern const GUID IID_IFrameEventHandler;
class IFrameEventHandler: public IUnknown {
 public:
  // Get the frame's current URL.
  virtual void GetUrl(BSTR* url) = 0;

  // Notify the frame event handler of the associated browser's current URL.
  virtual HRESULT SetUrl(BSTR url) = 0;

  // Returns the current document ready state.
  virtual READYSTATE GetReadyState() = 0;

  // Notify the frame event handler that |handler| has been attached
  // to an immediate sub-browser of the browser it's attached to.
  virtual HRESULT AddSubHandler(IFrameEventHandler* handler) = 0;
  // Notify the frame event handler that |handler| has detached from
  // an immediate sub-browser of the browser it's attached to.
  virtual HRESULT RemoveSubHandler(IFrameEventHandler* handler) = 0;

  // A parent frame handler has seen a readystate drop, indicating
  // that our associated browser instance has gone out of scope.
  // @pre this frame event handler is attached to a browser.
  virtual void TearDown() = 0;

  // Insert code inside a tab whether by execution or injection.
  // @param code The code to insert.
  // @param file A file containing the code to insert.
  // @param type The type of the code to insert.
  virtual HRESULT InsertCode(BSTR code, BSTR file,
                             CeeeTabCodeType type) = 0;

  // Re-does any injections of code or CSS that should have been already done.
  // Called by the host when extensions have been loaded, as before then we
  // don't have details on which scripts to load.
  virtual void RedoDoneInjections() = 0;
};

// Fwd.
class IExtensionPortMessagingProvider;

// The interface presented to a frame event handler by its host.
extern const GUID IID_IFrameEventHandlerHost;
class IFrameEventHandlerHost: public IUnknown {
 public:
  // Notify the host that |handler| has attached to |browser|,
  // whose parent browser is |parent_browser|.
  virtual HRESULT AttachBrowser(IWebBrowser2* browser,
                                IWebBrowser2* parent_browser,
                                IFrameEventHandler* handler) = 0;
  // Notify the host that |handler| has detached from |browser|.
  // whose parent browser is |parent_browser|.
  virtual HRESULT DetachBrowser(IWebBrowser2* browser,
                                IWebBrowser2* parent_browser,
                                IFrameEventHandler* handler) = 0;
  // Returns the top level browser associated to this frame event handler.
  virtual HRESULT GetTopLevelBrowser(IWebBrowser2** browser) = 0;

  // Notify the host that our ready state has changed.
  virtual HRESULT OnReadyStateChanged(READYSTATE ready_state) = 0;

  // Get the current ready state of the host.
  virtual HRESULT GetReadyState(READYSTATE* ready_state) = 0;

  // Retrieve the CSS content from user scripts that match @p url.
  // @param url The URL to match.
  // @param require_all_frames Whether to require the all_frames property of the
  //        user script to be true.
  // @param css_content The single stream of CSS content.
  virtual HRESULT GetMatchingUserScriptsCssContent(
      const GURL& url, bool require_all_frames, std::string* css_content) = 0;

  // Retrieve the JS content from user scripts that match @p url.
  // @param url The URL to match.
  // @param location The location where the scripts will be run at.
  // @param require_all_frames Whether to require the all_frames property of the
  //        user script to be true.
  // @param js_file_list A vector of file path/content pairs.
  virtual HRESULT GetMatchingUserScriptsJsContent(
      const GURL& url, UserScript::RunLocation location,
      bool require_all_frames,
      UserScriptsLibrarian::JsFileList* js_file_list) = 0;

  // Retrieve our extension ID.
  // @param extension_id on success returns the extension id.
  virtual HRESULT GetExtensionId(std::wstring* extension_id) = 0;

  // Retrieve our extension base dir.
  // @param extension_path on success returns the extension base dir.
  virtual HRESULT GetExtensionPath(std::wstring* extension_path) = 0;

  // Retrieve the native API host.
  // @param host on success returns the native API host.
  virtual HRESULT GetExtensionPortMessagingProvider(
      IExtensionPortMessagingProvider** messaging_provider) = 0;

  // Execute the given code or file in the top level frame or all frames.
  // Note that only one of code or file can be non-empty.
  // @param code The script to execute.
  // @param file A file containing the script to execute.
  // @param all_frames If true, applies to the top level frame as well as
  //                   contained iframes.  Otherwise, applies only to the
  //                   top level frame.
  // @param type The type of the code to insert.
  virtual HRESULT InsertCode(BSTR code, BSTR file, BOOL all_frames,
                             CeeeTabCodeType type) = 0;
};

// The frame event handler is attached to an IWebBrowser2 instance, either
// a top-level instance or sub instances associated with frames.
// It is responsible for listening for events from the associated frame in
// order to e.g. instantiate content scripts that interact with the frame.
class FrameEventHandler
    : public CComObjectRootEx<CComSingleThreadModel>,
      public InitializingCoClass<FrameEventHandler>,
      public IPropertyNotifySink,
      public IAdviseSink,
      public IFrameEventHandler {
 public:
  BEGIN_COM_MAP(FrameEventHandler)
    COM_INTERFACE_ENTRY(IPropertyNotifySink)
    COM_INTERFACE_ENTRY(IAdviseSink)
    COM_INTERFACE_ENTRY_IID(IID_IFrameEventHandler, IFrameEventHandler)
  END_COM_MAP()
  DECLARE_PROTECT_FINAL_CONSTRUCT();

  FrameEventHandler();
  virtual ~FrameEventHandler();

  // Initialize the event handler.
  // @returns S_OK on success, E_DOCUMENT_NOT_MSHTML if the browser
  //    is not attached to an MSTHML document instance.
  HRESULT Initialize(IWebBrowser2* browser,
                     IWebBrowser2* parent_browser,
                     IFrameEventHandlerHost* host);
  void FinalRelease();

  // @name IPropertyNotifySink implementation
  // @{
  STDMETHOD(OnChanged)(DISPID property_disp_id);
  STDMETHOD(OnRequestEdit)(DISPID property_disp_id);
  // @}

  // @name IAdviseSink implementation
  // @{
  STDMETHOD_(void, OnDataChange)(FORMATETC* format, STGMEDIUM* storage);
  STDMETHOD_(void, OnViewChange)(DWORD aspect, LONG index);
  STDMETHOD_(void, OnRename)(IMoniker* moniker);
  STDMETHOD_(void, OnSave)();
  // We use this event to tear down
  STDMETHOD_(void, OnClose)();
  // @}

  // @name IFrameEventHandler implementation.
  // @{
  virtual void GetUrl(BSTR* url);
  virtual HRESULT SetUrl(BSTR url);
  virtual READYSTATE GetReadyState() { return document_ready_state_; }
  virtual HRESULT AddSubHandler(IFrameEventHandler* handler);
  virtual HRESULT RemoveSubHandler(IFrameEventHandler* handler);
  virtual void TearDown();
  virtual HRESULT InsertCode(BSTR code, BSTR file, CeeeTabCodeType type);
  virtual void RedoDoneInjections();
  // @}

  BSTR browser_url() const { return browser_url_; }

 protected:
  // Reinitialize state on a readystate drop to LOADING, which
  // signifies that either our associated browser is being refreshed
  // or is being re-navigated.
  void ReInitialize();

  // Issues a teardown call to all sub frame handlers.
  void TearDownSubHandlers();

  // Creates and initializes the content script manager for this handler.
  // This method is virtual to allow overriding by tests.
  virtual void InitializeContentScriptManager();

  // Reads the contents of an extension resource. The file path is assumed
  // to be relative to the root of the extension.
  virtual HRESULT GetExtensionResourceContents(const FilePath& file,
                                               std::string* contents);

  // Validates and returns the code content of either code or file.
  // Used by ExecuteScript and InsertCss.
  virtual HRESULT GetCodeOrFileContents(BSTR code, BSTR file,
                                        std::wstring* contents);

  // Handle a ready state change from document_ready_state_ to new_ready_state.
  virtual void HandleReadyStateChange(READYSTATE old_ready_state,
                                      READYSTATE new_ready_state);

  // Change the current document ready state to new_ready_state
  // and invoke HandleReadyStateChange if the ready state changed.
  void SetNewReadyState(READYSTATE new_ready_state);

  // Retrieves our document's ready state.
  HRESULT GetDocumentReadyState(READYSTATE* ready_state);

  // Inject CSS for @p match_url.
  virtual void LoadCss(const GURL& match_url);

  // Inject start scripts for @p match_url.
  virtual void LoadStartScripts(const GURL& match_url);

  // Inject end scripts for @p match_url.
  virtual void LoadEndScripts(const GURL& match_url);

  // Subscribes for events etc.
  // @pre document_ is non-NULL and implements IHTMLDocument2.
  HRESULT AttachToHtmlDocument(IWebBrowser2* browser,
                               IWebBrowser2* parent_browser,
                               IFrameEventHandlerHost* host);

  // Sentinel value for non-subscribed cookies.
  static const DWORD kInvalidCookie = -1;

  // Connection cookie for IPropertyNotifySink connection point.
  DWORD property_notify_sink_cookie_;

  // Connection cookie for IAdviseSink subscription.
  DWORD advise_sink_cookie_;

  // The browser we're attached to.
  CComPtr<IWebBrowser2> browser_;

  // Our parent browser, if any.
  CComPtr<IWebBrowser2> parent_browser_;

  // The current URL browser_ is navigated or navigating to.
  CComBSTR browser_url_;

  // Our host object.
  CComPtr<IFrameEventHandlerHost> host_;

  // The document object of browser_, but only if it implements
  // IHTMLDocument2 - e.g. is an HTML document object.
  CComPtr<IHTMLDocument2> document_;

  // The last recorded document_ ready state.
  READYSTATE document_ready_state_;

  // True iff we've initialized debugging.
  bool initialized_debugging_;

  // Each of these is true iff we've attempted content script
  // CSS/start/end script injection.
  bool loaded_css_;
  bool loaded_start_scripts_;
  bool loaded_end_scripts_;

  // Our content script manager.
  scoped_ptr<ContentScriptManager> content_script_manager_;

  typedef std::set<CAdapt<CComPtr<IFrameEventHandler> > > SubHandlerSet;
  // The sub frames handlers we've been advised of by our host.
  SubHandlerSet sub_handlers_;

  struct DeferredInjection {
    std::wstring code;
    std::wstring file;
    CeeeTabCodeType type;
  };

  // Injections we deferred until extension information is available.
  std::list<DeferredInjection> deferred_injections_;

  DISALLOW_COPY_AND_ASSIGN(FrameEventHandler);
};

#endif  // CEEE_IE_PLUGIN_BHO_FRAME_EVENT_HANDLER_H_
