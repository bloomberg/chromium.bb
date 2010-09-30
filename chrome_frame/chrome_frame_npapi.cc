// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/chrome_frame_npapi.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome_frame/ff_privilege_check.h"
#include "chrome_frame/np_utils.h"
#include "chrome_frame/scoped_ns_ptr_win.h"
#include "chrome_frame/utils.h"

MessageLoop* ChromeFrameNPAPI::message_loop_ = NULL;
int ChromeFrameNPAPI::instance_count_ = 0;

static const char* kNpEventNames[] = {
  "focus",
  "blur",
};

NPClass ChromeFrameNPAPI::plugin_class_ = {
  NP_CLASS_STRUCT_VERSION,
  ChromeFrameNPAPI::AllocateObject,
  ChromeFrameNPAPI::DeallocateObject,
  ChromeFrameNPAPI::Invalidate,
  ChromeFrameNPAPI::HasMethod,
  ChromeFrameNPAPI::Invoke,
  NULL,  // invokeDefault
  ChromeFrameNPAPI::HasProperty,
  ChromeFrameNPAPI::GetProperty,
  ChromeFrameNPAPI::SetProperty,
  NULL,  // remove property
  NULL,  // enumeration
  NULL,  // construct
};

NPIdentifier
    ChromeFrameNPAPI::plugin_property_identifiers_[PLUGIN_PROPERTY_COUNT]
        = {0};

const NPUTF8* ChromeFrameNPAPI::plugin_property_identifier_names_[] = {
  "version",
  "src",
  "onload",
  "onloaderror",
  "onmessage",
  "readystate",
  "onprivatemessage",
  "usechromenetwork",
};

const NPUTF8* ChromeFrameNPAPI::plugin_method_identifier_names_[] = {
  "postMessage",
  "postPrivateMessage",
  "installExtension",
  "loadExtension",
  "enableExtensionAutomation",
  "getEnabledExtensions"
};

ChromeFrameNPAPI::PluginMethod ChromeFrameNPAPI::plugin_methods_[] = {
  &ChromeFrameNPAPI::postMessage,
  &ChromeFrameNPAPI::postPrivateMessage,
  &ChromeFrameNPAPI::installExtension,
  &ChromeFrameNPAPI::loadExtension,
  &ChromeFrameNPAPI::enableExtensionAutomation,
  &ChromeFrameNPAPI::getEnabledExtensions,
};

NPIdentifier
    ChromeFrameNPAPI::plugin_method_identifiers_[arraysize(plugin_methods_)]
        = {0};


void ChromeFrameNPAPI::CompileAsserts() {
  NOTREACHED();  // This function should never be invoked.

  COMPILE_ASSERT(arraysize(plugin_method_identifier_names_) ==
                 arraysize(plugin_methods_),
                 you_must_add_both_plugin_method_and_name);

  COMPILE_ASSERT(arraysize(plugin_property_identifier_names_) ==
                 arraysize(plugin_property_identifiers_),
                 you_must_add_both_plugin_property_and_name);
}

static const char kPluginSrcAttribute[] = "src";
static const char kPluginForceFullPageAttribute[] = "force_full_page";
static const char kPluginOnloadAttribute[] = "onload";
static const char kPluginOnErrorAttribute[] = "onloaderror";
static const char kPluginOnMessageAttribute[] = "onmessage";
static const char kPluginOnPrivateMessageAttribute[] = "onprivatemessage";
// These properties can only be set in arguments at control instantiation.
// When the privileged_mode property is provided and set to true, the control
// will probe for whether its hosting document has the system principal, in
// which case privileged mode will be enabled.
static const char kPluginPrivilegedModeAttribute[] = "privileged_mode";
// If privileged mode is enabled, the string value of this argument will
// be appended to the chrome.exe command line.
static const char kPluginChromeExtraArguments[] = "chrome_extra_arguments";
// If privileged mode is enabled, the string value of this argument will
// be used as the profile name for our chrome.exe instance.
static const char kPluginChromeProfileName[] = "chrome_profile_name";
// If privileged mode is enabled, this argument will be taken as a
// comma-separated list of API function calls to automate.
static const char kPluginChromeFunctionsAutomatedAttribute[] =
    "chrome_functions_automated";
// If chrome network stack is to be used
static const char kPluginUseChromeNetwork[] = "usechromenetwork";

// ChromeFrameNPAPI member defines.

// TODO(tommi): remove ignore_setfocus_ since that's not how focus is
//  handled anymore.

ChromeFrameNPAPI::ChromeFrameNPAPI()
    : instance_(NULL),
    mode_(NP_EMBED),
    force_full_page_plugin_(false),
    ready_state_(READYSTATE_LOADING),
    enabled_popups_(false),
    navigate_after_initialization_(false) {
}

ChromeFrameNPAPI::~ChromeFrameNPAPI() {
  if (IsWindow()) {
    if (!UnsubclassWindow()) {
      // TODO(tommi): Figure out why this can sometimes happen in the
      // WidgetModeFF_Resize unittest.
      DLOG(ERROR) << "Couldn't unsubclass safely!";
      UnsubclassWindow(TRUE);
    }
  }
  m_hWnd = NULL;

  instance_count_--;
  if (instance_count_ <= 0) {
    delete message_loop_;
    message_loop_ = NULL;
  }

  Uninitialize();
}

std::string ChromeFrameNPAPI::GetLocation() {
  // Note that GetWindowObject() will cache the window object here.
  return np_utils::GetLocation(instance_, GetWindowObject());
}

bool ChromeFrameNPAPI::Initialize(NPMIMEType mime_type, NPP instance,
                                  uint16 mode, int16 argc, char* argn[],
                                  char* argv[]) {
  if (!Base::Initialize())
    return false;

  instance_ = instance;
  mime_type_ = mime_type;
  mode_ = mode;
  document_url_ = GetLocation();

  if (instance_count_ == 0) {
    DCHECK(message_loop_ == NULL);
    message_loop_ = new MessageLoop();
  }

  instance_count_++;

  // Create our prefs service wrapper here.
  DCHECK(!pref_service_.get());
  pref_service_ = CreatePrefService();
  if (!pref_service_.get()) {
    NOTREACHED() << "new NpProxyService";
    return false;
  }

  // Temporary variables for privileged only parameters
  const char* onprivatemessage_arg = NULL;
  const char* chrome_extra_arguments_arg = NULL;
  const char* chrome_profile_name_arg = NULL;
  bool chrome_network_arg_set = false;
  bool chrome_network_arg = false;
  bool wants_privileged = false;

  for (int i = 0; i < argc; ++i) {
    if (LowerCaseEqualsASCII(argn[i], kPluginSrcAttribute)) {
      src_ = ResolveURL(GetDocumentUrl(), argv[i]);
    } else if (LowerCaseEqualsASCII(argn[i], kPluginForceFullPageAttribute)) {
      force_full_page_plugin_ = atoi(argv[i]) ? true : false;
    } else if (LowerCaseEqualsASCII(argn[i], kPluginOnErrorAttribute)) {
      onerror_handler_ = JavascriptToNPObject(argv[i]);
    } else if (LowerCaseEqualsASCII(argn[i], kPluginOnMessageAttribute)) {
      onmessage_handler_ = JavascriptToNPObject(argv[i]);
    } else if (LowerCaseEqualsASCII(argn[i],
                                   kPluginPrivilegedModeAttribute)) {
      // Test for the FireFox privileged mode if the user requests it
      // in initialization parameters.
      wants_privileged = atoi(argv[i]) ? true : false;
    } else if (LowerCaseEqualsASCII(argn[i],
                                    kPluginOnPrivateMessageAttribute)) {
      onprivatemessage_arg = argv[i];
    } else if (LowerCaseEqualsASCII(argn[i], kPluginChromeExtraArguments)) {
      chrome_extra_arguments_arg = argv[i];
    } else if (LowerCaseEqualsASCII(argn[i], kPluginChromeProfileName)) {
      chrome_profile_name_arg = argv[i];
    } else if (LowerCaseEqualsASCII(argn[i],
                                    kPluginChromeFunctionsAutomatedAttribute)) {
      functions_enabled_.clear();
      // SplitString writes one empty entry for blank strings, so we need this
      // to allow specifying zero automation of API functions.
      if (argv[i][0] != '\0')
        SplitString(argv[i], ',', &functions_enabled_);
    } else if (LowerCaseEqualsASCII(argn[i], kPluginUseChromeNetwork)) {
      chrome_network_arg_set = true;
      chrome_network_arg = atoi(argv[i]) ? true : false;
    }
  }

  // Is the privileged mode requested?
  if (wants_privileged) {
    is_privileged_ = IsFireFoxPrivilegedInvocation(instance);
    if (!is_privileged_) {
      DLOG(WARNING) << "Privileged mode requested in non-privileged context";
    }
  }

  std::wstring extra_arguments;
  std::wstring profile_name(GetHostProcessName(false));
  if (is_privileged_) {
    // Process any privileged mode-only arguments we were handed.
    if (onprivatemessage_arg)
      onprivatemessage_handler_ = JavascriptToNPObject(onprivatemessage_arg);

    if (chrome_extra_arguments_arg)
      extra_arguments = UTF8ToWide(chrome_extra_arguments_arg);

    if (chrome_profile_name_arg)
      profile_name = UTF8ToWide(chrome_profile_name_arg);

    if (chrome_network_arg_set)
      automation_client_->set_use_chrome_network(chrome_network_arg);
  }

  static const wchar_t kHandleTopLevelRequests[] = L"HandleTopLevelRequests";
  bool top_level_requests = GetConfigBool(true, kHandleTopLevelRequests);
  automation_client_->set_handle_top_level_requests(top_level_requests);
  automation_client_->set_route_all_top_level_navigations(true);

  // Setup Url fetcher.
  url_fetcher_.set_NPPInstance(instance_);
  url_fetcher_.set_frame_busting(!is_privileged_);
  automation_client_->SetUrlFetcher(&url_fetcher_);

  // TODO(joshia): Initialize navigation here and send proxy config as
  // part of LaunchSettings
  /*
  if (!src_.empty())
    automation_client_->InitiateNavigation(src_, is_privileged_);

  std::string proxy_settings;
  bool has_prefs = pref_service_->Initialize(instance_,
                                             automation_client_.get());
  if (has_prefs && pref_service_->GetProxyValueJSONString(&proxy_settings)) {
    automation_client_->SetProxySettings(proxy_settings);
  }
  */

  // We can't call SubscribeToFocusEvents here since
  // when Initialize gets called, Opera is in a state where
  // it can't handle calls back and the thread will hang.
  // Instead, we call SubscribeToFocusEvents when we initialize
  // our plugin window.

  // TODO(stoyan): Ask host for specific interface whether to honor
  // host's in-private mode.
  return InitializeAutomation(profile_name, extra_arguments,
                              GetBrowserIncognitoMode(), true,
                              GURL(src_), GURL(), true);
}

void ChromeFrameNPAPI::Uninitialize() {
  // Don't call SetReadyState as it will end up calling FireEvent.
  // We are in the context of NPP_DESTROY.
  ready_state_ = READYSTATE_UNINITIALIZED;

  UnsubscribeFromFocusEvents();

  if (pref_service_) {
    pref_service_->UnInitialize();
    pref_service_ = NULL;
  }

  window_object_.Free();
  onerror_handler_.Free();
  onmessage_handler_.Free();
  onprivatemessage_handler_.Free();

  Base::Uninitialize();
}

void ChromeFrameNPAPI::OnFinalMessage(HWND window) {
  // The automation server should be gone by now.
  Uninitialize();
}

void ChromeFrameNPAPI::SubscribeToFocusEvents() {
  DCHECK(focus_listener_.get() == NULL);

  focus_listener_ = new DomEventListener(this);
  if (!focus_listener_->Subscribe(instance_, kNpEventNames,
                                  arraysize(kNpEventNames))) {
    focus_listener_ = NULL;
    focus_listener_ = new NPObjectEventListener(this);
    if (!focus_listener_->Subscribe(instance_, kNpEventNames,
                                    arraysize(kNpEventNames))) {
      DLOG(ERROR) << "Failed to subscribe to focus events";
      focus_listener_ = NULL;
    }
  }
}

void ChromeFrameNPAPI::UnsubscribeFromFocusEvents() {
  if (!focus_listener_.get())
    return;

  bool ret = focus_listener_->Unsubscribe(instance_, kNpEventNames,
                                          arraysize(kNpEventNames));
  DLOG_IF(WARNING, !ret) << "focus_listener_->Unsubscribe failed";
  focus_listener_ = NULL;
}

bool ChromeFrameNPAPI::SetWindow(NPWindow* window_info) {
  if (!window_info || !automation_client_.get()) {
    NOTREACHED();
    return false;
  }

  HWND window = reinterpret_cast<HWND>(window_info->window);
  if (!::IsWindow(window)) {
    // No window created yet. Ignore this call.
    return false;
  }

  if (IsWindow()) {
    // We've already subclassed, make sure that SetWindow doesn't get called
    // with an HWND other than the one we subclassed during our lifetime.
    DCHECK(window == m_hWnd);
    return true;
  }

  automation_client_->SetParentWindow(window);

  SubscribeToFocusEvents();

  if (force_full_page_plugin_) {
    // By default full page mode is only enabled when the plugin is loaded off
    // a separate file, i.e. it is the primary content in the window. Even if
    // we specify the width/height attributes for the plugin as 100% each, FF
    // instantiates the plugin passing in a width/height of 100px each. To
    // workaround this we resize the plugin window passed in by FF to the size
    // of its parent.
    HWND plugin_parent_window = ::GetParent(window);
    RECT plugin_parent_rect = {0};
    ::GetClientRect(plugin_parent_window, &plugin_parent_rect);
    ::SetWindowPos(window, NULL, plugin_parent_rect.left,
                   plugin_parent_rect.top,
                   plugin_parent_rect.right - plugin_parent_rect.left,
                   plugin_parent_rect.bottom - plugin_parent_rect.top, 0);
  }

  // Subclass the browser's plugin window here.
  if (SubclassWindow(window)) {
    DWORD new_style_flags = WS_CLIPCHILDREN;
    ModifyStyle(0, new_style_flags, 0);

    if (ready_state_ < READYSTATE_INTERACTIVE) {
      SetReadyState(READYSTATE_INTERACTIVE);
    }
  }

  return true;
}

void ChromeFrameNPAPI::Print(NPPrint* print_info) {
  if (!print_info) {
    NOTREACHED();
    return;
  }

  // We dont support full tab mode yet.
  if (print_info->mode != NP_EMBED) {
    NOTREACHED();
    return;
  }

  NPWindow window = print_info->print.embedPrint.window;

  RECT print_bounds = {0};
  print_bounds.left = window.x;
  print_bounds.top = window.y;
  print_bounds.right = window.x + window.width;
  print_bounds.bottom = window.x + window.height;

  automation_client_->Print(
      reinterpret_cast<HDC>(print_info->print.embedPrint.platformPrint),
      print_bounds);
}

void ChromeFrameNPAPI::UrlNotify(const char* url, NPReason reason,
                                 void* notify_data) {
  if (enabled_popups_) {
    // We have opened the URL so tell the browser to restore popup settings
    enabled_popups_ = false;
    npapi::PopPopupsEnabledState(instance_);
  }

  url_fetcher_.UrlNotify(url, reason, notify_data);
}

void ChromeFrameNPAPI::OnAcceleratorPressed(int tab_handle,
                                            const MSG& accel_message) {
  DLOG(INFO) << __FUNCTION__ << " msg:"
      << StringPrintf("0x%04X", accel_message.message) << " key:"
      << accel_message.wParam;

  // The host browser does call TranslateMessage on messages like WM_KEYDOWN
  // WM_KEYUP, etc, which will result in messages like WM_CHAR, WM_SYSCHAR, etc
  // being posted to the message queue. We don't post these messages here to
  // avoid these messages from getting handled twice.
  if (!is_privileged_ &&
      accel_message.message != WM_CHAR &&
      accel_message.message != WM_DEADCHAR &&
      accel_message.message != WM_SYSCHAR &&
      accel_message.message != WM_SYSDEADCHAR) {
    // A very primitive way to handle keystrokes.
    // TODO(tommi): When we've implemented a way for chrome to
    //  know when keystrokes are handled (deterministically) on that side,
    //  then this function should get called and not otherwise.
    ::PostMessage(::GetParent(m_hWnd), accel_message.message,
                  accel_message.wParam, accel_message.lParam);
  }

  if (automation_client_.get()) {
    TabProxy* tab = automation_client_->tab();
    if (tab) {
      tab->ProcessUnhandledAccelerator(accel_message);
    }
  }
}

void ChromeFrameNPAPI::OnTabbedOut(int tab_handle, bool reverse) {
  DLOG(INFO) << __FUNCTION__;

  ignore_setfocus_ = true;
  HWND parent = ::GetParent(m_hWnd);
  ::SetFocus(parent);

  INPUT input = {0};
  input.type = INPUT_KEYBOARD;
  input.ki.wVk = VK_TAB;
  SendInput(1, &input, sizeof(input));
  input.ki.dwFlags = KEYEVENTF_KEYUP;
  SendInput(1, &input, sizeof(input));

  ignore_setfocus_ = false;
}

void ChromeFrameNPAPI::OnOpenURL(int tab_handle,
                                 const GURL& url,
                                 const GURL& referrer,
                                 int open_disposition) {
  std::string target;
  switch (open_disposition) {
    case NEW_FOREGROUND_TAB:
      target = "_blank";
      break;
    case NEW_BACKGROUND_TAB:
      target = "_blank";
      break;
    case NEW_WINDOW:
    case NEW_POPUP:
      target = "_new";
      break;
    default:
      break;
  }

  // Tell the browser to temporarily allow popups
  enabled_popups_ = true;
  npapi::PushPopupsEnabledState(instance_, TRUE);
  npapi::GetURLNotify(instance_, url.spec().c_str(), target.c_str(), NULL);
}

bool ChromeFrameNPAPI::HasMethod(NPObject* obj, NPIdentifier name) {
  for (int i = 0; i < arraysize(plugin_methods_); ++i) {
    if (name == plugin_method_identifiers_[i])
      return true;
  }

  return false;
}

bool ChromeFrameNPAPI::Invoke(NPObject* header, NPIdentifier name,
                              const NPVariant* args, uint32_t arg_count,
                              NPVariant* result) {
  ChromeFrameNPAPI* plugin_instance = ChromeFrameInstanceFromNPObject(header);
  if (!plugin_instance && (plugin_instance->automation_client_.get()))
    return false;

  bool success = false;
  for (int i = 0; i < arraysize(plugin_methods_); ++i) {
    if (name == plugin_method_identifiers_[i]) {
      PluginMethod method = plugin_methods_[i];
      success = (plugin_instance->*method)(header, args, arg_count, result);
      break;
    }
  }

  return success;
}

void ChromeFrameNPAPI::InitializeIdentifiers() {
  npapi::GetStringIdentifiers(plugin_method_identifier_names_,
                              arraysize(plugin_methods_),
                              plugin_method_identifiers_);

  npapi::GetStringIdentifiers(plugin_property_identifier_names_,
                              PLUGIN_PROPERTY_COUNT,
                              plugin_property_identifiers_);
}

NPObject* ChromeFrameNPAPI::AllocateObject(NPP instance, NPClass* class_name) {
  static bool identifiers_initialized = false;

  ChromeFrameNPObject* plugin_object = new ChromeFrameNPObject();
  DCHECK(plugin_object != NULL);

  plugin_object->chrome_frame_plugin_instance = new ChromeFrameNPAPI();
  DCHECK(plugin_object->chrome_frame_plugin_instance != NULL);

  plugin_object->npp = NULL;

  COMPILE_ASSERT(arraysize(plugin_method_identifiers_) ==
                 arraysize(plugin_method_identifier_names_),
                 method_count_mismatch);

  COMPILE_ASSERT(arraysize(plugin_method_identifiers_) ==
                 arraysize(plugin_methods_),
                 method_count_mismatch);

  if (!identifiers_initialized) {
    InitializeIdentifiers();
    identifiers_initialized  = true;
  }

  return reinterpret_cast<NPObject*>(plugin_object);
}

void ChromeFrameNPAPI::DeallocateObject(NPObject* header) {
  ChromeFrameNPObject* plugin_object =
      reinterpret_cast<ChromeFrameNPObject*>(header);
  DCHECK(plugin_object != NULL);

  if (plugin_object) {
    delete plugin_object->chrome_frame_plugin_instance;
    delete plugin_object;
  }
}

void ChromeFrameNPAPI::Invalidate(NPObject* header) {
  DCHECK(header);
  ChromeFrameNPObject* plugin_object =
      reinterpret_cast<ChromeFrameNPObject*>(header);
  if (plugin_object) {
    DCHECK(plugin_object->chrome_frame_plugin_instance);
    plugin_object->chrome_frame_plugin_instance->Uninitialize();
  }
}

ChromeFrameNPAPI* ChromeFrameNPAPI::ChromeFrameInstanceFromPluginInstance(
    NPP instance) {
  if ((instance == NULL) || (instance->pdata == NULL)) {
    NOTREACHED();
    return NULL;
  }

  return ChromeFrameInstanceFromNPObject(instance->pdata);
}

ChromeFrameNPAPI* ChromeFrameNPAPI::ChromeFrameInstanceFromNPObject(
    void* object) {
  ChromeFrameNPObject* plugin_object =
      reinterpret_cast<ChromeFrameNPObject*>(object);
  if (!plugin_object) {
    NOTREACHED();
    return NULL;
  }

  DCHECK(plugin_object->chrome_frame_plugin_instance);
  return plugin_object->chrome_frame_plugin_instance;
}

bool ChromeFrameNPAPI::HasProperty(NPObject* obj, NPIdentifier name) {
  for (int i = 0; i < PLUGIN_PROPERTY_COUNT; ++i) {
    if (name == plugin_property_identifiers_[i])
      return true;
  }
  return false;
}

bool ChromeFrameNPAPI::GetProperty(NPIdentifier name,
                                   NPVariant* variant) {
  if (name == plugin_property_identifiers_[PLUGIN_PROPERTY_ONERROR]) {
    if (onerror_handler_) {
      variant->type = NPVariantType_Object;
      variant->value.objectValue = onerror_handler_.Copy();
      return true;
    }
  } else if (name == plugin_property_identifiers_[PLUGIN_PROPERTY_ONMESSAGE]) {
    if (onmessage_handler_) {
      variant->type = NPVariantType_Object;
      variant->value.objectValue = onmessage_handler_.Copy();
      return true;
    }
  } else if (name ==
             plugin_property_identifiers_[PLUGIN_PROPERTY_ONPRIVATEMESSAGE]) {
    if (!is_privileged_) {
      DLOG(WARNING) << "Attempt to read onprivatemessage property while not "
                       "privileged";
    } else {
      if (onprivatemessage_handler_) {
        variant->type = NPVariantType_Object;
        variant->value.objectValue =
            onprivatemessage_handler_.Copy();
        return true;
      }
    }
  } else if (name == plugin_property_identifiers_[PLUGIN_PROPERTY_SRC]) {
    AllocateStringVariant(src_, variant);
    return true;
  } else if (name == plugin_property_identifiers_[PLUGIN_PROPERTY_VERSION]) {
    const std::wstring version =
        automation_client_->GetVersion();
    AllocateStringVariant(WideToUTF8(version), variant);
    return true;
  } else if (name == plugin_property_identifiers_[PLUGIN_PROPERTY_READYSTATE]) {
    INT32_TO_NPVARIANT(ready_state_, *variant);
    return true;
  } else if (name ==
        plugin_property_identifiers_[PLUGIN_PROPERTY_USECHROMENETWORK]) {
    BOOLEAN_TO_NPVARIANT(automation_client_->use_chrome_network(), *variant);
    return true;
  }

  return false;
}

bool ChromeFrameNPAPI::GetProperty(NPObject* object, NPIdentifier name,
                                   NPVariant* variant) {
  if (!object || !variant) {
    NOTREACHED();
    return false;
  }

  ChromeFrameNPAPI* plugin_instance = ChromeFrameInstanceFromNPObject(object);
  if (!plugin_instance) {
    NOTREACHED();
    return false;
  }

  return plugin_instance->GetProperty(name, variant);
}

bool ChromeFrameNPAPI::SetProperty(NPIdentifier name,
                                   const NPVariant* variant) {
  if (NPVARIANT_IS_OBJECT(*variant)) {
    if (name == plugin_property_identifiers_[PLUGIN_PROPERTY_ONERROR]) {
      onerror_handler_.Free();
      onerror_handler_ = variant->value.objectValue;
      return true;
    } else if (
        name == plugin_property_identifiers_[PLUGIN_PROPERTY_ONMESSAGE]) {
      onmessage_handler_.Free();
      onmessage_handler_ = variant->value.objectValue;
      return true;
    } else if (name ==
              plugin_property_identifiers_[PLUGIN_PROPERTY_ONPRIVATEMESSAGE]) {
      if (!is_privileged_) {
        DLOG(WARNING) << "Attempt to set onprivatemessage while not privileged";
      } else {
        onprivatemessage_handler_.Free();
        onprivatemessage_handler_ = variant->value.objectValue;
        return true;
      }
    }
  } else if (NPVARIANT_IS_STRING(*variant) || NPVARIANT_IS_NULL(*variant)) {
    if (name == plugin_property_identifiers_[PLUGIN_PROPERTY_SRC]) {
      return NavigateToURL(variant, 1, NULL);
    }
  } else if (NPVARIANT_IS_BOOLEAN(*variant)) {
    if (name ==
        plugin_property_identifiers_[PLUGIN_PROPERTY_USECHROMENETWORK]) {
      automation_client_->set_use_chrome_network(
          NPVARIANT_TO_BOOLEAN(*variant));
    }
  }

  return false;
}

bool ChromeFrameNPAPI::SetProperty(NPObject* object, NPIdentifier name,
                                   const NPVariant* variant) {
  if (!object || !variant) {
    DLOG(ERROR) << "Cannot set property: " << npapi::StringFromIdentifier(name);
    return false;
  }

  ChromeFrameNPAPI* plugin_instance = ChromeFrameInstanceFromNPObject(object);
  if (!plugin_instance) {
    NOTREACHED();
    return false;
  }

  return plugin_instance->SetProperty(name, variant);
}

void ChromeFrameNPAPI::OnFocus() {
  DLOG(INFO) << __FUNCTION__;
  PostMessage(WM_SETFOCUS, 0, 0);
}

void ChromeFrameNPAPI::OnEvent(const char* event_name) {
  DCHECK(event_name);
  DLOG(INFO) << event_name;

  if (lstrcmpiA(event_name, "focus") == 0) {
    OnFocus();
  } else if (lstrcmpiA(event_name, "blur") == 0) {
    OnBlur();
  } else {
    NOTREACHED() << event_name;
  }
}

LRESULT CALLBACK ChromeFrameNPAPI::DropKillFocusHook(int code, WPARAM wparam,
                                                     LPARAM lparam) {
  LRESULT ret = 0;
  CWPSTRUCT* wp = reinterpret_cast<CWPSTRUCT*>(lparam);
  if ((code < 0) || (wp->message != WM_KILLFOCUS))
    ret = ::CallNextHookEx(NULL, code, wparam, lparam);

  return ret;
}

LRESULT ChromeFrameNPAPI::OnSetFocus(UINT message, WPARAM wparam,
                                     LPARAM lparam, BOOL& handled) {  // NO_LINT
  // Opera has a WH_CALLWNDPROC hook that handles WM_KILLFOCUS and
  // prevents us from setting the focus to the tab.
  // To work around that, we set a temporary hook here that does nothing
  // (not even call other hooks) when it sees WM_KILLFOCUS.
  HHOOK hook = NULL;
  hook = ::SetWindowsHookEx(WH_CALLWNDPROC, DropKillFocusHook, NULL,
                            ::GetCurrentThreadId());
  // Since we chain message maps, make sure we are not calling base class
  // twice for WM_SETFOCUS.
  BOOL handled_by_base = TRUE;
  LRESULT ret = Base::OnSetFocus(message, wparam, lparam, handled_by_base);
  if (hook)
    ::UnhookWindowsHookEx(hook);

  return ret;
}

void ChromeFrameNPAPI::OnBlur() {
  DLOG(INFO) << __FUNCTION__;
}

void ChromeFrameNPAPI::OnLoad(int, const GURL& gurl) {
  DLOG(INFO) << "Firing onload";
  FireEvent("load", gurl.spec());
}

void ChromeFrameNPAPI::OnLoadFailed(int error_code, const std::string& url) {
  FireEvent("loaderror", url);

  ScopedNpVariant result;
  InvokeDefault(onerror_handler_, url, &result);
}

void ChromeFrameNPAPI::OnMessageFromChromeFrame(int tab_handle,
                                                const std::string& message,
                                                const std::string& origin,
                                                const std::string& target) {
  bool private_message = false;
  if (target.compare("*") != 0) {
    if (is_privileged_) {
      private_message = true;
    } else {
      if (!HaveSameOrigin(target, document_url_)) {
        DLOG(WARNING) << "Dropping posted message since target doesn't match "
            "the current document's origin. target=" << target;
        return;
      }
    }
  }

  // Create a MessageEvent object that contains the message and origin
  // as well as supporting other MessageEvent (see the HTML5 spec) properties.
  // Then call the onmessage handler.
  ScopedNpObject<NPObject> event;
  bool ok = CreateMessageEvent(false, true, message, origin, event.Receive());
  if (ok) {
    // Don't call FireEvent here (or we'll have an event wrapped by an event).
    DispatchEvent(event);

    ScopedNpVariant result;
    NPVariant params[2];
    OBJECT_TO_NPVARIANT(event, params[0]);
    bool invoke = false;
    if (private_message) {
      DCHECK(is_privileged_);
      STRINGN_TO_NPVARIANT(target.c_str(), target.length(), params[1]);
      invoke = InvokeDefault(onprivatemessage_handler_,
                             arraysize(params),
                             params,
                             &result);
    } else {
      invoke = InvokeDefault(onmessage_handler_, params[0], &result);
    }
    DLOG_IF(WARNING, !invoke) << "InvokeDefault failed";
  } else {
    NOTREACHED() << "CreateMessageEvent";
  }
}

void ChromeFrameNPAPI::OnAutomationServerReady() {
  Base::OnAutomationServerReady();

  std::string proxy_settings;
  bool has_prefs = pref_service_->Initialize(instance_,
                                             automation_client_.get());
  if (has_prefs && pref_service_->GetProxyValueJSONString(&proxy_settings)) {
    automation_client_->SetProxySettings(proxy_settings);
  }

  if (navigate_after_initialization_ && !src_.empty()) {
    navigate_after_initialization_ = false;
    if (!automation_client_->InitiateNavigation(src_,
                                                GetDocumentUrl(),
                                                is_privileged_)) {
      DLOG(ERROR) << "Failed to navigate to: " << src_;
      src_.clear();
    }
  }

  SetReadyState(READYSTATE_COMPLETE);
}

void ChromeFrameNPAPI::OnAutomationServerLaunchFailed(
    AutomationLaunchResult reason, const std::string& server_version) {
  SetReadyState(READYSTATE_UNINITIALIZED);

  if (reason == AUTOMATION_VERSION_MISMATCH) {
    DisplayVersionMismatchWarning(m_hWnd, server_version);
  }
}

bool ChromeFrameNPAPI::InvokeDefault(NPObject* object,
                                     unsigned param_count,
                                     const NPVariant* params,
                                     NPVariant* result) {
  if (!object)
    return false;

  bool ret = npapi::InvokeDefault(instance_, object, params, param_count,
                                  result);
  // InvokeDefault can return false in FF even though we do see the call
  // go through.  It's not clear to me what the circumstances are, so
  // we log it as a warning while tracking it down.
  DLOG_IF(WARNING, !ret) << "npapi::InvokeDefault failed";
  return ret;
}

bool ChromeFrameNPAPI::InvokeDefault(NPObject* object, const std::string& param,
                                     NPVariant* result) {
  NPVariant arg;
  STRINGN_TO_NPVARIANT(param.c_str(), param.length(), arg);
  return InvokeDefault(object, arg, result);
}

bool ChromeFrameNPAPI::InvokeDefault(NPObject* object, const NPVariant& param,
                                     NPVariant* result) {
  return InvokeDefault(object, 1, &param, result);
}

bool ChromeFrameNPAPI::CreateEvent(const std::string& type, bool bubbles,
                                   bool cancelable, NPObject** basic_event) {
  DCHECK(basic_event);
  NPObject* window = GetWindowObject();
  if (!window) {
    // Can fail if the browser is closing (seen in Opera).
    return false;
  }

  const char* identifier_names[] = {
    "document",
    "createEvent",
    "initEvent",
  };

  NPIdentifier identifiers[arraysize(identifier_names)];
  npapi::GetStringIdentifiers(identifier_names, arraysize(identifier_names),
                              identifiers);

  // Fetch the document object from the window.
  ScopedNpVariant document;
  bool ok = npapi::GetProperty(instance_, window, identifiers[0], &document);
  if (!ok) {
    // This could happen if the page is being unloaded.
    DLOG(WARNING) << "Failed to fetch the document object";
    return false;
  }

  bool success = false;
  if (ok && NPVARIANT_IS_OBJECT(document)) {
    // Call document.createEvent("Event") to create a basic event object.
    NPVariant event_type;
    STRINGN_TO_NPVARIANT("Event", sizeof("Event") - 1, event_type);
    ScopedNpVariant result;
    success = npapi::Invoke(instance_, NPVARIANT_TO_OBJECT(document),
                            identifiers[1], &event_type, 1, &result);
    if (!NPVARIANT_IS_OBJECT(result)) {
      DLOG(WARNING) << "Failed to invoke createEvent";
      success = false;
    } else {
      NPVariant init_args[3];
      STRINGN_TO_NPVARIANT(type.c_str(), type.length(), init_args[0]);
      BOOLEAN_TO_NPVARIANT(bubbles, init_args[1]);
      BOOLEAN_TO_NPVARIANT(cancelable, init_args[2]);

      // Now initialize the event object by calling
      // event.initEvent(type, bubbles, cancelable);
      ScopedNpVariant init_results;
      ok = npapi::Invoke(instance_, NPVARIANT_TO_OBJECT(result), identifiers[2],
                         init_args, arraysize(init_args), &init_results);
      if (ok) {
        success = true;
        // Finally, pass the ownership to the caller.
        *basic_event = NPVARIANT_TO_OBJECT(result);
        VOID_TO_NPVARIANT(result);  // Prevent the object from being released.
      } else {
        DLOG(ERROR) << "initEvent failed";
        success = false;
      }
    }
  }

  return success;
}

bool ChromeFrameNPAPI::CreateMessageEvent(bool bubbles, bool cancelable,
                                          const std::string& data,
                                          const std::string& origin,
                                          NPObject** message_event) {
  DCHECK(message_event);
  ScopedNpObject<NPObject> event;
  bool ok = CreateEvent("message", false, true, event.Receive());
  if (ok) {
    typedef enum {
      DATA,
      ORIGIN,
      LAST_EVENT_ID,
      SOURCE,
      MESSAGE_PORT,
      IDENTIFIER_COUNT,  // Must be last.
    } StringIdentifiers;

    static NPIdentifier identifiers[IDENTIFIER_COUNT] = {0};
    if (!identifiers[0]) {
      const NPUTF8* identifier_names[] = {
        "data",
        "origin",
        "lastEventId",
        "source",
        "messagePort",
      };
      COMPILE_ASSERT(arraysize(identifier_names) == arraysize(identifiers),
                     mismatched_array_size);
      npapi::GetStringIdentifiers(identifier_names, IDENTIFIER_COUNT,
                                  identifiers);
    }

    NPVariant arg;
    STRINGN_TO_NPVARIANT(data.c_str(), data.length(), arg);
    npapi::SetProperty(instance_, event, identifiers[DATA], &arg);
    STRINGN_TO_NPVARIANT(origin.c_str(), origin.length(), arg);
    npapi::SetProperty(instance_, event, identifiers[ORIGIN], &arg);
    STRINGN_TO_NPVARIANT("", 0, arg);
    npapi::SetProperty(instance_, event, identifiers[LAST_EVENT_ID], &arg);
    NULL_TO_NPVARIANT(arg);
    npapi::SetProperty(instance_, event, identifiers[SOURCE], &arg);
    npapi::SetProperty(instance_, event, identifiers[MESSAGE_PORT], &arg);
    *message_event = event.Detach();
  }

  return ok;
}


void ChromeFrameNPAPI::DispatchEvent(NPObject* event) {
  DCHECK(event != NULL);

  ScopedNpObject<NPObject> embed;
  npapi::GetValue(instance_, NPNVPluginElementNPObject, &embed);
  if (embed != NULL) {
    NPVariant param;
    OBJECT_TO_NPVARIANT(event, param);
    ScopedNpVariant result;
    bool invoke = npapi::Invoke(instance_, embed,
        npapi::GetStringIdentifier("dispatchEvent"), &param, 1, &result);
    DLOG_IF(WARNING, !invoke) << "dispatchEvent failed";
  } else {
    NOTREACHED() << "NPNVPluginElementNPObject";
  }
}

bool ChromeFrameNPAPI::ExecuteScript(const std::string& script,
                                     NPVariant* result) {
  NPObject* window = GetWindowObject();
  if (!window) {
    NOTREACHED();
    return false;
  }

  NPString script_for_execution;
  script_for_execution.UTF8Characters = script.c_str();
  script_for_execution.UTF8Length = script.length();

  return npapi::Evaluate(instance_, window, &script_for_execution, result);
}

NPObject* ChromeFrameNPAPI::JavascriptToNPObject(const std::string& script) {
  // Convert the passed in script to an invocable NPObject
  // To achieve this we save away the function in a dummy window property
  // which is then read to get the script object representing the function.

  std::string script_code =
      "javascript:window.__cf_get_function_object =";

  // If we are able to look up the name in the javascript namespace, then it
  // means that the caller passed in a function name. Convert the function
  // name to a NPObject we can invoke on.
  if (IsValidJavascriptFunction(script)) {
    script_code += script;
  } else {
    script_code += "new Function(\"";
    script_code += script;
    script_code += "\");";
  }

  NPVariant result;
  if (!ExecuteScript(script_code, &result)) {
    NOTREACHED();
    return NULL;
  }

  DCHECK(result.type == NPVariantType_Object);
  DCHECK(result.value.objectValue != NULL);
  return result.value.objectValue;
}

bool ChromeFrameNPAPI::IsValidJavascriptFunction(const std::string& script) {
  std::string script_code = "javascript:window['";
  script_code += script;
  script_code += "'];";

  ScopedNpVariant result;
  if (!ExecuteScript(script_code, &result)) {
    NOTREACHED();
    return NULL;
  }

  return result.type == NPVariantType_Object;
}

bool ChromeFrameNPAPI::NavigateToURL(const NPVariant* args, uint32_t arg_count,
                                     NPVariant* result) {
  // Note that 'result' might be NULL.
  if (arg_count != 1 || !(NPVARIANT_IS_STRING(args[0]) ||
                          NPVARIANT_IS_NULL(args[0]))) {
    NOTREACHED();
    return false;
  }

  if (ready_state_ == READYSTATE_UNINITIALIZED) {
    // Error(L"Chrome Frame failed to initialize.");
    // TODO(tommi): call NPN_SetException
    DLOG(WARNING) << "NavigateToURL called after failed initialization";
    return false;
  }

  std::string url("about:blank");
  if (!NPVARIANT_IS_NULL(args[0])) {
    const NPString& str = args[0].value.stringValue;
    if (str.UTF8Length) {
      url.assign(std::string(str.UTF8Characters, str.UTF8Length));
    }
  }

  GURL document_url(GetDocumentUrl());
  if (document_url.SchemeIsSecure()) {
    GURL source_url(url);
    if (!source_url.SchemeIsSecure()) {
      DLOG(WARNING) << __FUNCTION__ << " Prevnting navigation to HTTP url"
          " since the containing document is HTTPS. URL: " << source_url <<
          " Document URL: " << document_url;
      return false;
    }
  }

  std::string full_url = ResolveURL(GetDocumentUrl(), url);

  src_ = full_url;
  // Navigate only if we completed initialization i.e. proxy is set etc.
  if (ready_state_ == READYSTATE_COMPLETE) {
    if (!automation_client_->InitiateNavigation(full_url,
                                                GetDocumentUrl(),
                                                is_privileged_)) {
      // TODO(tommi): call NPN_SetException.
      src_.clear();
      return false;
    }
  } else {
    navigate_after_initialization_ = true;
  }
  return true;
}

bool ChromeFrameNPAPI::postMessage(NPObject* npobject, const NPVariant* args,
                                   uint32_t arg_count, NPVariant* result) {
  // TODO(tommi) See if we can factor these checks out somehow.
  if (arg_count < 1 || arg_count > 2 || !NPVARIANT_IS_STRING(args[0])) {
    NOTREACHED();
    return false;
  }

  const NPString& str = args[0].value.stringValue;
  std::string message(str.UTF8Characters, str.UTF8Length);
  std::string target;
  if (arg_count == 2 && NPVARIANT_IS_STRING(args[1])) {
    const NPString& str = args[1].value.stringValue;
    target.assign(str.UTF8Characters, str.UTF8Length);
    if (target.compare("*") != 0) {
      GURL resolved(target);
      if (!resolved.is_valid()) {
        npapi::SetException(npobject,
                            "Unable to parse the specified target URL.");
        return false;
      }
      target = resolved.spec();
    }
  } else {
    target = "*";
  }

  GURL url(GURL(document_url_).GetOrigin());
  std::string origin(url.is_empty() ? "null" : url.spec());

  automation_client_->ForwardMessageFromExternalHost(message, origin, target);

  return true;
}

bool ChromeFrameNPAPI::postPrivateMessage(NPObject* npobject,
                                          const NPVariant* args,
                                          uint32_t arg_count,
                                          NPVariant* result) {
  if (!is_privileged_) {
    DLOG(WARNING) << "postPrivateMessage invoked in non-privileged mode";
    return false;
  }

  if (arg_count != 3 || !NPVARIANT_IS_STRING(args[0]) ||
      !NPVARIANT_IS_STRING(args[1]) || !NPVARIANT_IS_STRING(args[2])) {
    NOTREACHED();
    return false;
  }

  const NPString& message_str = args[0].value.stringValue;
  const NPString& origin_str = args[1].value.stringValue;
  const NPString& target_str = args[2].value.stringValue;
  std::string message(message_str.UTF8Characters, message_str.UTF8Length);
  std::string origin(origin_str.UTF8Characters, origin_str.UTF8Length);
  std::string target(target_str.UTF8Characters, target_str.UTF8Length);

  automation_client_->ForwardMessageFromExternalHost(message, origin, target);

  return true;
}

bool ChromeFrameNPAPI::installExtension(NPObject* npobject,
                                        const NPVariant* args,
                                        uint32_t arg_count,
                                        NPVariant* result) {
  if (arg_count > 2 || !NPVARIANT_IS_STRING(args[0]) ||
      (arg_count == 2 && !NPVARIANT_IS_OBJECT(args[1]))) {
    NOTREACHED();
    return false;
  }

  if (!is_privileged_) {
    DLOG(WARNING) << "installExtension invoked in non-privileged mode";
    return false;
  }

  if (!automation_client_.get()) {
    DLOG(WARNING) << "installExtension invoked with no automaton client";
    NOTREACHED();
    return false;
  }

  const NPString& crx_path_str = args[0].value.stringValue;
  std::string crx_path_a(crx_path_str.UTF8Characters, crx_path_str.UTF8Length);
  FilePath::StringType crx_path_u(UTF8ToWide(crx_path_a));
  FilePath crx_path(crx_path_u);
  NPObject* retained_function = npapi::RetainObject(args[1].value.objectValue);

  automation_client_->InstallExtension(crx_path, retained_function);
  // The response to this command will be returned in the OnExtensionInstalled
  // delegate callback function.

  return true;
}

void ChromeFrameNPAPI::OnExtensionInstalled(
    const FilePath& path,
    void* user_data,
    AutomationMsg_ExtensionResponseValues res) {
  ScopedNpVariant result;
  NPVariant param;
  INT32_TO_NPVARIANT(res, param);
  NPObject* func = reinterpret_cast<NPObject*>(user_data);

  InvokeDefault(func, param, &result);
  npapi::ReleaseObject(func);
}

bool ChromeFrameNPAPI::loadExtension(NPObject* npobject,
                                     const NPVariant* args,
                                     uint32_t arg_count,
                                     NPVariant* result) {
  if (arg_count > 2 || !NPVARIANT_IS_STRING(args[0]) ||
      (arg_count == 2 && !NPVARIANT_IS_OBJECT(args[1]))) {
    NOTREACHED();
    return false;
  }

  if (!is_privileged_) {
    DLOG(WARNING) << "loadExtension invoked in non-privileged mode";
    return false;
  }

  if (!automation_client_.get()) {
    DLOG(WARNING) << "loadExtension invoked with no automaton client";
    NOTREACHED();
    return false;
  }

  const NPString& path_str = args[0].value.stringValue;
  std::string path_a(path_str.UTF8Characters, path_str.UTF8Length);
  FilePath::StringType path_u(UTF8ToWide(path_a));
  FilePath path(path_u);
  NPObject* retained_function = npapi::RetainObject(args[1].value.objectValue);

  automation_client_->LoadExpandedExtension(path, retained_function);
  // The response to this command will be returned in the OnExtensionInstalled
  // delegate callback function.

  return true;
}

bool ChromeFrameNPAPI::enableExtensionAutomation(NPObject* npobject,
                                                 const NPVariant* args,
                                                 uint32_t arg_count,
                                                 NPVariant* result) {
  if (arg_count > 1 || (arg_count == 1 && !NPVARIANT_IS_STRING(args[0]))) {
    NOTREACHED();
    return false;
  }

  if (!is_privileged_) {
    DLOG(WARNING) <<
        "enableExtensionAutomation invoked in non-privileged mode";
    return false;
  }

  if (!automation_client_.get()) {
    DLOG(WARNING) <<
        "enableExtensionAutomation invoked with no automaton client";
    NOTREACHED();
    return false;
  }

  if (!automation_client_->tab()) {
    DLOG(WARNING) << "enableExtensionAutomation invoked with no hosted tab";
    NOTREACHED();
    return false;
  }

  // Empty by default e.g. if no arguments passed.
  std::vector<std::string> functions;

  if (arg_count == 1) {
    const NPString& functions_str = args[0].value.stringValue;
    std::string functions_a(functions_str.UTF8Characters,
                            functions_str.UTF8Length);

    // SplitString writes one empty entry for blank strings, so we need this
    // to allow specifying zero automation of API functions.
    if (functions_a[0] != '\0')
      SplitString(functions_a, ',', &functions);
  }

  automation_client_->tab()->SetEnableExtensionAutomation(functions);
  // This function returns no result.

  return true;
}

bool ChromeFrameNPAPI::getEnabledExtensions(NPObject* npobject,
                                            const NPVariant* args,
                                            uint32_t arg_count,
                                            NPVariant* result) {
  if (arg_count > 1 || !NPVARIANT_IS_OBJECT(args[0])) {
    NOTREACHED();
    return false;
  }

  if (!is_privileged_) {
    DLOG(WARNING) << "getEnabledExtensions invoked in non-privileged mode";
    return false;
  }

  if (!automation_client_.get()) {
    DLOG(WARNING) << "getEnabledExtensions invoked with no automaton client";
    NOTREACHED();
    return false;
  }

  NPObject* retained_function = npapi::RetainObject(args[0].value.objectValue);

  automation_client_->GetEnabledExtensions(retained_function);
  // The response to this command will be returned in the
  // OnGetEnabledExtensionsCompleted delegate callback function.

  return true;
}

void ChromeFrameNPAPI::OnGetEnabledExtensionsComplete(
    void* user_data,
    const std::vector<FilePath>& extension_directories) {
  std::vector<std::wstring> extension_paths;
  for (size_t i = 0; i < extension_directories.size(); ++i) {
    extension_paths.push_back(extension_directories[i].value());
  }
  std::wstring tab_delimited = JoinString(extension_paths, L'\t');

  std::string res = WideToUTF8(tab_delimited);

  ScopedNpVariant result;
  NPVariant param;
  STRINGN_TO_NPVARIANT(res.c_str(), res.length(), param);

  NPObject* func = reinterpret_cast<NPObject*>(user_data);
  InvokeDefault(func, param, &result);
  npapi::ReleaseObject(func);
}

void ChromeFrameNPAPI::FireEvent(const std::string& event_type,
                                 const std::string& data) {
  NPVariant arg;
  STRINGN_TO_NPVARIANT(data.c_str(), data.length(), arg);
  FireEvent(event_type, arg);
}

void ChromeFrameNPAPI::FireEvent(const std::string& event_type,
                                 const NPVariant& data) {
  // Check that we're not bundling an event inside an event.
  // Right now we're only expecting simple types for the data argument.
  DCHECK(NPVARIANT_IS_OBJECT(data) == false);

  ScopedNpObject<NPObject> ev;
  CreateEvent(event_type, false, false, ev.Receive());
  if (ev) {
    // Add the 'data' member to the event.
    bool set = npapi::SetProperty(instance_, ev,
        npapi::GetStringIdentifier("data"), const_cast<NPVariant*>(&data));
    DCHECK(set);
    DispatchEvent(ev);
  }
}

NpProxyService* ChromeFrameNPAPI::CreatePrefService() {
  return new NpProxyService;
}

NPObject* ChromeFrameNPAPI::GetWindowObject() const {
  if (!window_object_.get()) {
    NPError ret = npapi::GetValue(instance_, NPNVWindowNPObject,
        window_object_.Receive());
    DLOG_IF(ERROR, ret != NPERR_NO_ERROR) << "NPNVWindowNPObject failed";
  }
  return window_object_;
}

bool ChromeFrameNPAPI::GetBrowserIncognitoMode() {
  bool incognito_mode = false;

  // Check disabled for Opera due to bug:
  // http://code.google.com/p/chromium/issues/detail?id=24287
  if (GetBrowserType() != BROWSER_OPERA) {
    // Check whether host browser is in private mode;
    NPBool private_mode = FALSE;
    NPError err = npapi::GetValue(instance_,
                                  NPNVprivateModeBool,
                                  &private_mode);
    if (err == NPERR_NO_ERROR && private_mode) {
      incognito_mode = true;
    }
  } else {
    DLOG(WARNING) << "Not checking for private mode in Opera";
  }

  return incognito_mode;
}

bool ChromeFrameNPAPI::HandleContextMenuCommand(UINT cmd,
    const IPC::ContextMenuParams& params) {
  if (cmd == IDC_ABOUT_CHROME_FRAME) {
    // TODO: implement "About Chrome Frame"
  }
  return false;
}

NPError ChromeFrameNPAPI::NewStream(NPMIMEType type, NPStream* stream,
                                    NPBool seekable, uint16* stream_type) {
  return url_fetcher_.NewStream(type, stream, seekable, stream_type);
}

int32 ChromeFrameNPAPI::WriteReady(NPStream* stream) {
  return url_fetcher_.WriteReady(stream);
}

int32 ChromeFrameNPAPI::Write(NPStream* stream, int32 offset, int32 len,
                              void* buffer) {
  return url_fetcher_.Write(stream, offset, len, buffer);
}

NPError ChromeFrameNPAPI::DestroyStream(NPStream* stream, NPReason reason) {
  return url_fetcher_.DestroyStream(stream, reason);
}
