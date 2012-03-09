// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/extension_dispatcher.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_piece.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/extension_permission_set.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/chrome_render_process_observer.h"
#include "chrome/renderer/extensions/app_bindings.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "chrome/renderer/extensions/chrome_v8_extension.h"
#include "chrome/renderer/extensions/custom_bindings_util.h"
#include "chrome/renderer/extensions/event_bindings.h"
#include "chrome/renderer/extensions/extension_groups.h"
#include "chrome/renderer/extensions/miscellaneous_bindings.h"
#include "chrome/renderer/extensions/schema_generated_bindings.h"
#include "chrome/renderer/extensions/user_script_slave.h"
#include "chrome/renderer/extensions/webstore_bindings.h"
#include "chrome/renderer/module_system.h"
#include "chrome/renderer/native_handler.h"
#include "chrome/renderer/resource_bundle_source_map.h"
#include "content/public/renderer/render_thread.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDataSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityPolicy.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScopedUserGesture.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/resource/resource_bundle.h"
#include "v8/include/v8.h"

#include "chrome/renderer/extensions/chrome_private_custom_bindings.h"
#include "chrome/renderer/extensions/context_menus_custom_bindings.h"
#include "chrome/renderer/extensions/experimental.socket_custom_bindings.h"
#include "chrome/renderer/extensions/extension_custom_bindings.h"
#include "chrome/renderer/extensions/file_browser_handler_custom_bindings.h"
#include "chrome/renderer/extensions/file_browser_private_custom_bindings.h"
#include "chrome/renderer/extensions/i18n_custom_bindings.h"
#include "chrome/renderer/extensions/page_actions_custom_bindings.h"
#include "chrome/renderer/extensions/page_capture_custom_bindings.h"
#include "chrome/renderer/extensions/tabs_custom_bindings.h"
#include "chrome/renderer/extensions/tts_custom_bindings.h"
#include "chrome/renderer/extensions/web_request_custom_bindings.h"

namespace {

static const int64 kInitialExtensionIdleHandlerDelayMs = 5*1000;
static const int64 kMaxExtensionIdleHandlerDelayMs = 5*60*1000;

ChromeV8Context::ContextType ExtensionGroupToContextType(int extension_group) {
  if (extension_group == EXTENSION_GROUP_CONTENT_SCRIPTS)
    return ChromeV8Context::CONTENT_SCRIPT;
  return ChromeV8Context::OTHER;
}

class ChromeHiddenNativeHandler : public NativeHandler {
 public:
  ChromeHiddenNativeHandler() {
    RouteFunction("GetChromeHidden",
        base::Bind(&ChromeHiddenNativeHandler::GetChromeHidden,
                   base::Unretained(this)));
  }

  v8::Handle<v8::Value> GetChromeHidden(const v8::Arguments& args) {
    return ChromeV8Context::GetOrCreateChromeHidden(v8::Context::GetCurrent());
  }
};

class PrintNativeHandler : public NativeHandler {
 public:
  PrintNativeHandler() {
    RouteFunction("Print",
        base::Bind(&PrintNativeHandler::Print,
                   base::Unretained(this)));
  }

  v8::Handle<v8::Value> Print(const v8::Arguments& args) {
    if (args.Length() < 1)
      return v8::Undefined();

    std::vector<std::string> components;
    for (int i = 0; i < args.Length(); ++i)
      components.push_back(*v8::String::Utf8Value(args[i]->ToString()));

    LOG(ERROR) << JoinString(components, ',');
    return v8::Undefined();
  }
};

class ContextInfoNativeHandler : public NativeHandler {
 public:
  explicit ContextInfoNativeHandler(ExtensionDispatcher* extension_dispatcher,
                                    bool is_bindings_allowed,
                                    WebKit::WebFrame* frame,
                                    int world_id)
      : extension_dispatcher_(extension_dispatcher),
        is_bindings_allowed_(is_bindings_allowed),
        frame_(frame),
        world_id_(world_id) {
    RouteFunction("IsBindingsAllowed",
        base::Bind(&ContextInfoNativeHandler::IsBindingsAllowed,
            base::Unretained(this)));
    RouteFunction("IsAPIAllowed",
        base::Bind(&ContextInfoNativeHandler::IsAPIAllowed,
            base::Unretained(this)));
  }

  v8::Handle<v8::Value> IsBindingsAllowed(const v8::Arguments& args) {
    return v8::Boolean::New(is_bindings_allowed_);
  }

  v8::Handle<v8::Value> IsAPIAllowed(const v8::Arguments& args) {
    std::string custom_api_name = *v8::String::AsciiValue(args[0]->ToString());
    return v8::Boolean::New(extension_dispatcher_->AllowCustomAPI(
        frame_, custom_api_name, world_id_));
  }

 private:
  ExtensionDispatcher* extension_dispatcher_;
  bool is_bindings_allowed_;
  WebKit::WebFrame* frame_;
  int world_id_;
};

}

using namespace extensions;

using WebKit::WebDataSource;
using WebKit::WebDocument;
using WebKit::WebFrame;
using WebKit::WebSecurityPolicy;
using WebKit::WebString;
using WebKit::WebScopedUserGesture;
using WebKit::WebVector;
using WebKit::WebView;
using content::RenderThread;

ExtensionDispatcher::ExtensionDispatcher()
    : is_webkit_initialized_(false),
      webrequest_adblock_(false),
      webrequest_adblock_plus_(false),
      webrequest_other_(false),
      source_map_(&ResourceBundle::GetSharedInstance()) {
  const CommandLine& command_line = *(CommandLine::ForCurrentProcess());
  is_extension_process_ =
      command_line.HasSwitch(switches::kExtensionProcess) ||
      command_line.HasSwitch(switches::kSingleProcess);

  if (is_extension_process_) {
    RenderThread::Get()->SetIdleNotificationDelayInMs(
        kInitialExtensionIdleHandlerDelayMs);
  }

  user_script_slave_.reset(new UserScriptSlave(&extensions_));
  PopulateSourceMap();
}

ExtensionDispatcher::~ExtensionDispatcher() {
}

bool ExtensionDispatcher::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExtensionDispatcher, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_MessageInvoke, OnMessageInvoke)
    IPC_MESSAGE_HANDLER(ExtensionMsg_DeliverMessage, OnDeliverMessage)
    IPC_MESSAGE_HANDLER(ExtensionMsg_SetFunctionNames, OnSetFunctionNames)
    IPC_MESSAGE_HANDLER(ExtensionMsg_Loaded, OnLoaded)
    IPC_MESSAGE_HANDLER(ExtensionMsg_Unloaded, OnUnloaded)
    IPC_MESSAGE_HANDLER(ExtensionMsg_SetScriptingWhitelist,
                        OnSetScriptingWhitelist)
    IPC_MESSAGE_HANDLER(ExtensionMsg_ActivateExtension, OnActivateExtension)
    IPC_MESSAGE_HANDLER(ExtensionMsg_ActivateApplication, OnActivateApplication)
    IPC_MESSAGE_HANDLER(ExtensionMsg_UpdatePermissions, OnUpdatePermissions)
    IPC_MESSAGE_HANDLER(ExtensionMsg_UpdateUserScripts, OnUpdateUserScripts)
    IPC_MESSAGE_HANDLER(ExtensionMsg_UsingWebRequestAPI, OnUsingWebRequestAPI)
    IPC_MESSAGE_HANDLER(ExtensionMsg_ShouldClose, OnShouldClose)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ExtensionDispatcher::WebKitInitialized() {
  // For extensions, we want to ensure we call the IdleHandler every so often,
  // even if the extension keeps up activity.
  if (is_extension_process_) {
    forced_idle_timer_.Start(FROM_HERE,
        base::TimeDelta::FromMilliseconds(kMaxExtensionIdleHandlerDelayMs),
        RenderThread::Get(), &RenderThread::IdleHandler);
  }

  // Initialize host permissions for any extensions that were activated before
  // WebKit was initialized.
  for (std::set<std::string>::iterator iter = active_extension_ids_.begin();
       iter != active_extension_ids_.end(); ++iter) {
    const Extension* extension = extensions_.GetByID(*iter);
    if (extension)
      InitOriginPermissions(extension);
  }

  is_webkit_initialized_ = true;
}

void ExtensionDispatcher::IdleNotification() {
  if (is_extension_process_) {
    // Dampen the forced delay as well if the extension stays idle for long
    // periods of time.
    int64 forced_delay_ms = std::max(
        RenderThread::Get()->GetIdleNotificationDelayInMs(),
        kMaxExtensionIdleHandlerDelayMs);
    forced_idle_timer_.Stop();
    forced_idle_timer_.Start(FROM_HERE,
        base::TimeDelta::FromMilliseconds(forced_delay_ms),
        RenderThread::Get(), &RenderThread::IdleHandler);
  }
}

void ExtensionDispatcher::OnSetFunctionNames(
    const std::vector<std::string>& names) {
  function_names_.clear();
  for (size_t i = 0; i < names.size(); ++i)
    function_names_.insert(names[i]);
}

void ExtensionDispatcher::OnMessageInvoke(const std::string& extension_id,
                                          const std::string& function_name,
                                          const ListValue& args,
                                          const GURL& event_url,
                                          bool user_gesture) {
  scoped_ptr<WebScopedUserGesture> web_user_gesture;
  if (user_gesture) {
    web_user_gesture.reset(new WebScopedUserGesture);
  }

  v8_context_set_.DispatchChromeHiddenMethod(
      extension_id, function_name, args, NULL, event_url);

  // Reset the idle handler each time there's any activity like event or message
  // dispatch, for which Invoke is the chokepoint.
  if (is_extension_process_) {
    RenderThread::Get()->ScheduleIdleHandler(
        kInitialExtensionIdleHandlerDelayMs);
  }

  const Extension* extension = extensions_.GetByID(extension_id);
  // Tell the browser process that the event is dispatched and we're idle.
  // TODO(mpcomplete): differentiate between background page and other views.
  if (extension && !extension->background_page_persists() &&
      function_name == "Event.dispatchJSON") { // may always be true
    RenderThread::Get()->Send(
        new ExtensionHostMsg_ExtensionEventAck(extension_id));
  }
}

void ExtensionDispatcher::OnDeliverMessage(int target_port_id,
                                           const std::string& message) {
  MiscellaneousBindings::DeliverMessage(
      v8_context_set_.GetAll(),
      target_port_id,
      message,
      NULL);  // All render views.
}

void ExtensionDispatcher::OnLoaded(
    const std::vector<ExtensionMsg_Loaded_Params>& loaded_extensions) {
  std::vector<WebString> platform_app_patterns;

  std::vector<ExtensionMsg_Loaded_Params>::const_iterator i;
  for (i = loaded_extensions.begin(); i != loaded_extensions.end(); ++i) {
    scoped_refptr<const Extension> extension(i->ConvertToExtension());
    if (!extension) {
      // This can happen if extension parsing fails for any reason. One reason
      // this can legitimately happen is if the
      // --enable-experimental-extension-apis changes at runtime, which happens
      // during browser tests. Existing renderers won't know about the change.
      continue;
    }

    extensions_.Insert(extension);

    if (extension->is_platform_app()) {
      platform_app_patterns.push_back(
          WebString::fromUTF8(extension->url().spec() + "*"));
    }
  }

  if (!platform_app_patterns.empty()) {
    // We have collected a set of platform-app extensions, so let's tell WebKit
    // about them so that it can provide a default stylesheet for them.
    //
    // TODO(miket): consider enhancing WebView to allow removing
    // single stylesheets, or else to edit the pattern set associated
    // with one.
    WebVector<WebString> patterns;
    patterns.assign(platform_app_patterns);
    WebView::addUserStyleSheet(
        WebString::fromUTF8(ResourceBundle::GetSharedInstance().
                            GetRawDataResource(IDR_PLATFORM_APP_CSS)),
        patterns,
        WebView::UserContentInjectInAllFrames,
        WebView::UserStyleInjectInExistingDocuments);
  }
}

void ExtensionDispatcher::OnUnloaded(const std::string& id) {
  extensions_.Remove(id);
  // If the extension is later reloaded with a different set of permissions,
  // we'd like it to get a new isolated world ID, so that it can pick up the
  // changed origin whitelist.
  user_script_slave_->RemoveIsolatedWorld(id);

  // We don't do anything with existing platform-app stylesheets. They will
  // stay resident, but the URL pattern corresponding to the unloaded
  // extension's URL just won't match anything anymore.
}

void ExtensionDispatcher::OnSetScriptingWhitelist(
    const Extension::ScriptingWhitelist& extension_ids) {
  Extension::SetScriptingWhitelist(extension_ids);
}

bool ExtensionDispatcher::IsApplicationActive(
    const std::string& extension_id) const {
  return active_application_ids_.find(extension_id) !=
      active_application_ids_.end();
}

bool ExtensionDispatcher::IsExtensionActive(
    const std::string& extension_id) const {
  return active_extension_ids_.find(extension_id) !=
      active_extension_ids_.end();
}

bool ExtensionDispatcher::AllowScriptExtension(
    WebFrame* frame,
    const std::string& v8_extension_name,
    int extension_group) {
  return AllowScriptExtension(frame, v8_extension_name, extension_group, 0);
}

namespace {

// This is what the extension_group variable will be when DidCreateScriptContext
// is called. We know because it's the same as what AllowScriptExtension gets
// passed, and the two functions are called sequentially from WebKit.
//
// TODO(koz): Plumb extension_group through to AllowScriptExtension() from
// WebKit.
static int g_hack_extension_group = 0;

}

bool ExtensionDispatcher::AllowScriptExtension(
    WebFrame* frame,
    const std::string& v8_extension_name,
    int extension_group,
    int world_id) {
  g_hack_extension_group = extension_group;
  return true;
}

bool ExtensionDispatcher::AllowCustomAPI(
    WebFrame* frame,
    const std::string& custom_binding_api_name,
    int world_id) {
  std::string extension_id = GetExtensionID(frame, world_id);
  if (test_extension_id_ == extension_id)
    return true;
  const Extension* extension = extensions_.GetByID(extension_id);
  if (!extension) {
    // This can happen when a resource is blocked due to CSP; a valid
    // chrome-extension:// URL is navigated to, so it passes the initial
    // checks, but the URL gets changed to "chrome-extension://invalid"
    // afterwards (see chrome_content_renderer_client.cc). An extension
    // page still gets loaded, just for the extension with ID "invalid",
    // which of course isn't found so GetById extension will be NULL.
    //
    // Reference: http://crbug.com/111614.
    CHECK_EQ("invalid", extension_id);
    return false;
  }
  return custom_bindings_util::AllowAPIInjection(
      custom_binding_api_name, *extension, this);
}

void ExtensionDispatcher::RegisterNativeHandlers(ModuleSystem* module_system,
                                                 ChromeV8Context* context) {
  module_system->RegisterNativeHandler("app",
      scoped_ptr<NativeHandler>(new AppBindings(this, context)));
  module_system->RegisterNativeHandler("webstore",
      scoped_ptr<NativeHandler>(new WebstoreBindings(this, context)));
  module_system->RegisterNativeHandler("event_bindings",
      scoped_ptr<NativeHandler>(EventBindings::Get(this)));
  module_system->RegisterNativeHandler("miscellaneous_bindings",
      scoped_ptr<NativeHandler>(MiscellaneousBindings::Get(this)));
  module_system->RegisterNativeHandler("schema_generated_bindings",
      scoped_ptr<NativeHandler>(SchemaGeneratedBindings::Get(this)));

  // Custom bindings.
  module_system->RegisterNativeHandler("chrome_private",
      scoped_ptr<NativeHandler>(
          new ChromePrivateCustomBindings(this)));
  module_system->RegisterNativeHandler("context_menus",
      scoped_ptr<NativeHandler>(new ContextMenusCustomBindings()));
  module_system->RegisterNativeHandler("extension",
      scoped_ptr<NativeHandler>(
          new ExtensionCustomBindings(this)));
  module_system->RegisterNativeHandler("experimental_socket",
      scoped_ptr<NativeHandler>(new ExperimentalSocketCustomBindings()));
  module_system->RegisterNativeHandler("file_browser_handler",
      scoped_ptr<NativeHandler>(new FileBrowserHandlerCustomBindings()));
  module_system->RegisterNativeHandler("file_browser_private",
      scoped_ptr<NativeHandler>(new FileBrowserPrivateCustomBindings()));
  module_system->RegisterNativeHandler("i18n",
      scoped_ptr<NativeHandler>(new I18NCustomBindings()));
  module_system->RegisterNativeHandler("page_actions",
      scoped_ptr<NativeHandler>(
          new PageActionsCustomBindings(this)));
  module_system->RegisterNativeHandler("page_capture",
      scoped_ptr<NativeHandler>(new PageCaptureCustomBindings()));
  module_system->RegisterNativeHandler("tabs",
      scoped_ptr<NativeHandler>(new TabsCustomBindings()));
  module_system->RegisterNativeHandler("tts",
      scoped_ptr<NativeHandler>(new TTSCustomBindings()));
  module_system->RegisterNativeHandler("web_request",
      scoped_ptr<NativeHandler>(new WebRequestCustomBindings()));
}

void ExtensionDispatcher::PopulateSourceMap() {
  source_map_.RegisterSource("app", IDR_APP_BINDINGS_JS);
  source_map_.RegisterSource("webstore", IDR_WEBSTORE_BINDINGS_JS);
  source_map_.RegisterSource("event_bindings", IDR_EVENT_BINDINGS_JS);
  source_map_.RegisterSource("miscellaneous_bindings",
      IDR_MISCELLANEOUS_BINDINGS_JS);
  source_map_.RegisterSource("schema_generated_bindings",
      IDR_SCHEMA_GENERATED_BINDINGS_JS);
  source_map_.RegisterSource("json_schema", IDR_JSON_SCHEMA_JS);
  source_map_.RegisterSource("apitest", IDR_EXTENSION_APITEST_JS);
  source_map_.RegisterSource("setup_bindings", IDR_SETUP_BINDINGS_JS);

    // Custom bindings.
  source_map_.RegisterSource("browserAction",
                             IDR_BROWSER_ACTION_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("chromePrivate",
                             IDR_CHROME_PRIVATE_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("contentSettings",
                             IDR_CONTENT_SETTINGS_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("contextMenus",
                             IDR_CONTEXT_MENUS_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("devtools", IDR_DEVTOOLS_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("experimental.declarative",
                             IDR_EXPERIMENTAL_DECLARATIVE_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("experimental.socket",
                             IDR_EXPERIMENTAL_SOCKET_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("extension", IDR_EXTENSION_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("fileBrowserHandler",
                             IDR_FILE_BROWSER_HANDLER_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("fileBrowserPrivate",
                             IDR_FILE_BROWSER_PRIVATE_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("i18n", IDR_I18N_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("input.ime", IDR_INPUT_IME_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("omnibox", IDR_OMNIBOX_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("pageActions",
                             IDR_PAGE_ACTIONS_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("pageAction", IDR_PAGE_ACTION_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("pageCapture",
                             IDR_PAGE_CAPTURE_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("storage", IDR_STORAGE_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("tabs", IDR_TABS_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("tts", IDR_TTS_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("ttsEngine", IDR_TTS_ENGINE_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("types", IDR_TYPES_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("webRequest", IDR_WEB_REQUEST_CUSTOM_BINDINGS_JS);
}

void ExtensionDispatcher::DidCreateScriptContext(
    WebFrame* frame, v8::Handle<v8::Context> v8_context, int extension_group,
    int world_id) {
  // TODO(koz): If the caller didn't pass extension_group, use the last value.
  if (extension_group == -1)
    extension_group = g_hack_extension_group;

  std::string extension_id = GetExtensionID(frame, world_id);
  ChromeV8Context* context =
      new ChromeV8Context(
          v8_context,
          frame,
          extension_id,
          ExtensionGroupToContextType(extension_group));
  v8_context_set_.Add(context);

  scoped_ptr<ModuleSystem> module_system(new ModuleSystem(&source_map_));
  RegisterNativeHandlers(module_system.get(), context);

  bool is_bindings_allowed =
      IsTestExtensionId(extension_id) ||
      context->context_type() == ChromeV8Context::CONTENT_SCRIPT ||
      extensions_.ExtensionBindingsAllowed(ExtensionURLInfo(
          frame->document().securityOrigin(),
          UserScriptSlave::GetDataSourceURLForFrame(frame)));

  module_system->RegisterNativeHandler("chrome_hidden",
      scoped_ptr<NativeHandler>(new ChromeHiddenNativeHandler()));
  module_system->RegisterNativeHandler("context_info",
      scoped_ptr<NativeHandler>(new ContextInfoNativeHandler(
          this,
          is_bindings_allowed,
          frame,
          world_id)));
  module_system->RegisterNativeHandler("print",
      scoped_ptr<NativeHandler>(new PrintNativeHandler()));

  const Extension* extension = extensions_.GetByID(context->extension_id());
  int manifest_version = 1;
  if (extension)
    manifest_version = extension->manifest_version();

  module_system->RunString("var chrome; chrome = chrome || {};",
                           "setup-chrome-object");
  module_system->Require("setup_bindings");
  module_system->set_natives_enabled(false);

  context->set_module_system(module_system.Pass());

  context->DispatchOnLoadEvent(
      is_extension_process_,
      ChromeRenderProcessObserver::is_incognito_process(),
      manifest_version);

  VLOG(1) << "Num tracked contexts: " << v8_context_set_.size();
}

std::string ExtensionDispatcher::GetExtensionID(WebFrame* frame, int world_id) {
  if (!test_extension_id_.empty()) {
    return test_extension_id_;
  } else if (world_id != 0) {
    // Isolated worlds (content script).
    return user_script_slave_->GetExtensionIdForIsolatedWorld(world_id);
  } else {
    // Extension pages (chrome-extension:// URLs).
    GURL frame_url = UserScriptSlave::GetDataSourceURLForFrame(frame);
    return extensions_.GetExtensionOrAppIDByURL(
        ExtensionURLInfo(frame->document().securityOrigin(), frame_url));
  }
}

void ExtensionDispatcher::WillReleaseScriptContext(
    WebFrame* frame, v8::Handle<v8::Context> v8_context, int world_id) {
  ChromeV8Context* context = v8_context_set_.GetByV8Context(v8_context);
  if (!context)
    return;

  context->DispatchOnUnloadEvent();

  v8_context_set_.Remove(context);
  VLOG(1) << "Num tracked contexts: " << v8_context_set_.size();
}

void ExtensionDispatcher::SetTestExtensionId(const std::string& id) {
  test_extension_id_ = id;
}

bool ExtensionDispatcher::IsTestExtensionId(const std::string& id) {
  return !test_extension_id_.empty() && id == test_extension_id_;
}

void ExtensionDispatcher::OnActivateApplication(
    const std::string& extension_id) {
  active_application_ids_.insert(extension_id);
}

void ExtensionDispatcher::OnActivateExtension(
    const std::string& extension_id) {
  active_extension_ids_.insert(extension_id);

  // This is called when starting a new extension page, so start the idle
  // handler ticking.
  RenderThread::Get()->ScheduleIdleHandler(kInitialExtensionIdleHandlerDelayMs);

  UpdateActiveExtensions();

  const Extension* extension = extensions_.GetByID(extension_id);
  if (!extension)
    return;

  if (is_webkit_initialized_)
    InitOriginPermissions(extension);
}

void ExtensionDispatcher::InitOriginPermissions(const Extension* extension) {
  // TODO(jstritar): We should try to remove this special case. Also, these
  // whitelist entries need to be updated when the kManagement permission
  // changes.
  if (extension->HasAPIPermission(ExtensionAPIPermission::kManagement)) {
    WebSecurityPolicy::addOriginAccessWhitelistEntry(
        extension->url(),
        WebString::fromUTF8(chrome::kChromeUIScheme),
        WebString::fromUTF8(chrome::kChromeUIExtensionIconHost),
        false);
  }

  UpdateOriginPermissions(UpdatedExtensionPermissionsInfo::ADDED,
                          extension,
                          extension->GetActivePermissions()->explicit_hosts());
}

void ExtensionDispatcher::UpdateOriginPermissions(
    UpdatedExtensionPermissionsInfo::Reason reason,
    const Extension* extension,
    const URLPatternSet& origins) {
  for (URLPatternSet::const_iterator i = origins.begin();
       i != origins.end(); ++i) {
    const char* schemes[] = {
      chrome::kHttpScheme,
      chrome::kHttpsScheme,
      chrome::kFileScheme,
      chrome::kChromeUIScheme,
    };
    for (size_t j = 0; j < arraysize(schemes); ++j) {
      if (i->MatchesScheme(schemes[j])) {
        ((reason == UpdatedExtensionPermissionsInfo::REMOVED) ?
         WebSecurityPolicy::removeOriginAccessWhitelistEntry :
         WebSecurityPolicy::addOriginAccessWhitelistEntry)(
              extension->url(),
              WebString::fromUTF8(schemes[j]),
              WebString::fromUTF8(i->host()),
              i->match_subdomains());
      }
    }
  }
}

void ExtensionDispatcher::OnUpdatePermissions(
    int reason_id,
    const std::string& extension_id,
    const ExtensionAPIPermissionSet& apis,
    const URLPatternSet& explicit_hosts,
    const URLPatternSet& scriptable_hosts) {
  const Extension* extension = extensions_.GetByID(extension_id);
  if (!extension)
    return;

  scoped_refptr<const ExtensionPermissionSet> delta =
      new ExtensionPermissionSet(apis, explicit_hosts, scriptable_hosts);
  scoped_refptr<const ExtensionPermissionSet> old_active =
      extension->GetActivePermissions();
  UpdatedExtensionPermissionsInfo::Reason reason =
      static_cast<UpdatedExtensionPermissionsInfo::Reason>(reason_id);

  const ExtensionPermissionSet* new_active = NULL;
  if (reason == UpdatedExtensionPermissionsInfo::ADDED) {
    new_active = ExtensionPermissionSet::CreateUnion(old_active, delta);
  } else {
    CHECK_EQ(UpdatedExtensionPermissionsInfo::REMOVED, reason);
    new_active = ExtensionPermissionSet::CreateDifference(old_active, delta);
  }

  extension->SetActivePermissions(new_active);
  UpdateOriginPermissions(reason, extension, explicit_hosts);
}

void ExtensionDispatcher::OnUpdateUserScripts(
    base::SharedMemoryHandle scripts) {
  DCHECK(base::SharedMemory::IsHandleValid(scripts)) << "Bad scripts handle";
  user_script_slave_->UpdateScripts(scripts);
  UpdateActiveExtensions();
}

void ExtensionDispatcher::UpdateActiveExtensions() {
  // In single-process mode, the browser process reports the active extensions.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return;

  std::set<std::string> active_extensions = active_extension_ids_;
  user_script_slave_->GetActiveExtensions(&active_extensions);
  child_process_logging::SetActiveExtensions(active_extensions);
}

void ExtensionDispatcher::RegisterExtension(v8::Extension* extension,
                                            bool restrict_to_extensions) {
  if (restrict_to_extensions)
    restricted_v8_extensions_.insert(extension->name());

  RenderThread::Get()->RegisterExtension(extension);
}

void ExtensionDispatcher::OnUsingWebRequestAPI(
    bool adblock, bool adblock_plus, bool other) {
  webrequest_adblock_ = adblock;
  webrequest_adblock_plus_ = adblock_plus;
  webrequest_other_ = other;
}

void ExtensionDispatcher::OnShouldClose(const std::string& extension_id,
                                        int sequence_id) {
  RenderThread::Get()->Send(
      new ExtensionHostMsg_ShouldCloseAck(extension_id, sequence_id));
}
