// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/dispatcher.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/externally_connectable.h"
#include "extensions/common/manifest_handlers/sandboxed_page_info.h"
#include "extensions/common/message_bundle.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"
#include "extensions/common/view_type.h"
#include "extensions/renderer/api_activity_logger.h"
#include "extensions/renderer/api_definitions_natives.h"
#include "extensions/renderer/app_runtime_custom_bindings.h"
#include "extensions/renderer/binding_generating_native_handler.h"
#include "extensions/renderer/blob_native_handler.h"
#include "extensions/renderer/content_watcher.h"
#include "extensions/renderer/context_menus_custom_bindings.h"
#include "extensions/renderer/css_native_handler.h"
#include "extensions/renderer/dispatcher_delegate.h"
#include "extensions/renderer/document_custom_bindings.h"
#include "extensions/renderer/dom_activity_logger.h"
#include "extensions/renderer/event_bindings.h"
#include "extensions/renderer/extension_groups.h"
#include "extensions/renderer/extension_helper.h"
#include "extensions/renderer/extensions_renderer_client.h"
#include "extensions/renderer/file_system_natives.h"
#include "extensions/renderer/i18n_custom_bindings.h"
#include "extensions/renderer/id_generator_custom_bindings.h"
#include "extensions/renderer/lazy_background_page_native_handler.h"
#include "extensions/renderer/logging_native_handler.h"
#include "extensions/renderer/messaging_bindings.h"
#include "extensions/renderer/module_system.h"
#include "extensions/renderer/print_native_handler.h"
#include "extensions/renderer/process_info_native_handler.h"
#include "extensions/renderer/render_view_observer_natives.h"
#include "extensions/renderer/request_sender.h"
#include "extensions/renderer/runtime_custom_bindings.h"
#include "extensions/renderer/safe_builtins.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "extensions/renderer/script_injection.h"
#include "extensions/renderer/script_injection_manager.h"
#include "extensions/renderer/send_request_natives.h"
#include "extensions/renderer/set_icon_natives.h"
#include "extensions/renderer/test_features_native_handler.h"
#include "extensions/renderer/user_gestures_native_handler.h"
#include "extensions/renderer/utils_native_handler.h"
#include "extensions/renderer/v8_context_native_handler.h"
#include "grit/content_resources.h"
#include "grit/extensions_renderer_resources.h"
#include "mojo/public/js/bindings/constants.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebCustomElement.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/web/WebScopedUserGesture.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "v8/include/v8.h"

using base::UserMetricsAction;
using blink::WebDataSource;
using blink::WebDocument;
using blink::WebFrame;
using blink::WebScopedUserGesture;
using blink::WebSecurityPolicy;
using blink::WebString;
using blink::WebVector;
using blink::WebView;
using content::RenderThread;
using content::RenderView;

namespace extensions {

namespace {

static const int64 kInitialExtensionIdleHandlerDelayMs = 5 * 1000;
static const int64 kMaxExtensionIdleHandlerDelayMs = 5 * 60 * 1000;
static const char kEventDispatchFunction[] = "dispatchEvent";
static const char kOnSuspendEvent[] = "runtime.onSuspend";
static const char kOnSuspendCanceledEvent[] = "runtime.onSuspendCanceled";

// Returns the global value for "chrome" from |context|. If one doesn't exist
// creates a new object for it.
//
// Note that this isn't necessarily an object, since webpages can write, for
// example, "window.chrome = true".
v8::Handle<v8::Value> GetOrCreateChrome(ScriptContext* context) {
  v8::Handle<v8::String> chrome_string(
      v8::String::NewFromUtf8(context->isolate(), "chrome"));
  v8::Handle<v8::Object> global(context->v8_context()->Global());
  v8::Handle<v8::Value> chrome(global->Get(chrome_string));
  if (chrome->IsUndefined()) {
    chrome = v8::Object::New(context->isolate());
    global->Set(chrome_string, chrome);
  }
  return chrome;
}

// Returns |value| cast to an object if possible, else an empty handle.
v8::Handle<v8::Object> AsObjectOrEmpty(v8::Handle<v8::Value> value) {
  return value->IsObject() ? value.As<v8::Object>() : v8::Handle<v8::Object>();
}

// Calls a method |method_name| in a module |module_name| belonging to the
// module system from |context|. Intended as a callback target from
// ScriptContextSet::ForEach.
void CallModuleMethod(const std::string& module_name,
                      const std::string& method_name,
                      const base::ListValue* args,
                      ScriptContext* context) {
  v8::HandleScope handle_scope(context->isolate());
  v8::Context::Scope context_scope(context->v8_context());

  scoped_ptr<content::V8ValueConverter> converter(
      content::V8ValueConverter::create());

  std::vector<v8::Handle<v8::Value> > arguments;
  for (base::ListValue::const_iterator it = args->begin(); it != args->end();
       ++it) {
    arguments.push_back(converter->ToV8Value(*it, context->v8_context()));
  }

  context->module_system()->CallModuleMethod(
      module_name, method_name, &arguments);
}

// This handles the "chrome." root API object in script contexts.
class ChromeNativeHandler : public ObjectBackedNativeHandler {
 public:
  explicit ChromeNativeHandler(ScriptContext* context)
      : ObjectBackedNativeHandler(context) {
    RouteFunction(
        "GetChrome",
        base::Bind(&ChromeNativeHandler::GetChrome, base::Unretained(this)));
  }

  void GetChrome(const v8::FunctionCallbackInfo<v8::Value>& args) {
    args.GetReturnValue().Set(GetOrCreateChrome(context()));
  }
};

}  // namespace

Dispatcher::Dispatcher(DispatcherDelegate* delegate)
    : delegate_(delegate),
      content_watcher_(new ContentWatcher()),
      source_map_(&ResourceBundle::GetSharedInstance()),
      v8_schema_registry_(new V8SchemaRegistry),
      is_webkit_initialized_(false),
      user_script_set_manager_observer_(this) {
  const CommandLine& command_line = *(CommandLine::ForCurrentProcess());
  is_extension_process_ =
      command_line.HasSwitch(extensions::switches::kExtensionProcess) ||
      command_line.HasSwitch(::switches::kSingleProcess);

  if (is_extension_process_) {
    RenderThread::Get()->SetIdleNotificationDelayInMs(
        kInitialExtensionIdleHandlerDelayMs);
  }

  RenderThread::Get()->RegisterExtension(SafeBuiltins::CreateV8Extension());

  user_script_set_manager_.reset(new UserScriptSetManager(&extensions_));
  script_injection_manager_.reset(
      new ScriptInjectionManager(&extensions_, user_script_set_manager_.get()));
  user_script_set_manager_observer_.Add(user_script_set_manager_.get());
  request_sender_.reset(new RequestSender(this));
  PopulateSourceMap();
}

Dispatcher::~Dispatcher() {
}

void Dispatcher::OnRenderViewCreated(content::RenderView* render_view) {
  script_injection_manager_->OnRenderViewCreated(render_view);
}

bool Dispatcher::IsExtensionActive(const std::string& extension_id) const {
  bool is_active =
      active_extension_ids_.find(extension_id) != active_extension_ids_.end();
  if (is_active)
    CHECK(extensions_.Contains(extension_id));
  return is_active;
}

std::string Dispatcher::GetExtensionID(const WebFrame* frame, int world_id) {
  if (world_id != 0) {
    // Isolated worlds (content script).
    return ScriptInjection::GetExtensionIdForIsolatedWorld(world_id);
  }

  // TODO(kalman): Delete this check.
  if (frame->document().securityOrigin().isUnique())
    return std::string();

  // Extension pages (chrome-extension:// URLs).
  GURL frame_url = ScriptContext::GetDataSourceURLForFrame(frame);
  return extensions_.GetExtensionOrAppIDByURL(frame_url);
}

void Dispatcher::DidCreateScriptContext(
    WebFrame* frame,
    const v8::Handle<v8::Context>& v8_context,
    int extension_group,
    int world_id) {
#if !defined(ENABLE_EXTENSIONS)
  return;
#endif

  std::string extension_id = GetExtensionID(frame, world_id);

  const Extension* extension = extensions_.GetByID(extension_id);
  if (!extension && !extension_id.empty()) {
    // There are conditions where despite a context being associated with an
    // extension, no extension actually gets found.  Ignore "invalid" because
    // CSP blocks extension page loading by switching the extension ID to
    // "invalid". This isn't interesting.
    if (extension_id != "invalid") {
      LOG(ERROR) << "Extension \"" << extension_id << "\" not found";
      RenderThread::Get()->RecordAction(
          UserMetricsAction("ExtensionNotFound_ED"));
    }

    extension_id = "";
  }

  Feature::Context context_type =
      ClassifyJavaScriptContext(extension,
                                extension_group,
                                ScriptContext::GetDataSourceURLForFrame(frame),
                                frame->document().securityOrigin());

  ScriptContext* context =
      delegate_->CreateScriptContext(v8_context, frame, extension, context_type)
          .release();
  script_context_set_.Add(context);

  // Initialize origin permissions for content scripts, which can't be
  // initialized in |OnActivateExtension|.
  if (context_type == Feature::CONTENT_SCRIPT_CONTEXT)
    InitOriginPermissions(extension);

  {
    scoped_ptr<ModuleSystem> module_system(
        new ModuleSystem(context, &source_map_));
    context->set_module_system(module_system.Pass());
  }
  ModuleSystem* module_system = context->module_system();

  // Enable natives in startup.
  ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system);

  RegisterNativeHandlers(module_system, context);

  // chrome.Event is part of the public API (although undocumented). Make it
  // lazily evalulate to Event from event_bindings.js. For extensions only
  // though, not all webpages!
  if (context->extension()) {
    v8::Handle<v8::Object> chrome = AsObjectOrEmpty(GetOrCreateChrome(context));
    if (!chrome.IsEmpty())
      module_system->SetLazyField(chrome, "Event", kEventBindings, "Event");
  }

  UpdateBindingsForContext(context);

  bool is_within_platform_app = IsWithinPlatformApp();
  // Inject custom JS into the platform app context.
  if (is_within_platform_app) {
    module_system->Require("platformApp");
  }

  delegate_->RequireAdditionalModules(context, is_within_platform_app);

  VLOG(1) << "Num tracked contexts: " << script_context_set_.size();
}

void Dispatcher::WillReleaseScriptContext(
    WebFrame* frame,
    const v8::Handle<v8::Context>& v8_context,
    int world_id) {
  ScriptContext* context = script_context_set_.GetByV8Context(v8_context);
  if (!context)
    return;

  context->DispatchOnUnloadEvent();
  // TODO(kalman): add an invalidation observer interface to ScriptContext.
  request_sender_->InvalidateSource(context);

  script_context_set_.Remove(context);
  VLOG(1) << "Num tracked contexts: " << script_context_set_.size();
}

void Dispatcher::DidCreateDocumentElement(blink::WebFrame* frame) {
  // Note: use GetEffectiveDocumentURL not just frame->document()->url()
  // so that this also injects the stylesheet on about:blank frames that
  // are hosted in the extension process.
  GURL effective_document_url = ScriptContext::GetEffectiveDocumentURL(
      frame, frame->document().url(), true /* match_about_blank */);
  const Extension* extension =
      extensions_.GetExtensionOrAppByURL(effective_document_url);

  if (extension &&
      (extension->is_extension() || extension->is_platform_app())) {
    int resource_id =
        extension->is_platform_app() ? IDR_PLATFORM_APP_CSS : IDR_EXTENSION_CSS;
    std::string stylesheet = ResourceBundle::GetSharedInstance()
                                 .GetRawDataResource(resource_id)
                                 .as_string();
    ReplaceFirstSubstringAfterOffset(
        &stylesheet, 0, "$FONTFAMILY", system_font_family_);
    ReplaceFirstSubstringAfterOffset(
        &stylesheet, 0, "$FONTSIZE", system_font_size_);

    // Blink doesn't let us define an additional user agent stylesheet, so
    // we insert the default platform app or extension stylesheet into all
    // documents that are loaded in each app or extension.
    frame->document().insertStyleSheet(WebString::fromUTF8(stylesheet));
  }
  content_watcher_->DidCreateDocumentElement(frame);
}

void Dispatcher::DidMatchCSS(
    blink::WebFrame* frame,
    const blink::WebVector<blink::WebString>& newly_matching_selectors,
    const blink::WebVector<blink::WebString>& stopped_matching_selectors) {
  content_watcher_->DidMatchCSS(
      frame, newly_matching_selectors, stopped_matching_selectors);
}

void Dispatcher::OnExtensionResponse(int request_id,
                                     bool success,
                                     const base::ListValue& response,
                                     const std::string& error) {
  request_sender_->HandleResponse(request_id, success, response, error);
}

bool Dispatcher::CheckContextAccessToExtensionAPI(
    const std::string& function_name,
    ScriptContext* context) const {
  if (!context) {
    DLOG(ERROR) << "Not in a v8::Context";
    return false;
  }

  // Theoretically we could end up with bindings being injected into sandboxed
  // frames, for example content scripts. Don't let them execute API functions.
  blink::WebFrame* frame = context->web_frame();
  if (IsSandboxedPage(ScriptContext::GetDataSourceURLForFrame(frame))) {
    static const char kMessage[] =
        "%s cannot be used within a sandboxed frame.";
    std::string error_msg = base::StringPrintf(kMessage, function_name.c_str());
    context->isolate()->ThrowException(v8::Exception::Error(
        v8::String::NewFromUtf8(context->isolate(), error_msg.c_str())));
    return false;
  }

  Feature::Availability availability = context->GetAvailability(function_name);
  if (!availability.is_available()) {
    context->isolate()->ThrowException(
        v8::Exception::Error(v8::String::NewFromUtf8(
            context->isolate(), availability.message().c_str())));
  }

  return availability.is_available();
}

void Dispatcher::DispatchEvent(const std::string& extension_id,
                               const std::string& event_name) const {
  base::ListValue args;
  args.Set(0, new base::StringValue(event_name));
  args.Set(1, new base::ListValue());

  // Needed for Windows compilation, since kEventBindings is declared extern.
  const char* local_event_bindings = kEventBindings;
  script_context_set_.ForEach(extension_id,
                              base::Bind(&CallModuleMethod,
                                         local_event_bindings,
                                         kEventDispatchFunction,
                                         &args));
}

void Dispatcher::InvokeModuleSystemMethod(content::RenderView* render_view,
                                          const std::string& extension_id,
                                          const std::string& module_name,
                                          const std::string& function_name,
                                          const base::ListValue& args,
                                          bool user_gesture) {
  scoped_ptr<WebScopedUserGesture> web_user_gesture;
  if (user_gesture)
    web_user_gesture.reset(new WebScopedUserGesture);

  script_context_set_.ForEach(
      extension_id,
      render_view,
      base::Bind(&CallModuleMethod, module_name, function_name, &args));

  // Reset the idle handler each time there's any activity like event or message
  // dispatch, for which Invoke is the chokepoint.
  if (is_extension_process_) {
    RenderThread::Get()->ScheduleIdleHandler(
        kInitialExtensionIdleHandlerDelayMs);
  }

  // Tell the browser process when an event has been dispatched with a lazy
  // background page active.
  const Extension* extension = extensions_.GetByID(extension_id);
  if (extension && BackgroundInfo::HasLazyBackgroundPage(extension) &&
      module_name == kEventBindings &&
      function_name == kEventDispatchFunction) {
    RenderView* background_view =
        ExtensionHelper::GetBackgroundPage(extension_id);
    if (background_view) {
      background_view->Send(
          new ExtensionHostMsg_EventAck(background_view->GetRoutingID()));
    }
  }
}

void Dispatcher::ClearPortData(int port_id) {
  // Only the target port side has entries in |port_to_tab_id_map_|. If
  // |port_id| is a source port, std::map::erase() will just silently fail
  // here as a no-op.
  port_to_tab_id_map_.erase(port_id);
}

// static
std::vector<std::pair<std::string, int> > Dispatcher::GetJsResources() {
  std::vector<std::pair<std::string, int> > resources;

  // Libraries.
  resources.push_back(std::make_pair("entryIdManager", IDR_ENTRY_ID_MANAGER));
  resources.push_back(std::make_pair(kEventBindings, IDR_EVENT_BINDINGS_JS));
  resources.push_back(std::make_pair("imageUtil", IDR_IMAGE_UTIL_JS));
  resources.push_back(std::make_pair("json_schema", IDR_JSON_SCHEMA_JS));
  resources.push_back(std::make_pair("lastError", IDR_LAST_ERROR_JS));
  resources.push_back(std::make_pair("messaging", IDR_MESSAGING_JS));
  resources.push_back(
      std::make_pair("messaging_utils", IDR_MESSAGING_UTILS_JS));
  resources.push_back(std::make_pair(kSchemaUtils, IDR_SCHEMA_UTILS_JS));
  resources.push_back(std::make_pair("sendRequest", IDR_SEND_REQUEST_JS));
  resources.push_back(std::make_pair("setIcon", IDR_SET_ICON_JS));
  resources.push_back(std::make_pair("test", IDR_TEST_CUSTOM_BINDINGS_JS));
  resources.push_back(
      std::make_pair("test_environment_specific_bindings",
                     IDR_BROWSER_TEST_ENVIRONMENT_SPECIFIC_BINDINGS_JS));
  resources.push_back(std::make_pair("uncaught_exception_handler",
                                     IDR_UNCAUGHT_EXCEPTION_HANDLER_JS));
  resources.push_back(std::make_pair("unload_event", IDR_UNLOAD_EVENT_JS));
  resources.push_back(std::make_pair("utils", IDR_UTILS_JS));
  resources.push_back(
      std::make_pair(mojo::kBufferModuleName, IDR_MOJO_BUFFER_JS));
  resources.push_back(
      std::make_pair(mojo::kCodecModuleName, IDR_MOJO_CODEC_JS));
  resources.push_back(
      std::make_pair(mojo::kConnectionModuleName, IDR_MOJO_CONNECTION_JS));
  resources.push_back(
      std::make_pair(mojo::kConnectorModuleName, IDR_MOJO_CONNECTOR_JS));
  resources.push_back(
      std::make_pair(mojo::kRouterModuleName, IDR_MOJO_ROUTER_JS));
  resources.push_back(
      std::make_pair(mojo::kUnicodeModuleName, IDR_MOJO_UNICODE_JS));
  resources.push_back(
      std::make_pair(mojo::kValidatorModuleName, IDR_MOJO_VALIDATOR_JS));

  // Custom bindings.
  resources.push_back(
      std::make_pair("app.runtime", IDR_APP_RUNTIME_CUSTOM_BINDINGS_JS));
  resources.push_back(
      std::make_pair("contextMenus", IDR_CONTEXT_MENUS_CUSTOM_BINDINGS_JS));
  resources.push_back(
      std::make_pair("extension", IDR_EXTENSION_CUSTOM_BINDINGS_JS));
  resources.push_back(std::make_pair("i18n", IDR_I18N_CUSTOM_BINDINGS_JS));
  resources.push_back(
      std::make_pair("permissions", IDR_PERMISSIONS_CUSTOM_BINDINGS_JS));
  resources.push_back(
      std::make_pair("runtime", IDR_RUNTIME_CUSTOM_BINDINGS_JS));
  resources.push_back(std::make_pair("binding", IDR_BINDING_JS));

  // Custom types sources.
  resources.push_back(std::make_pair("StorageArea", IDR_STORAGE_AREA_JS));

  // Platform app sources that are not API-specific..
  resources.push_back(std::make_pair("platformApp", IDR_PLATFORM_APP_JS));

  return resources;
}

// NOTE: please use the naming convention "foo_natives" for these.
// static
void Dispatcher::RegisterNativeHandlers(ModuleSystem* module_system,
                                        ScriptContext* context,
                                        Dispatcher* dispatcher,
                                        RequestSender* request_sender,
                                        V8SchemaRegistry* v8_schema_registry) {
  module_system->RegisterNativeHandler(
      "chrome", scoped_ptr<NativeHandler>(new ChromeNativeHandler(context)));
  module_system->RegisterNativeHandler(
      "lazy_background_page",
      scoped_ptr<NativeHandler>(new LazyBackgroundPageNativeHandler(context)));
  module_system->RegisterNativeHandler(
      "logging", scoped_ptr<NativeHandler>(new LoggingNativeHandler(context)));
  module_system->RegisterNativeHandler("schema_registry",
                                       v8_schema_registry->AsNativeHandler());
  module_system->RegisterNativeHandler(
      "print", scoped_ptr<NativeHandler>(new PrintNativeHandler(context)));
  module_system->RegisterNativeHandler(
      "test_features",
      scoped_ptr<NativeHandler>(new TestFeaturesNativeHandler(context)));
  module_system->RegisterNativeHandler(
      "user_gestures",
      scoped_ptr<NativeHandler>(new UserGesturesNativeHandler(context)));
  module_system->RegisterNativeHandler(
      "utils", scoped_ptr<NativeHandler>(new UtilsNativeHandler(context)));
  module_system->RegisterNativeHandler(
      "v8_context",
      scoped_ptr<NativeHandler>(
          new V8ContextNativeHandler(context, dispatcher)));
  module_system->RegisterNativeHandler(
      "event_natives",
      scoped_ptr<NativeHandler>(new EventBindings(dispatcher, context)));
  module_system->RegisterNativeHandler(
      "messaging_natives",
      scoped_ptr<NativeHandler>(MessagingBindings::Get(dispatcher, context)));
  module_system->RegisterNativeHandler(
      "apiDefinitions",
      scoped_ptr<NativeHandler>(
          new ApiDefinitionsNatives(dispatcher, context)));
  module_system->RegisterNativeHandler(
      "sendRequest",
      scoped_ptr<NativeHandler>(
          new SendRequestNatives(request_sender, context)));
  module_system->RegisterNativeHandler(
      "setIcon",
      scoped_ptr<NativeHandler>(new SetIconNatives(request_sender, context)));
  module_system->RegisterNativeHandler(
      "activityLogger",
      scoped_ptr<NativeHandler>(new APIActivityLogger(context)));
  module_system->RegisterNativeHandler(
      "renderViewObserverNatives",
      scoped_ptr<NativeHandler>(new RenderViewObserverNatives(context)));

  // Natives used by multiple APIs.
  module_system->RegisterNativeHandler(
      "file_system_natives",
      scoped_ptr<NativeHandler>(new FileSystemNatives(context)));

  // Custom bindings.
  module_system->RegisterNativeHandler(
      "app_runtime",
      scoped_ptr<NativeHandler>(new AppRuntimeCustomBindings(context)));
  module_system->RegisterNativeHandler(
      "blob_natives",
      scoped_ptr<NativeHandler>(new BlobNativeHandler(context)));
  module_system->RegisterNativeHandler(
      "context_menus",
      scoped_ptr<NativeHandler>(new ContextMenusCustomBindings(context)));
  module_system->RegisterNativeHandler(
      "css_natives", scoped_ptr<NativeHandler>(new CssNativeHandler(context)));
  module_system->RegisterNativeHandler(
      "document_natives",
      scoped_ptr<NativeHandler>(new DocumentCustomBindings(context)));
  module_system->RegisterNativeHandler(
      "i18n", scoped_ptr<NativeHandler>(new I18NCustomBindings(context)));
  module_system->RegisterNativeHandler(
      "id_generator",
      scoped_ptr<NativeHandler>(new IdGeneratorCustomBindings(context)));
  module_system->RegisterNativeHandler(
      "runtime", scoped_ptr<NativeHandler>(new RuntimeCustomBindings(context)));
}

bool Dispatcher::OnControlMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(Dispatcher, message)
  IPC_MESSAGE_HANDLER(ExtensionMsg_ActivateExtension, OnActivateExtension)
  IPC_MESSAGE_HANDLER(ExtensionMsg_CancelSuspend, OnCancelSuspend)
  IPC_MESSAGE_HANDLER(ExtensionMsg_ClearTabSpecificPermissions,
                      OnClearTabSpecificPermissions)
  IPC_MESSAGE_HANDLER(ExtensionMsg_DeliverMessage, OnDeliverMessage)
  IPC_MESSAGE_HANDLER(ExtensionMsg_DispatchOnConnect, OnDispatchOnConnect)
  IPC_MESSAGE_HANDLER(ExtensionMsg_DispatchOnDisconnect, OnDispatchOnDisconnect)
  IPC_MESSAGE_HANDLER(ExtensionMsg_Loaded, OnLoaded)
  IPC_MESSAGE_HANDLER(ExtensionMsg_MessageInvoke, OnMessageInvoke)
  IPC_MESSAGE_HANDLER(ExtensionMsg_SetChannel, OnSetChannel)
  IPC_MESSAGE_HANDLER(ExtensionMsg_SetFunctionNames, OnSetFunctionNames)
  IPC_MESSAGE_HANDLER(ExtensionMsg_SetScriptingWhitelist,
                      OnSetScriptingWhitelist)
  IPC_MESSAGE_HANDLER(ExtensionMsg_SetSystemFont, OnSetSystemFont)
  IPC_MESSAGE_HANDLER(ExtensionMsg_ShouldSuspend, OnShouldSuspend)
  IPC_MESSAGE_HANDLER(ExtensionMsg_Suspend, OnSuspend)
  IPC_MESSAGE_HANDLER(ExtensionMsg_TransferBlobs, OnTransferBlobs)
  IPC_MESSAGE_HANDLER(ExtensionMsg_Unloaded, OnUnloaded)
  IPC_MESSAGE_HANDLER(ExtensionMsg_UpdatePermissions, OnUpdatePermissions)
  IPC_MESSAGE_HANDLER(ExtensionMsg_UpdateTabSpecificPermissions,
                      OnUpdateTabSpecificPermissions)
  IPC_MESSAGE_HANDLER(ExtensionMsg_UsingWebRequestAPI, OnUsingWebRequestAPI)
  IPC_MESSAGE_FORWARD(ExtensionMsg_WatchPages,
                      content_watcher_.get(),
                      ContentWatcher::OnWatchPages)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void Dispatcher::WebKitInitialized() {
  // For extensions, we want to ensure we call the IdleHandler every so often,
  // even if the extension keeps up activity.
  if (is_extension_process_) {
    forced_idle_timer_.reset(new base::RepeatingTimer<content::RenderThread>);
    forced_idle_timer_->Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kMaxExtensionIdleHandlerDelayMs),
        RenderThread::Get(),
        &RenderThread::IdleHandler);
  }

  // Initialize host permissions for any extensions that were activated before
  // WebKit was initialized.
  for (std::set<std::string>::iterator iter = active_extension_ids_.begin();
       iter != active_extension_ids_.end();
       ++iter) {
    const Extension* extension = extensions_.GetByID(*iter);
    CHECK(extension);

    InitOriginPermissions(extension);
  }

  EnableCustomElementWhiteList();

  is_webkit_initialized_ = true;
}

void Dispatcher::IdleNotification() {
  if (is_extension_process_ && forced_idle_timer_) {
    // Dampen the forced delay as well if the extension stays idle for long
    // periods of time. (forced_idle_timer_ can be NULL after
    // OnRenderProcessShutdown has been called.)
    int64 forced_delay_ms =
        std::max(RenderThread::Get()->GetIdleNotificationDelayInMs(),
                 kMaxExtensionIdleHandlerDelayMs);
    forced_idle_timer_->Stop();
    forced_idle_timer_->Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(forced_delay_ms),
        RenderThread::Get(),
        &RenderThread::IdleHandler);
  }
}

void Dispatcher::OnRenderProcessShutdown() {
  v8_schema_registry_.reset();
  forced_idle_timer_.reset();
}

void Dispatcher::OnActivateExtension(const std::string& extension_id) {
  const Extension* extension = extensions_.GetByID(extension_id);
  if (!extension) {
    // Extension was activated but was never loaded. This probably means that
    // the renderer failed to load it (or the browser failed to tell us when it
    // did). Failures shouldn't happen, but instead of crashing there (which
    // executes on all renderers) be conservative and only crash in the renderer
    // of the extension which failed to load; this one.
    std::string& error = extension_load_errors_[extension_id];
    char minidump[256];
    base::debug::Alias(&minidump);
    base::snprintf(minidump,
                   arraysize(minidump),
                   "e::dispatcher:%s:%s",
                   extension_id.c_str(),
                   error.c_str());
    CHECK(extension) << extension_id << " was never loaded: " << error;
  }

  active_extension_ids_.insert(extension_id);

  // This is called when starting a new extension page, so start the idle
  // handler ticking.
  RenderThread::Get()->ScheduleIdleHandler(kInitialExtensionIdleHandlerDelayMs);

  if (is_webkit_initialized_) {
    extensions::DOMActivityLogger::AttachToWorld(
        extensions::DOMActivityLogger::kMainWorldId, extension_id);

    InitOriginPermissions(extension);
  }

  UpdateActiveExtensions();
}

void Dispatcher::OnCancelSuspend(const std::string& extension_id) {
  DispatchEvent(extension_id, kOnSuspendCanceledEvent);
}

void Dispatcher::OnClearTabSpecificPermissions(
    int tab_id,
    const std::vector<std::string>& extension_ids) {
  delegate_->ClearTabSpecificPermissions(this, tab_id, extension_ids);
}

void Dispatcher::OnDeliverMessage(int target_port_id, const Message& message) {
  scoped_ptr<RequestSender::ScopedTabID> scoped_tab_id;
  std::map<int, int>::const_iterator it =
      port_to_tab_id_map_.find(target_port_id);
  if (it != port_to_tab_id_map_.end()) {
    scoped_tab_id.reset(
        new RequestSender::ScopedTabID(request_sender(), it->second));
  }

  MessagingBindings::DeliverMessage(script_context_set_,
                                    target_port_id,
                                    message,
                                    NULL);  // All render views.
}

void Dispatcher::OnDispatchOnConnect(
    int target_port_id,
    const std::string& channel_name,
    const base::DictionaryValue& source_tab,
    const ExtensionMsg_ExternalConnectionInfo& info,
    const std::string& tls_channel_id) {
  DCHECK(!ContainsKey(port_to_tab_id_map_, target_port_id));
  DCHECK_EQ(1, target_port_id % 2);  // target renderer ports have odd IDs.
  int sender_tab_id = -1;
  source_tab.GetInteger("id", &sender_tab_id);
  port_to_tab_id_map_[target_port_id] = sender_tab_id;

  MessagingBindings::DispatchOnConnect(script_context_set_,
                                       target_port_id,
                                       channel_name,
                                       source_tab,
                                       info,
                                       tls_channel_id,
                                       NULL);  // All render views.
}

void Dispatcher::OnDispatchOnDisconnect(int port_id,
                                        const std::string& error_message) {
  MessagingBindings::DispatchOnDisconnect(script_context_set_,
                                          port_id,
                                          error_message,
                                          NULL);  // All render views.
}

void Dispatcher::OnLoaded(
    const std::vector<ExtensionMsg_Loaded_Params>& loaded_extensions) {
  std::vector<ExtensionMsg_Loaded_Params>::const_iterator i;
  for (i = loaded_extensions.begin(); i != loaded_extensions.end(); ++i) {
    std::string error;
    scoped_refptr<const Extension> extension = i->ConvertToExtension(&error);
    if (!extension.get()) {
      extension_load_errors_[i->id] = error;
      continue;
    }
    OnLoadedInternal(extension);
  }
  // Update the available bindings for all contexts. These may have changed if
  // an externally_connectable extension was loaded that can connect to an
  // open webpage.
  UpdateBindings("");
}

void Dispatcher::OnLoadedInternal(scoped_refptr<const Extension> extension) {
  extensions_.Insert(extension);
}

void Dispatcher::OnMessageInvoke(const std::string& extension_id,
                                 const std::string& module_name,
                                 const std::string& function_name,
                                 const base::ListValue& args,
                                 bool user_gesture) {
  InvokeModuleSystemMethod(
      NULL, extension_id, module_name, function_name, args, user_gesture);
}

void Dispatcher::OnSetChannel(int channel) {
  delegate_->SetChannel(channel);
}

void Dispatcher::OnSetFunctionNames(const std::vector<std::string>& names) {
  function_names_.clear();
  for (size_t i = 0; i < names.size(); ++i)
    function_names_.insert(names[i]);
}

void Dispatcher::OnSetScriptingWhitelist(
    const ExtensionsClient::ScriptingWhitelist& extension_ids) {
  ExtensionsClient::Get()->SetScriptingWhitelist(extension_ids);
}

void Dispatcher::OnSetSystemFont(const std::string& font_family,
                                 const std::string& font_size) {
  system_font_family_ = font_family;
  system_font_size_ = font_size;
}

void Dispatcher::OnShouldSuspend(const std::string& extension_id,
                                 uint64 sequence_id) {
  RenderThread::Get()->Send(
      new ExtensionHostMsg_ShouldSuspendAck(extension_id, sequence_id));
}

void Dispatcher::OnSuspend(const std::string& extension_id) {
  // Dispatch the suspend event. This doesn't go through the standard event
  // dispatch machinery because it requires special handling. We need to let
  // the browser know when we are starting and stopping the event dispatch, so
  // that it still considers the extension idle despite any activity the suspend
  // event creates.
  DispatchEvent(extension_id, kOnSuspendEvent);
  RenderThread::Get()->Send(new ExtensionHostMsg_SuspendAck(extension_id));
}

void Dispatcher::OnTransferBlobs(const std::vector<std::string>& blob_uuids) {
  RenderThread::Get()->Send(new ExtensionHostMsg_TransferBlobsAck(blob_uuids));
}

void Dispatcher::OnUnloaded(const std::string& id) {
  extensions_.Remove(id);
  active_extension_ids_.erase(id);

  // If the extension is later reloaded with a different set of permissions,
  // we'd like it to get a new isolated world ID, so that it can pick up the
  // changed origin whitelist.
  ScriptInjection::RemoveIsolatedWorld(id);

  // Invalidate all of the contexts that were removed.
  // TODO(kalman): add an invalidation observer interface to ScriptContext.
  ScriptContextSet::ContextSet removed_contexts =
      script_context_set_.OnExtensionUnloaded(id);
  for (ScriptContextSet::ContextSet::iterator it = removed_contexts.begin();
       it != removed_contexts.end();
       ++it) {
    request_sender_->InvalidateSource(*it);
  }

  // Update the available bindings for the remaining contexts. These may have
  // changed if an externally_connectable extension is unloaded and a webpage
  // is no longer accessible.
  UpdateBindings("");

  // Invalidates the messages map for the extension in case the extension is
  // reloaded with a new messages map.
  EraseL10nMessagesMap(id);

  // We don't do anything with existing platform-app stylesheets. They will
  // stay resident, but the URL pattern corresponding to the unloaded
  // extension's URL just won't match anything anymore.
}

void Dispatcher::OnUpdatePermissions(
    const ExtensionMsg_UpdatePermissions_Params& params) {
  const Extension* extension = extensions_.GetByID(params.extension_id);
  if (!extension)
    return;

  scoped_refptr<const PermissionSet> active =
      params.active_permissions.ToPermissionSet();
  scoped_refptr<const PermissionSet> withheld =
      params.withheld_permissions.ToPermissionSet();

  if (is_webkit_initialized_) {
    UpdateOriginPermissions(
        extension,
        extension->permissions_data()->GetEffectiveHostPermissions(),
        active->effective_hosts());
  }

  extension->permissions_data()->SetPermissions(active, withheld);
  UpdateBindings(extension->id());
}

void Dispatcher::OnUpdateTabSpecificPermissions(
    const GURL& url,
    int tab_id,
    const std::string& extension_id,
    const URLPatternSet& origin_set) {
  delegate_->UpdateTabSpecificPermissions(
      this, url, tab_id, extension_id, origin_set);
}

void Dispatcher::OnUsingWebRequestAPI(bool webrequest_used) {
  delegate_->HandleWebRequestAPIUsage(webrequest_used);
}

void Dispatcher::OnUserScriptsUpdated(
      const std::set<std::string>& changed_extensions,
      const std::vector<UserScript*>& scripts) {
  UpdateActiveExtensions();
}

void Dispatcher::UpdateActiveExtensions() {
  std::set<std::string> active_extensions = active_extension_ids_;
  user_script_set_manager_->GetAllActiveExtensionIds(&active_extensions);
  delegate_->OnActiveExtensionsUpdated(active_extensions);
}

void Dispatcher::InitOriginPermissions(const Extension* extension) {
  delegate_->InitOriginPermissions(extension,
                                   IsExtensionActive(extension->id()));
  UpdateOriginPermissions(
      extension,
      URLPatternSet(),  // No old permissions.
      extension->permissions_data()->GetEffectiveHostPermissions());
}

void Dispatcher::UpdateOriginPermissions(
    const Extension* extension,
    const URLPatternSet& old_patterns,
    const URLPatternSet& new_patterns) {
  static const char* kSchemes[] = {
      url::kHttpScheme,
      url::kHttpsScheme,
      url::kFileScheme,
      content::kChromeUIScheme,
      url::kFtpScheme,
  };
  for (size_t i = 0; i < arraysize(kSchemes); ++i) {
    const char* scheme = kSchemes[i];
    // Remove all old patterns...
    for (URLPatternSet::const_iterator pattern = old_patterns.begin();
         pattern != old_patterns.end(); ++pattern) {
      if (pattern->MatchesScheme(scheme)) {
        WebSecurityPolicy::removeOriginAccessWhitelistEntry(
            extension->url(),
            WebString::fromUTF8(scheme),
            WebString::fromUTF8(pattern->host()),
            pattern->match_subdomains());
      }
    }
    // ...And add the new ones.
    for (URLPatternSet::const_iterator pattern = new_patterns.begin();
         pattern != new_patterns.end(); ++pattern) {
      if (pattern->MatchesScheme(scheme)) {
        WebSecurityPolicy::addOriginAccessWhitelistEntry(
            extension->url(),
            WebString::fromUTF8(scheme),
            WebString::fromUTF8(pattern->host()),
            pattern->match_subdomains());
      }
    }
  }
}

void Dispatcher::EnableCustomElementWhiteList() {
  blink::WebCustomElement::addEmbedderCustomElementName("appplugin");
  blink::WebCustomElement::addEmbedderCustomElementName("appview");
  blink::WebCustomElement::addEmbedderCustomElementName("browserplugin");
  blink::WebCustomElement::addEmbedderCustomElementName("extensionoptions");
  blink::WebCustomElement::addEmbedderCustomElementName(
      "extensionoptionsplugin");
  blink::WebCustomElement::addEmbedderCustomElementName("webview");
}

void Dispatcher::UpdateBindings(const std::string& extension_id) {
  script_context_set().ForEach(extension_id,
                               base::Bind(&Dispatcher::UpdateBindingsForContext,
                                          base::Unretained(this)));
}

void Dispatcher::UpdateBindingsForContext(ScriptContext* context) {
  v8::HandleScope handle_scope(context->isolate());
  v8::Context::Scope context_scope(context->v8_context());

  // TODO(kalman): Make the bindings registration have zero overhead then run
  // the same code regardless of context type.
  switch (context->context_type()) {
    case Feature::UNSPECIFIED_CONTEXT:
    case Feature::WEB_PAGE_CONTEXT:
    case Feature::BLESSED_WEB_PAGE_CONTEXT: {
      // Web page context; it's too expensive to run the full bindings code.
      // Hard-code that the app and webstore APIs are available...
      if (context->GetAvailability("app").is_available())
        RegisterBinding("app", context);

      if (context->GetAvailability("webstore").is_available())
        RegisterBinding("webstore", context);

      // ... and that the runtime API might be available if any extension can
      // connect to it.
      bool runtime_is_available = false;
      for (ExtensionSet::const_iterator it = extensions_.begin();
           it != extensions_.end();
           ++it) {
        ExternallyConnectableInfo* info =
            static_cast<ExternallyConnectableInfo*>(
                (*it)->GetManifestData(manifest_keys::kExternallyConnectable));
        if (info && info->matches.MatchesURL(context->GetURL())) {
          runtime_is_available = true;
          break;
        }
      }
      if (runtime_is_available)
        RegisterBinding("runtime", context);
      break;
    }

    case Feature::BLESSED_EXTENSION_CONTEXT:
    case Feature::UNBLESSED_EXTENSION_CONTEXT:
    case Feature::CONTENT_SCRIPT_CONTEXT:
    case Feature::WEBUI_CONTEXT: {
      // Extension context; iterate through all the APIs and bind the available
      // ones.
      const FeatureProvider* api_feature_provider =
          FeatureProvider::GetAPIFeatures();
      const std::vector<std::string>& apis =
          api_feature_provider->GetAllFeatureNames();
      for (std::vector<std::string>::const_iterator it = apis.begin();
           it != apis.end();
           ++it) {
        const std::string& api_name = *it;
        Feature* feature = api_feature_provider->GetFeature(api_name);
        DCHECK(feature);

        // Internal APIs are included via require(api_name) from internal code
        // rather than chrome[api_name].
        if (feature->IsInternal())
          continue;

        // If this API has a parent feature (and isn't marked 'noparent'),
        // then this must be a function or event, so we should not register.
        if (api_feature_provider->GetParent(feature) != NULL)
          continue;

        if (context->IsAnyFeatureAvailableToContext(*feature))
          RegisterBinding(api_name, context);
      }
      if (CommandLine::ForCurrentProcess()->HasSwitch(::switches::kTestType)) {
        RegisterBinding("test", context);
      }
      break;
    }
  }
}

void Dispatcher::RegisterBinding(const std::string& api_name,
                                 ScriptContext* context) {
  std::string bind_name;
  v8::Handle<v8::Object> bind_object =
      GetOrCreateBindObjectIfAvailable(api_name, &bind_name, context);

  // Empty if the bind object failed to be created, probably because the
  // extension overrode chrome with a non-object, e.g. window.chrome = true.
  if (bind_object.IsEmpty())
    return;

  v8::Local<v8::String> v8_api_name =
      v8::String::NewFromUtf8(context->isolate(), api_name.c_str());
  if (bind_object->HasRealNamedProperty(v8_api_name)) {
    // The bind object may already have the property if the API has been
    // registered before (or if the extension has put something there already,
    // but, whatevs).
    //
    // In the former case, we need to re-register the bindings for the APIs
    // which the extension now has permissions for (if any), but not touch any
    // others so that we don't destroy state such as event listeners.
    //
    // TODO(kalman): Only register available APIs to make this all moot.
    if (bind_object->HasRealNamedCallbackProperty(v8_api_name))
      return;  // lazy binding still there, nothing to do
    if (bind_object->Get(v8_api_name)->IsObject())
      return;  // binding has already been fully installed
  }

  ModuleSystem* module_system = context->module_system();
  if (!source_map_.Contains(api_name)) {
    module_system->RegisterNativeHandler(
        api_name,
        scoped_ptr<NativeHandler>(new BindingGeneratingNativeHandler(
            module_system, api_name, "binding")));
    module_system->SetNativeLazyField(
        bind_object, bind_name, api_name, "binding");
  } else {
    module_system->SetLazyField(bind_object, bind_name, api_name, "binding");
  }
}

// NOTE: please use the naming convention "foo_natives" for these.
void Dispatcher::RegisterNativeHandlers(ModuleSystem* module_system,
                                        ScriptContext* context) {
  RegisterNativeHandlers(module_system,
                         context,
                         this,
                         request_sender_.get(),
                         v8_schema_registry_.get());
  const Extension* extension = context->extension();
  int manifest_version = extension ? extension->manifest_version() : 1;
  bool send_request_disabled =
      (extension && Manifest::IsUnpackedLocation(extension->location()) &&
       BackgroundInfo::HasLazyBackgroundPage(extension));
  module_system->RegisterNativeHandler(
      "process",
      scoped_ptr<NativeHandler>(new ProcessInfoNativeHandler(
          context,
          context->GetExtensionID(),
          context->GetContextTypeDescription(),
          ExtensionsRendererClient::Get()->IsIncognitoProcess(),
          manifest_version,
          send_request_disabled)));

  delegate_->RegisterNativeHandlers(this, module_system, context);
}

void Dispatcher::PopulateSourceMap() {
  const std::vector<std::pair<std::string, int> > resources = GetJsResources();
  for (std::vector<std::pair<std::string, int> >::const_iterator resource =
           resources.begin();
       resource != resources.end();
       ++resource) {
    source_map_.RegisterSource(resource->first, resource->second);
  }
  delegate_->PopulateSourceMap(&source_map_);
}

bool Dispatcher::IsWithinPlatformApp() {
  for (std::set<std::string>::iterator iter = active_extension_ids_.begin();
       iter != active_extension_ids_.end();
       ++iter) {
    const Extension* extension = extensions_.GetByID(*iter);
    if (extension && extension->is_platform_app())
      return true;
  }
  return false;
}

// TODO(kalman): This is checking for the wrong thing, it should be checking if
// the frame's security origin is unique. The extension sandbox directive is
// checked for in extensions/common/manifest_handlers/csp_info.cc.
bool Dispatcher::IsSandboxedPage(const GURL& url) const {
  if (url.SchemeIs(kExtensionScheme)) {
    const Extension* extension = extensions_.GetByID(url.host());
    if (extension) {
      return SandboxedPageInfo::IsSandboxedPage(extension, url.path());
    }
  }
  return false;
}

Feature::Context Dispatcher::ClassifyJavaScriptContext(
    const Extension* extension,
    int extension_group,
    const GURL& url,
    const blink::WebSecurityOrigin& origin) {
  // WARNING: This logic must match ProcessMap::GuessContextType, as much as
  // possible.

  DCHECK_GE(extension_group, 0);
  if (extension_group == EXTENSION_GROUP_CONTENT_SCRIPTS) {
    return extension ?  // TODO(kalman): when does this happen?
               Feature::CONTENT_SCRIPT_CONTEXT
                     : Feature::UNSPECIFIED_CONTEXT;
  }

  // We have an explicit check for sandboxed pages before checking whether the
  // extension is active in this process because:
  // 1. Sandboxed pages run in the same process as regular extension pages, so
  //    the extension is considered active.
  // 2. ScriptContext creation (which triggers bindings injection) happens
  //    before the SecurityContext is updated with the sandbox flags (after
  //    reading the CSP header), so the caller can't check if the context's
  //    security origin is unique yet.
  if (IsSandboxedPage(url))
    return Feature::WEB_PAGE_CONTEXT;

  if (extension && IsExtensionActive(extension->id())) {
    // |extension| is active in this process, but it could be either a true
    // extension process or within the extent of a hosted app. In the latter
    // case this would usually be considered a (blessed) web page context,
    // unless the extension in question is a component extension, in which case
    // we cheat and call it blessed.
    return (extension->is_hosted_app() &&
            extension->location() != Manifest::COMPONENT)
               ? Feature::BLESSED_WEB_PAGE_CONTEXT
               : Feature::BLESSED_EXTENSION_CONTEXT;
  }

  // TODO(kalman): This isUnique() check is wrong, it should be performed as
  // part of IsSandboxedPage().
  if (!origin.isUnique() && extensions_.ExtensionBindingsAllowed(url)) {
    if (!extension)  // TODO(kalman): when does this happen?
      return Feature::UNSPECIFIED_CONTEXT;
    return extension->is_hosted_app() ? Feature::BLESSED_WEB_PAGE_CONTEXT
                                      : Feature::UNBLESSED_EXTENSION_CONTEXT;
  }

  if (!url.is_valid())
    return Feature::UNSPECIFIED_CONTEXT;

  if (url.SchemeIs(content::kChromeUIScheme))
    return Feature::WEBUI_CONTEXT;

  return Feature::WEB_PAGE_CONTEXT;
}

v8::Handle<v8::Object> Dispatcher::GetOrCreateObject(
    const v8::Handle<v8::Object>& object,
    const std::string& field,
    v8::Isolate* isolate) {
  v8::Handle<v8::String> key = v8::String::NewFromUtf8(isolate, field.c_str());
  // If the object has a callback property, it is assumed it is an unavailable
  // API, so it is safe to delete. This is checked before GetOrCreateObject is
  // called.
  if (object->HasRealNamedCallbackProperty(key)) {
    object->Delete(key);
  } else if (object->HasRealNamedProperty(key)) {
    v8::Handle<v8::Value> value = object->Get(key);
    CHECK(value->IsObject());
    return v8::Handle<v8::Object>::Cast(value);
  }

  v8::Handle<v8::Object> new_object = v8::Object::New(isolate);
  object->Set(key, new_object);
  return new_object;
}

v8::Handle<v8::Object> Dispatcher::GetOrCreateBindObjectIfAvailable(
    const std::string& api_name,
    std::string* bind_name,
    ScriptContext* context) {
  std::vector<std::string> split;
  base::SplitString(api_name, '.', &split);

  v8::Handle<v8::Object> bind_object;

  // Check if this API has an ancestor. If the API's ancestor is available and
  // the API is not available, don't install the bindings for this API. If
  // the API is available and its ancestor is not, delete the ancestor and
  // install the bindings for the API. This is to prevent loading the ancestor
  // API schema if it will not be needed.
  //
  // For example:
  //  If app is available and app.window is not, just install app.
  //  If app.window is available and app is not, delete app and install
  //  app.window on a new object so app does not have to be loaded.
  const FeatureProvider* api_feature_provider =
      FeatureProvider::GetAPIFeatures();
  std::string ancestor_name;
  bool only_ancestor_available = false;

  for (size_t i = 0; i < split.size() - 1; ++i) {
    ancestor_name += (i ? "." : "") + split[i];
    if (api_feature_provider->GetFeature(ancestor_name) &&
        context->GetAvailability(ancestor_name).is_available() &&
        !context->GetAvailability(api_name).is_available()) {
      only_ancestor_available = true;
      break;
    }

    if (bind_object.IsEmpty()) {
      bind_object = AsObjectOrEmpty(GetOrCreateChrome(context));
      if (bind_object.IsEmpty())
        return v8::Handle<v8::Object>();
    }
    bind_object = GetOrCreateObject(bind_object, split[i], context->isolate());
  }

  if (only_ancestor_available)
    return v8::Handle<v8::Object>();

  if (bind_name)
    *bind_name = split.back();

  return bind_object.IsEmpty() ? AsObjectOrEmpty(GetOrCreateChrome(context))
                               : bind_object;
}

}  // namespace extensions
