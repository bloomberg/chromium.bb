// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_CHROME_FRAME_NPAPI_H_
#define CHROME_FRAME_CHROME_FRAME_NPAPI_H_

#include <atlbase.h>
#include <atlwin.h>
#include <string>

#include "chrome_frame/chrome_frame_automation.h"
#include "chrome_frame/chrome_frame_plugin.h"
#include "chrome_frame/np_browser_functions.h"
#include "chrome_frame/np_event_listener.h"
#include "chrome_frame/np_proxy_service.h"
#include "chrome_frame/npapi_url_request.h"

class MessageLoop;

// ChromeFrameNPAPI: Implementation of the NPAPI plugin, which is responsible
// for hosting a chrome frame, i.e. an iframe like widget which hosts the the
// chrome window. This object delegates to Chrome.exe  (via the Chrome
// IPC-based automation mechanism) for the actual rendering.
class ChromeFrameNPAPI
    : public CWindowImpl<ChromeFrameNPAPI>,
      public ChromeFramePlugin<ChromeFrameNPAPI>,
      public NpEventDelegate {
 public:
  typedef ChromeFramePlugin<ChromeFrameNPAPI> Base;

  // NPObject structure which is exposed by us.
  struct ChromeFrameNPObject : public NPObject {
    NPP npp;
    ChromeFrameNPAPI* chrome_frame_plugin_instance;
  };

  typedef enum {
    PLUGIN_PROPERTY_VERSION,
    PLUGIN_PROPERTY_SRC,
    PLUGIN_PROPERTY_ONLOAD,
    PLUGIN_PROPERTY_ONERROR,
    PLUGIN_PROPERTY_ONMESSAGE,
    PLUGIN_PROPERTY_READYSTATE,
    PLUGIN_PROPERTY_ONPRIVATEMESSAGE,
    PLUGIN_PROPERTY_USECHROMENETWORK,
    PLUGIN_PROPERTY_COUNT  // must be last
  } PluginPropertyId;

  static const int kWmSwitchFocusToChromeFrame = WM_APP + 0x100;

  static NPClass plugin_class_;
  static NPClass* PluginClass() {
    return &plugin_class_;
  }

  ChromeFrameNPAPI();
  ~ChromeFrameNPAPI();

  bool Initialize(NPMIMEType mime_type, NPP instance, uint16 mode,
                  int16 argc, char* argn[], char* argv[]);
  void Uninitialize();

  bool SetWindow(NPWindow* window_info);
  void UrlNotify(const char* url, NPReason reason, void* notify_data);
  bool NewStream(NPMIMEType type, NPStream* stream, NPBool seekable,
                 uint16* stream_type);
  int32 WriteReady(NPStream* stream);
  int32 Write(NPStream* stream, int32 offset, int32 len, void* buffer);
  NPError DestroyStream(NPStream* stream, NPReason reason);

  void Print(NPPrint* print_info);

  // NPObject functions, which ensure that the plugin object is scriptable.
  static bool HasMethod(NPObject* obj, NPIdentifier name);
  static bool Invoke(NPObject* header, NPIdentifier name,
                     const NPVariant* args, uint32_t arg_count,
                     NPVariant* result);
  static NPObject* AllocateObject(NPP instance, NPClass* class_name);
  static void DeallocateObject(NPObject* header);

  // Called by the scripting environment when the native code is shutdown.
  // Any attempt to message a NPObject instance after the invalidate callback
  // has been called will result in undefined behavior, even if the native code
  // is still retaining those NPObject instances.
  static void Invalidate(NPObject* header);

  // The following functions need to be implemented to ensure that FF3
  // invokes methods on the plugin. If these methods are not implemented
  // then invokes on the plugin NPObject from the script fail with a
  // bad NPObject error.
  static bool HasProperty(NPObject* obj, NPIdentifier name);
  static bool GetProperty(NPObject* obj, NPIdentifier name, NPVariant *variant);
  static bool SetProperty(NPObject* obj, NPIdentifier name,
                          const NPVariant *variant);

  // Returns the ChromeFrameNPAPI object pointer from the NPP instance structure
  // passed in by the browser.
  static ChromeFrameNPAPI* ChromeFrameInstanceFromPluginInstance(NPP instance);

  // Returns the ChromeFrameNPAPI object pointer from the NPObject structure
  // which represents our plugin class.
  static ChromeFrameNPAPI* ChromeFrameInstanceFromNPObject(void* object);

BEGIN_MSG_MAP(ChromeFrameNPAPI)
  MESSAGE_HANDLER(WM_SETFOCUS, OnSetFocus)
  CHAIN_MSG_MAP(Base)
END_MSG_MAP()

  LRESULT OnSetFocus(UINT message, WPARAM wparam, LPARAM lparam,
                     BOOL& handled);  // NO_LINT

  // Implementation of NpEventDelegate
  virtual void OnEvent(const char* event_name);

  void OnFocus();
  void OnBlur();

  // Implementation of SetProperty, public to allow unittesting.
  bool SetProperty(NPIdentifier name, const NPVariant *variant);
  // Implementation of GetProperty, public to allow unittesting.
  bool GetProperty(NPIdentifier name, NPVariant *variant);

  // Initialize string->identifier mapping, public to allow unittesting.
  static void InitializeIdentifiers();

  bool HandleContextMenuCommand(UINT cmd, const IPC::ContextMenuParams& params);
 protected:
  // Handler for accelerator messages passed on from the hosted chrome
  // instance.
  virtual void OnAcceleratorPressed(int tab_handle, const MSG& accel_message);
  virtual void OnTabbedOut(int tab_handle, bool reverse);
  virtual void OnOpenURL(int tab_handle, const GURL& url, int open_disposition);
  virtual void OnLoad(int tab_handle, const GURL& url);
  virtual void OnMessageFromChromeFrame(int tab_handle,
                                        const std::string& message,
                                        const std::string& origin,
                                        const std::string& target);
  virtual void OnSetCookieAsync(int tab_handle, const GURL& url,
                                const std::string& cookie);

  // ChromeFrameDelegate overrides
  virtual void OnLoadFailed(int error_code, const std::string& url);
  virtual void OnAutomationServerReady();
  virtual void OnAutomationServerLaunchFailed(
      AutomationLaunchResult reason, const std::string& server_version);
  virtual void OnExtensionInstalled(const FilePath& path,
      void* user_data, AutomationMsg_ExtensionResponseValues response);

 private:
  void SubscribeToFocusEvents();
  void UnsubscribeFromFocusEvents();

  // Equivalent of:
  // event = window.document.createEvent("Event");
  // event.initEvent(type, bubbles, cancelable);
  // and then returns the event object.
  bool CreateEvent(const std::string& type, bool bubbles, bool cancelable,
                   NPObject** basic_event);

  // Creates and initializes an event object of type "message".
  // Used for postMessage.
  bool CreateMessageEvent(bool bubbles, bool cancelable,
      const std::string& data, const std::string& origin,
      NPObject** message_event);

  // Calls chrome_frame.dispatchEvent to fire events to event listeners.
  void DispatchEvent(NPObject* event);

  // Returns a pointer to the <object> element in the page that
  // hosts the plugin.  Note that this is the parent element of the <embed>
  // element.  The <embed> element doesn't support some of the events that
  // we require, so we use the object element for receiving events.
  bool GetObjectElement(nsIDOMElement** element);

  // Prototype for all methods that can be invoked from script.
  typedef bool (ChromeFrameNPAPI::*PluginMethod)(NPObject* npobject,
                                                 const NPVariant* args,
                                                 uint32_t arg_count,
                                                 NPVariant* result);

  // Implementations of scriptable methods.

  bool NavigateToURL(const NPVariant* args, uint32_t arg_count,
                     NPVariant* result);

  bool postMessage(NPObject* npobject, const NPVariant* args,
                   uint32_t arg_count, NPVariant* result);

  // This method is only available when the control is in privileged mode.
  bool postPrivateMessage(NPObject* npobject, const NPVariant* args,
                          uint32_t arg_count, NPVariant* result);

  // This method is only available when the control is in privileged mode.
  bool installExtension(NPObject* npobject, const NPVariant* args,
                        uint32_t arg_count, NPVariant* result);

  // This method is only available when the control is in privileged mode.
  bool loadExtension(NPObject* npobject, const NPVariant* args,
                     uint32_t arg_count, NPVariant* result);

  // This method is only available when the control is in privileged mode.
  bool enableExtensionAutomation(NPObject* npobject, const NPVariant* args,
                                 uint32_t arg_count, NPVariant* result);

  // Pointers to method implementations.
  static PluginMethod plugin_methods_[];

  // NPObject method ids exposed by the plugin.
  static NPIdentifier plugin_method_identifiers_[];

  // NPObject method names exposed by the plugin.
  static const NPUTF8* plugin_method_identifier_names_[];

  // NPObject property ids exposed by the plugin.
  static NPIdentifier plugin_property_identifiers_[];

  // NPObject property names exposed by the plugin.
  static const NPUTF8*
      plugin_property_identifier_names_[];

  virtual void OnFinalMessage(HWND window);

  // Helper function to invoke a function on a NPObject.
  bool InvokeDefault(NPObject* object, const std::string& param,
                     NPVariant* result);

  bool InvokeDefault(NPObject* object, const NPVariant& param,
                     NPVariant* result);

  bool InvokeDefault(NPObject* object, unsigned param_count,
                     const NPVariant* params, NPVariant* result);

  // Helper function to convert javascript code to a NPObject we can
  // invoke on.
  virtual NPObject* JavascriptToNPObject(const std::string& function_name);

  // Helper function to execute a script.
  // Returns S_OK on success.
  bool ExecuteScript(const std::string& script, NPVariant* result);

  // Returns true if the script passed in is a valid function in the DOM.
  bool IsValidJavascriptFunction(const std::string& script);

  // Converts the data parameter to an NPVariant and forwards the call to the
  // other FireEvent method.
  void FireEvent(const std::string& event_type, const std::string& data);

  // Creates an event object, assigns the data parameter to a |data| property
  // on the event object and then calls DispatchEvent to fire the event to
  // listeners.  event_type is the name of the event being fired.
  void FireEvent(const std::string& event_type, const NPVariant& data);

  // Returns a new prefs service. Virtual to allow overriding in unittests.
  virtual NpProxyService* CreatePrefService();

  // Returns our associated windows' location.
  virtual std::string GetLocation();

  // Returns true iff we're successfully able to query for the browser's
  // incognito mode, and the browser returns true.
  virtual bool GetBrowserIncognitoMode();

  // Returns the window script object for the page.
  // This function will cache the window object to avoid calling
  // npapi::GetValue which can cause problems in Opera.
  NPObject* GetWindowObject() const;

  virtual void SetReadyState(READYSTATE new_state) {
    ready_state_ = new_state;
    NPVariant var;
    INT32_TO_NPVARIANT(ready_state_, var);
    FireEvent("readystatechanged", var);
  }

  // Host function to compile-time asserts over members of this class.
  static void CompileAsserts();

  static LRESULT CALLBACK DropKillFocusHook(int code, WPARAM wparam,
                                            LPARAM lparam);  // NO_LINT

  // The plugins opaque instance handle
  NPP instance_;

  // The plugin instantiation mode (NP_FULL or NP_EMBED)
  int16 mode_;
  // The plugins mime type.
  std::string mime_type_;

  // Set to true if we need a full page plugin.
  bool force_full_page_plugin_;

  scoped_refptr<NpProxyService> pref_service_;

  // Used to receive focus and blur events from the object element
  // that hosts the plugin.
  scoped_refptr<NpEventListener> focus_listener_;

  // In some cases the IPC channel proxy object is instantiated on the UI
  // thread in FF. It then tries to use the IPC logger, which relies on
  // the message loop being around. Declaring a dummy message loop
  // is a hack to get around this. Eventually the automation code needs to
  // be fixed to ensure that the channel proxy always gets created on a thread
  // with a message loop.
  static MessageLoop* message_loop_;
  static int instance_count_;

  // The following members contain the NPObject pointers representing the
  // onload/onerror/onmessage handlers on the page.
  ScopedNpObject<NPObject> onerror_handler_;
  ScopedNpObject<NPObject> onmessage_handler_;
  ScopedNpObject<NPObject> onprivatemessage_handler_;

  // As a workaround for a problem in Opera we cache the window object.
  // The problem stems from two things: window messages aren't always removed
  // from the message queue and messages can be pumped inside GetValue.
  // This can cause an infinite recursion of processing the same message
  // repeatedly.
  mutable ScopedNpObject<NPObject> window_object_;

  // Note since 'onload' is a registered event name, the browser will
  // automagically create a code block for the handling code and hook it
  // up to the CF object via addEventListener.
  // See this list of known event types:
  // http://www.w3.org/TR/DOM-Level-3-Events/events.html#Event-types

  READYSTATE ready_state_;


  // Popups are enabled
  bool enabled_popups_;

  // The value of src property keeping the current URL.
  std::string src_;
  // Used to fetch network resources when host network stack is in use.
  NPAPIUrlRequestManager url_fetcher_;
};

#endif  // CHROME_FRAME_CHROME_FRAME_NPAPI_H_
