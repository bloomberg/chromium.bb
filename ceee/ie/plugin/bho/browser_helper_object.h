// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// IE browser helper object implementation.
#ifndef CEEE_IE_PLUGIN_BHO_BROWSER_HELPER_OBJECT_H_
#define CEEE_IE_PLUGIN_BHO_BROWSER_HELPER_OBJECT_H_

#include <atlbase.h>
#include <atlcom.h>
#include <mshtml.h>  // Needed for exdisp.h
#include <exdisp.h>
#include <exdispid.h>
#include <deque>
#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/scoped_ptr.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_comptr.h"
#include "base/task.h"
#include "ceee/ie/broker/broker_rpc_client.h"
#include "ceee/ie/plugin/bho/tab_events_funnel.h"
#include "ceee/ie/common/chrome_frame_host.h"
#include "ceee/ie/common/rgs_helper.h"
#include "ceee/ie/plugin/bho/frame_event_handler.h"
#include "ceee/ie/plugin/bho/extension_port_manager.h"
#include "ceee/ie/plugin/bho/tool_band_visibility.h"
#include "ceee/ie/plugin/bho/web_browser_events_source.h"
#include "ceee/ie/plugin/bho/web_progress_notifier.h"
#include "ceee/ie/plugin/scripting/userscripts_librarian.h"
#include "ceee/ie/plugin/scripting/content_script_native_api.h"
#include "ceee/ie/plugin/toolband/resource.h"

#include "broker_lib.h"  // NOLINT
#include "toolband.h"  // NOLINT

struct IObjectWithSite;

// Implementation of an IE browser helper object.
class ATL_NO_VTABLE BrowserHelperObject
    : public CComObjectRootEx<CComSingleThreadModel>,
      public CComCoClass<BrowserHelperObject, &CLSID_BrowserHelperObject>,
      public IObjectWithSiteImpl<BrowserHelperObject>,
      public IDispEventSimpleImpl<0,
                                  BrowserHelperObject,
                                  &DIID_DWebBrowserEvents2>,
      public IPersistImpl<BrowserHelperObject>,
      public IFrameEventHandlerHost,
      public IExtensionPortMessagingProvider,
      public IChromeFrameHostEvents,
      public ICeeeBho,
      public ToolBandVisibility,
      public WebBrowserEventsSource {
 public:
  DECLARE_REGISTRY_RESOURCEID_EX(IDR_BROWSERHELPEROBJECT)
  BEGIN_REGISTRY_MAP(BrowserHelperObject)
    REGMAP_UUID("CLSID", CLSID_BrowserHelperObject)
    REGMAP_RESOURCE("NAME", IDS_CEEE_BHO_NAME)
  END_REGISTRY_MAP()

  DECLARE_NOT_AGGREGATABLE(BrowserHelperObject)

  BEGIN_COM_MAP(BrowserHelperObject)
    COM_INTERFACE_ENTRY(IObjectWithSite)
    COM_INTERFACE_ENTRY(IPersist)
    COM_INTERFACE_ENTRY(ICeeeBho)
    COM_INTERFACE_ENTRY_IID(IID_IFrameEventHandlerHost, IFrameEventHandlerHost)
  END_COM_MAP()

  DECLARE_PROTECT_FINAL_CONSTRUCT()

  BEGIN_SINK_MAP(BrowserHelperObject)
    SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_BEFORENAVIGATE2,
                    OnBeforeNavigate2,
                    &handler_type_idispatch_5variantptr_boolptr_)
    SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_DOCUMENTCOMPLETE,
                    OnDocumentComplete, &handler_type_idispatch_variantptr_)
    SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_NAVIGATECOMPLETE2,
                    OnNavigateComplete2, &handler_type_idispatch_variantptr_)
    SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_NAVIGATEERROR,
                    OnNavigateError,
                    &handler_type_idispatch_3variantptr_boolptr_)
    SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_NEWWINDOW2,
                    OnNewWindow2, &handler_type_idispatchptr_boolptr_)
    SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_NEWWINDOW3,
                    OnNewWindow3,
                    &handler_type_idispatchptr_boolptr_dword_2bstr_)
  END_SINK_MAP()

  BrowserHelperObject();
  ~BrowserHelperObject();

  HRESULT FinalConstruct();
  void FinalRelease();

  // @name IObjectWithSite override.
  STDMETHOD(SetSite)(IUnknown* site);

  // @name IExtensionPortMessagingProvider implementation
  // @{
  virtual void CloseAll(IContentScriptNativeApi* instance);
  virtual HRESULT OpenChannelToExtension(IContentScriptNativeApi* instance,
                                         const std::string& extension,
                                         const std::string& channel_name,
                                         int cookie);
  virtual HRESULT PostMessage(int port_id, const std::string& message);
  // @}

  // @name IChromeFrameHostEvents implementation
  virtual HRESULT OnCfReadyStateChanged(LONG state);
  virtual HRESULT OnCfPrivateMessage(BSTR msg, BSTR origin, BSTR target);
  virtual HRESULT OnCfExtensionReady(BSTR path, int response);
  virtual HRESULT OnCfGetEnabledExtensionsComplete(
      SAFEARRAY* tab_delimited_paths);
  virtual HRESULT OnCfGetExtensionApisToAutomate(BSTR* functions_enabled);
  virtual HRESULT OnCfChannelError();

  // @name WebBrowser event handlers
  // @{
  STDMETHOD_(void, OnBeforeNavigate2)(IDispatch* webbrowser_disp, VARIANT* url,
                                      VARIANT* flags,
                                      VARIANT* target_frame_name,
                                      VARIANT* post_data, VARIANT* headers,
                                      VARIANT_BOOL* cancel);
  STDMETHOD_(void, OnDocumentComplete)(IDispatch* webbrowser_disp,
                                       VARIANT* url);
  STDMETHOD_(void, OnNavigateComplete2)(IDispatch* webbrowser_disp,
                                        VARIANT* url);
  STDMETHOD_(void, OnNavigateError)(IDispatch* webbrowser_disp, VARIANT* url,
                                    VARIANT* target_frame_name,
                                    VARIANT* status_code, VARIANT_BOOL* cancel);
  STDMETHOD_(void, OnNewWindow2)(IDispatch** webbrowser_disp,
                                 VARIANT_BOOL* cancel);
  STDMETHOD_(void, OnNewWindow3)(IDispatch** webbrowser_disp,
                                 VARIANT_BOOL* cancel, DWORD flags,
                                 BSTR url_context, BSTR url);
  // @}

  // @name IFrameEventHandlerHost
  // @{
  virtual HRESULT AttachBrowser(IWebBrowser2* browser,
                                IWebBrowser2* parent_browser,
                                IFrameEventHandler* handler);
  virtual HRESULT DetachBrowser(IWebBrowser2* browser,
                                IWebBrowser2* parent_browser,
                                IFrameEventHandler* handler);
  virtual HRESULT GetTopLevelBrowser(IWebBrowser2** browser);
  virtual HRESULT GetMatchingUserScriptsCssContent(
      const GURL& url, bool require_all_frames, std::string* css_content);
  virtual HRESULT GetMatchingUserScriptsJsContent(
      const GURL& url, UserScript::RunLocation location,
      bool require_all_frames,
      UserScriptsLibrarian::JsFileList* js_file_list);
  virtual HRESULT OnReadyStateChanged(READYSTATE ready_state);
  virtual HRESULT GetReadyState(READYSTATE* ready_state);
  virtual HRESULT GetExtensionId(std::wstring* extension_id);
  virtual HRESULT GetExtensionPath(std::wstring* extension_path);
  virtual HRESULT GetExtensionPortMessagingProvider(
      IExtensionPortMessagingProvider** messaging_provider);
  virtual HRESULT InsertCode(BSTR code, BSTR file, BOOL all_frames,
                             CeeeTabCodeType type);
  // @}

  // @name WebBrowserEventsSource
  // @{
  // Both RegisterSink and UnregisterSink are supposed to be called from the
  // main browser thread of the tab to which this BHO is attached. Sinks will
  // receive notifications on the same thread.
  virtual void RegisterSink(Sink* sink);
  virtual void UnregisterSink(Sink* sink);
  // @}

  // @name ICeeeBho implementation
  // @{
  STDMETHOD(SetToolBandSessionId)(long session_id);
  // @}

 protected:
  typedef base::win::ScopedComPtr<IContentScriptNativeApi, &GUID_NULL>
      ScopedContentScriptNativeApiPtr;
  typedef base::win::ScopedComPtr<IDispatch> ScopedDispatchPtr;
  typedef base::win::ScopedComPtr<IWebBrowser2> ScopedWebBrowser2Ptr;
  typedef base::win::ScopedComPtr<IUnknown> ScopedIUnkPtr;
  typedef base::win::ScopedComPtr<IFrameEventHandler, &IID_IFrameEventHandler>
      ScopedFrameEventHandlerPtr;
  typedef base::win::ScopedComPtr<IChromeFrameHost, &IID_IChromeFrameHost>
      ScopedChromeFramePtr;

  // Register proxy/stubs for executor interfaces.
  virtual HRESULT RegisterProxies();
  // Unregister proxy/stubs for executor interfaces.
  virtual void UnregisterProxies();

  // Implement the real call for OpenChannelToExtension and PostMessage.
  HRESULT OpenChannelToExtensionImpl(ScopedContentScriptNativeApiPtr instance,
                                     const std::string& extension,
                                     const std::string& channel_name,
                                     int cookie);
  HRESULT PostMessageImpl(int port_id, const std::string& message);

  // Finds the handler attached to webbrowser.
  // @returns S_OK if handler is found.
  HRESULT GetBrowserHandler(IWebBrowser2* webbrowser,
                            IFrameEventHandler** handler);

  virtual void HandleNavigateComplete(IWebBrowser2* webbrowser, BSTR url);

  // Unit testing seam to create the frame event handler.
  virtual HRESULT CreateFrameEventHandler(IWebBrowser2* browser,
                                          IWebBrowser2* parent_browser,
                                          IFrameEventHandler** handler);

  // Unit testing seam to connect to Broker RPC server.
  virtual HRESULT ConnectRpcBrokerClient();

  // Unit testing seam to get the parent of a browser.
  virtual HRESULT GetParentBrowser(IWebBrowser2* browser,
                                   IWebBrowser2** parent_browser);

  // Unit testing seam to create the broker registrar.
  // Release registrar ASAP. Cached reference may block broker from release.
  virtual HRESULT GetBrokerRegistrar(ICeeeBrokerRegistrar** broker);

  // Unit testing seam to create an executor.
  virtual HRESULT CreateExecutor(IUnknown** executor);

  // Unit testing seam to create a WebProgressNotifier instance.
  virtual WebProgressNotifier* CreateWebProgressNotifier();

  // Initializes the BHO to the given site.
  // Called from SetSite.
  // @note On failure the state of the BHO may be inconsistent,
  //    so a TearDown may be needed.
  HRESULT Initialize(IUnknown* site);

  // Tears down an initialized bho.
  // Called from SetSite.
  HRESULT TearDown();

  // Creates and initializes the chrome frame host.
  HRESULT InitializeChromeFrameHost();

  // Fetch and remembers the tab window we are attached to.
  // Virtual for testing purposes.
  virtual HRESULT GetTabWindow(IServiceProvider* service_provider);

  // Connect for notifications.
  HRESULT ConnectSinks(IServiceProvider* service_provider);

  // Isolate the creation of the host so we can overload it to mock
  // the Chrome Frame Host in our tests.
  virtual HRESULT CreateChromeFrameHost();

  // Accessor so that we can mock it in unit tests.
  virtual TabEventsFunnel& tab_events_funnel() { return tab_events_funnel_; }

  // Accessor so that we can mock it in unit tests.
  virtual IEventSender* broker_client() { return &broker_client_queue_; }

  // Accessor so that we can mock it in unit tests.
  virtual BrokerRpcClient& broker_rpc() { return broker_rpc_; }

  // Fires the private message to map a tab id to its associated tab handle.
  virtual HRESULT FireMapTabIdToHandle();

  // Fires the private message to map a tool band id to its associated tab
  // handle.
  virtual HRESULT FireMapToolbandIdToHandle(int toolband_id);

  // Fires the tab.onCreated event via the tab event funnel.
  virtual void FireOnCreatedEvent(BSTR url);

  // Fires the tab.onUpdated event via the tab event funnel.
  virtual void FireOnUpdatedEvent(BSTR url, READYSTATE ready_state);

  // Fires the tab.onRemoved event via the tab event funnel.
  virtual void FireOnRemovedEvent();

  // Fires the private message to unmap a tab to its BHO.
  virtual void FireOnUnmappedEvent();

  // Loads our manifest and initialize our librarian.
  virtual void LoadManifestFile(const std::wstring& base_dir);

  // Called when we know the base directory of our extension.
  void StartExtension(const wchar_t* base_dir);

  // Our ToolBandVisibility window maintains a refcount on us for the duration
  // of its lifetime. The self-reference is managed with these two methods.
  virtual void OnFinalMessage(HWND window);
  virtual LRESULT OnCreate(LPCREATESTRUCT lpCreateStruct);

  // Compares two URLs and returns whether they represent a hash change.
  virtual bool IsHashChange(BSTR url1, BSTR url2);

  // Ensure that the tab ID is correct. On the first time it's set, it will
  // call all deferred methods added to deferred_tab_id_call_.
  // This method should be called by every method that send a message or use
  // the tab event funnel, as they need the tab_id to be mapped.
  // If this method returns false, the caller should defer itself using the
  // deferred_tab_id_call_ list.
  virtual bool EnsureTabId();

  // Returns true if the browser interface passed in contains a full tab
  // chrome frame.
  virtual bool BrowserContainsChromeFrame(IWebBrowser2* browser);

  // Attach ourselves and the event handler to the browser, and launches the
  // right events when going to and from a Full Tab Chrome Frame.
  virtual HRESULT AttachBrowserHandler(IWebBrowser2* webbrowser,
                                       IFrameEventHandler** handler);

  // Helpers to call function in registrar with following release of broker.
  HRESULT RegisterTabExecutor(DWORD thread_id, IUnknown* executor);
  HRESULT UnregisterExecutor(DWORD thread_id);
  HRESULT SetTabIdForHandle(int tool_band_id, long tab_handle);
  HRESULT SetTabToolBandIdForHandle(int tool_band_id, long tab_handle);

  // Function info objects describing our message handlers.
  // Effectively const but can't make const because of silly ATL macro problem.
  static _ATL_FUNC_INFO handler_type_idispatch_5variantptr_boolptr_;
  static _ATL_FUNC_INFO handler_type_idispatch_variantptr_;
  static _ATL_FUNC_INFO handler_type_idispatch_3variantptr_boolptr_;
  static _ATL_FUNC_INFO handler_type_idispatchptr_boolptr_;
  static _ATL_FUNC_INFO handler_type_idispatchptr_boolptr_dword_2bstr_;

  // The top-level web browser (window) we're attached to. NULL before SetSite.
  ScopedWebBrowser2Ptr web_browser_;

  // The Chrome Frame host handling a Chrome Frame instance for us.
  ScopedChromeFramePtr chrome_frame_host_;

  // We keep a reference to the executor we registered so that we can
  // manually disconnect it, so it doesn't get called while we unregister it.
  ScopedIUnkPtr executor_;

  // Maintains a map from browser (top-level and sub-browsers) to the
  // attached FrameEventHandlers.
  typedef std::map<ScopedIUnkPtr, ScopedFrameEventHandlerPtr> BrowserHandlerMap;
  BrowserHandlerMap browsers_;

  // Initialized by LoadManifestFile() at
  // OnCfGetEnabledExtensionsComplete-time. Valid from that point forward.
  UserScriptsLibrarian librarian_;

  // Filesystem path to the .crx we will install (or have installed), or the
  // empty string, or (if not ending in .crx) the path to an exploded extension
  // directory to load (or which we have loaded).
  std::wstring extension_path_;

  // The extension we're associated with.  Set at
  // OnCfGetEnabledExtensionsComplete-time.
  // TODO(siggi@chromium.org): Generalize this to multiple extensions.
  std::wstring extension_id_;

  // The base directory of the extension we're associated with.
  // Set at OnCfGetEnabledExtensionsComplete time.
  std::wstring extension_base_dir_;

  // Extension port messaging and management is delegated to this.
  ExtensionPortManager extension_port_manager_;

  // Remember the tab window handle so that we can use it.
  HWND tab_window_;

  // Remember the tab id so we can pass it to the underlying Chrome.
  int tab_id_;

  // Makes sure we fire the onCreated event only once.
  bool fired_on_created_event_;

  // True if we found no enabled extensions and tried to install one.
  bool already_tried_installing_;

  // The last known ready state lower bound, so that we decide when to fire a
  // tabs.onUpdated event, which only when we go from all frames completed to
  // at least one of them not completed, and vice versa (from incomplete to
  // fully completely completed :-)...
  READYSTATE lower_bound_ready_state_;

  // Consumers of WebBrowser events.
  std::vector<Sink*> sinks_;

  // Used to generate and fire Web progress notifications.
  scoped_ptr<WebProgressNotifier> web_progress_notifier_;

  // True if the user is running IE7 or later.
  bool ie7_or_later_;

  // The thread we are running into.
  DWORD thread_id_;

  // Indicates if the current shown page is a full-tab chrome frame.
  bool full_tab_chrome_frame_;

  // The RPC client used to communicate with the broker.
  BrokerRpcClient broker_rpc_;

 private:
  // The BHO registers proxy/stubs for the CEEE executor on initialization.
  // These are the cookies to allow us to unregister then on teardown.
  std::vector<DWORD> proxy_stub_cookies_;

  // Used during initialization to get the tab information from Chrome and
  // register ourselves with the broker.
  HRESULT RegisterTabInfo();

  // Check if the first parameter belongs to the browser tree rooted at
  // root_browser.
  HRESULT VerifyBrowserInHierarchy(IWebBrowser2* webbrowser,
                                   IWebBrowser2* root_browser);

  // Send metrics histograms about the average load time of IE addons.
  void ReportAddonTimes(const char* name, const CLSID& clsid);
  void ReportSingleAddonTime(const char* name,
                             const CLSID& clsid,
                             const char* type);

  // This class is used as a replacement for the BrokerRpcClient for the
  // event funnels contained inside a BHO. When sending an event, this will
  // forward it to the BHO which will queue it until the tab_id is available,
  // then simply forward all events directly to the Broker in the order they
  // were sent to this queue. We don't do the whole queueing here for
  // simplicity.
  // TODO(hansl@chromium.org): This should be a generic class in base. Really
  // useful elsewhere in CEEE and maybe elsewhere.
  class BrokerEventQueue : public IEventSender {
   public:
    explicit BrokerEventQueue(BrowserHelperObject* bho) : bho_(bho) {}

    // If the BHO outlived the queue, we send it the events.
    HRESULT FireEvent(const char* event_name, const char* event_args) {
      if (bho_) {
        return bho_->SendEventToBroker(event_name, event_args);
      }
      return S_OK;
    }

   private:
    BrowserHelperObject* bho_;
    DISALLOW_COPY_AND_ASSIGN(BrokerEventQueue);
  };

  // This synchronously send the association between the given ID and our
  // current tab handle using the given event name.
  HRESULT SendIdMappingToBroker(const char* event_name, int id);

  // This either calls its Impl or queues the call for later, when we have a
  // tab_id.
  HRESULT SendEventToBroker(const char* event_name, const char* event_args);

  // Sends the event to the broker, for real, without delay, right now.
  HRESULT SendEventToBrokerImpl(const std::string& event_name,
                                const std::string& event_args);

  // List of calls to send to the broker.
  typedef std::deque<Task*> DeferredCallListType;
  DeferredCallListType deferred_events_call_;

  // This is passed to every funnel so they use the queue for sending events.
  BrokerEventQueue broker_client_queue_;

  // Used to dispatch tab events back to Chrome.
  TabEventsFunnel tab_events_funnel_;

  // List of BHOs which could be unregistered from IE and loaded by
  // CEEE instead.
  std::vector<base::win::ScopedComPtr<IObjectWithSite> > nested_bho_;
};

#endif  // CEEE_IE_PLUGIN_BHO_BROWSER_HELPER_OBJECT_H_
