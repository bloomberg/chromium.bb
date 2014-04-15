// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/dispatcher.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/user_metrics_action.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "chrome/common/extensions/manifest_handlers/externally_connectable.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/chrome_render_process_observer.h"
#include "chrome/renderer/extensions/api_activity_logger.h"
#include "chrome/renderer/extensions/api_definitions_natives.h"
#include "chrome/renderer/extensions/app_bindings.h"
#include "chrome/renderer/extensions/app_runtime_custom_bindings.h"
#include "chrome/renderer/extensions/app_window_custom_bindings.h"
#include "chrome/renderer/extensions/binding_generating_native_handler.h"
#include "chrome/renderer/extensions/blob_native_handler.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "chrome/renderer/extensions/chrome_v8_extension.h"
#include "chrome/renderer/extensions/content_watcher.h"
#include "chrome/renderer/extensions/context_menus_custom_bindings.h"
#include "chrome/renderer/extensions/css_native_handler.h"
#include "chrome/renderer/extensions/document_custom_bindings.h"
#include "chrome/renderer/extensions/dom_activity_logger.h"
#include "chrome/renderer/extensions/extension_groups.h"
#include "chrome/renderer/extensions/extension_helper.h"
#include "chrome/renderer/extensions/file_browser_handler_custom_bindings.h"
#include "chrome/renderer/extensions/file_browser_private_custom_bindings.h"
#include "chrome/renderer/extensions/file_system_natives.h"
#include "chrome/renderer/extensions/i18n_custom_bindings.h"
#include "chrome/renderer/extensions/id_generator_custom_bindings.h"
#include "chrome/renderer/extensions/logging_native_handler.h"
#include "chrome/renderer/extensions/media_galleries_custom_bindings.h"
#include "chrome/renderer/extensions/messaging_bindings.h"
#include "chrome/renderer/extensions/page_actions_custom_bindings.h"
#include "chrome/renderer/extensions/page_capture_custom_bindings.h"
#include "chrome/renderer/extensions/pepper_request_natives.h"
#include "chrome/renderer/extensions/render_view_observer_natives.h"
#include "chrome/renderer/extensions/runtime_custom_bindings.h"
#include "chrome/renderer/extensions/send_request_natives.h"
#include "chrome/renderer/extensions/set_icon_natives.h"
#include "chrome/renderer/extensions/sync_file_system_custom_bindings.h"
#include "chrome/renderer/extensions/tab_finder.h"
#include "chrome/renderer/extensions/tabs_custom_bindings.h"
#include "chrome/renderer/extensions/user_script_slave.h"
#include "chrome/renderer/extensions/utils_native_handler.h"
#include "chrome/renderer/extensions/webstore_bindings.h"
#include "chrome/renderer/resource_bundle_source_map.h"
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
#include "extensions/common/manifest_handlers/sandboxed_page_info.h"
#include "extensions/common/message_bundle.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"
#include "extensions/common/view_type.h"
#include "extensions/renderer/event_bindings.h"
#include "extensions/renderer/module_system.h"
#include "extensions/renderer/object_backed_native_handler.h"
#include "extensions/renderer/request_sender.h"
#include "extensions/renderer/safe_builtins.h"
#include "extensions/renderer/script_context.h"
#include "grit/common_resources.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebCustomElement.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/web/WebScopedUserGesture.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "v8/include/v8.h"

#if defined(ENABLE_WEBRTC)
#include "chrome/renderer/extensions/cast_streaming_native_handler.h"
#endif

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

static const int64 kInitialExtensionIdleHandlerDelayMs = 5*1000;
static const int64 kMaxExtensionIdleHandlerDelayMs = 5*60*1000;
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

class TestFeaturesNativeHandler : public ObjectBackedNativeHandler {
 public:
  explicit TestFeaturesNativeHandler(ChromeV8Context* context)
      : ObjectBackedNativeHandler(context) {
    RouteFunction("GetAPIFeatures",
        base::Bind(&TestFeaturesNativeHandler::GetAPIFeatures,
                   base::Unretained(this)));
  }

 private:
  void GetAPIFeatures(const v8::FunctionCallbackInfo<v8::Value>& args) {
    base::Value* value = base::JSONReader::Read(
        ResourceBundle::GetSharedInstance().GetRawDataResource(
            IDR_EXTENSION_API_FEATURES).as_string());
    scoped_ptr<content::V8ValueConverter> converter(
        content::V8ValueConverter::create());
    args.GetReturnValue().Set(
        converter->ToV8Value(value, context()->v8_context()));
  }
};

class UserGesturesNativeHandler : public ObjectBackedNativeHandler {
 public:
  explicit UserGesturesNativeHandler(ChromeV8Context* context)
      : ObjectBackedNativeHandler(context) {
    RouteFunction("IsProcessingUserGesture",
        base::Bind(&UserGesturesNativeHandler::IsProcessingUserGesture,
                   base::Unretained(this)));
    RouteFunction("RunWithUserGesture",
        base::Bind(&UserGesturesNativeHandler::RunWithUserGesture,
                   base::Unretained(this)));
    RouteFunction("RunWithoutUserGesture",
        base::Bind(&UserGesturesNativeHandler::RunWithoutUserGesture,
                   base::Unretained(this)));
  }

 private:
  void IsProcessingUserGesture(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    args.GetReturnValue().Set(v8::Boolean::New(
        args.GetIsolate(),
        blink::WebUserGestureIndicator::isProcessingUserGesture()));
  }

  void RunWithUserGesture(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    blink::WebScopedUserGesture user_gesture;
    CHECK_EQ(args.Length(), 1);
    CHECK(args[0]->IsFunction());
    v8::Handle<v8::Value> no_args;
    context()->CallFunction(v8::Handle<v8::Function>::Cast(args[0]),
                            0, &no_args);
  }

  void RunWithoutUserGesture(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    blink::WebUserGestureIndicator::consumeUserGesture();
    CHECK_EQ(args.Length(), 1);
    CHECK(args[0]->IsFunction());
    v8::Handle<v8::Value> no_args;
    context()->CallFunction(v8::Handle<v8::Function>::Cast(args[0]),
                            0, &no_args);
  }
};

class V8ContextNativeHandler : public ObjectBackedNativeHandler {
 public:
  V8ContextNativeHandler(ChromeV8Context* context, Dispatcher* dispatcher)
      : ObjectBackedNativeHandler(context),
        context_(context),
        dispatcher_(dispatcher) {
    RouteFunction("GetAvailability",
        base::Bind(&V8ContextNativeHandler::GetAvailability,
                   base::Unretained(this)));
    RouteFunction("GetModuleSystem",
        base::Bind(&V8ContextNativeHandler::GetModuleSystem,
                   base::Unretained(this)));
  }

 private:
  void GetAvailability(const v8::FunctionCallbackInfo<v8::Value>& args) {
    CHECK_EQ(args.Length(), 1);
    v8::Isolate* isolate = args.GetIsolate();
    std::string api_name = *v8::String::Utf8Value(args[0]->ToString());
    Feature::Availability availability = context_->GetAvailability(api_name);

    v8::Handle<v8::Object> ret = v8::Object::New(isolate);
    ret->Set(v8::String::NewFromUtf8(isolate, "is_available"),
             v8::Boolean::New(isolate, availability.is_available()));
    ret->Set(v8::String::NewFromUtf8(isolate, "message"),
             v8::String::NewFromUtf8(isolate, availability.message().c_str()));
    ret->Set(v8::String::NewFromUtf8(isolate, "result"),
             v8::Integer::New(isolate, availability.result()));
    args.GetReturnValue().Set(ret);
  }

  void GetModuleSystem(const v8::FunctionCallbackInfo<v8::Value>& args) {
    CHECK_EQ(args.Length(), 1);
    CHECK(args[0]->IsObject());
    v8::Handle<v8::Context> v8_context =
        v8::Handle<v8::Object>::Cast(args[0])->CreationContext();
    ChromeV8Context* context = dispatcher_->v8_context_set().GetByV8Context(
        v8_context);
    args.GetReturnValue().Set(context->module_system()->NewInstance());
  }

  ChromeV8Context* context_;
  Dispatcher* dispatcher_;
};

class ChromeNativeHandler : public ObjectBackedNativeHandler {
 public:
  explicit ChromeNativeHandler(ChromeV8Context* context)
      : ObjectBackedNativeHandler(context) {
    RouteFunction("GetChrome",
        base::Bind(&ChromeNativeHandler::GetChrome, base::Unretained(this)));
  }

  void GetChrome(const v8::FunctionCallbackInfo<v8::Value>& args) {
    args.GetReturnValue().Set(GetOrCreateChrome(context()));
  }
};

class PrintNativeHandler : public ObjectBackedNativeHandler {
 public:
  explicit PrintNativeHandler(ChromeV8Context* context)
      : ObjectBackedNativeHandler(context) {
    RouteFunction("Print",
        base::Bind(&PrintNativeHandler::Print,
                   base::Unretained(this)));
  }

  void Print(const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() < 1)
      return;

    std::vector<std::string> components;
    for (int i = 0; i < args.Length(); ++i)
      components.push_back(*v8::String::Utf8Value(args[i]->ToString()));

    LOG(ERROR) << JoinString(components, ',');
  }
};

class LazyBackgroundPageNativeHandler : public ChromeV8Extension {
 public:
  LazyBackgroundPageNativeHandler(Dispatcher* dispatcher,
                                  ChromeV8Context* context)
      : ChromeV8Extension(dispatcher, context) {
    RouteFunction("IncrementKeepaliveCount",
        base::Bind(&LazyBackgroundPageNativeHandler::IncrementKeepaliveCount,
                   base::Unretained(this)));
    RouteFunction("DecrementKeepaliveCount",
        base::Bind(&LazyBackgroundPageNativeHandler::DecrementKeepaliveCount,
                   base::Unretained(this)));
  }

  void IncrementKeepaliveCount(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (!context())
      return;
    RenderView* render_view = context()->GetRenderView();
    if (IsContextLazyBackgroundPage(render_view, context()->extension())) {
      render_view->Send(new ExtensionHostMsg_IncrementLazyKeepaliveCount(
          render_view->GetRoutingID()));
    }
  }

  void DecrementKeepaliveCount(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (!context())
      return;
    RenderView* render_view = context()->GetRenderView();
    if (IsContextLazyBackgroundPage(render_view, context()->extension())) {
      render_view->Send(new ExtensionHostMsg_DecrementLazyKeepaliveCount(
          render_view->GetRoutingID()));
    }
  }

 private:
  bool IsContextLazyBackgroundPage(RenderView* render_view,
                                   const Extension* extension) {
    if (!render_view)
      return false;

    ExtensionHelper* helper = ExtensionHelper::Get(render_view);
    return (extension && BackgroundInfo::HasLazyBackgroundPage(extension) &&
            helper->view_type() == VIEW_TYPE_EXTENSION_BACKGROUND_PAGE);
  }
};

class ProcessInfoNativeHandler : public ChromeV8Extension {
 public:
  ProcessInfoNativeHandler(Dispatcher* dispatcher,
                           ChromeV8Context* context,
                           const std::string& extension_id,
                           const std::string& context_type,
                           bool is_incognito_context,
                           int manifest_version,
                           bool send_request_disabled)
      : ChromeV8Extension(dispatcher, context),
        extension_id_(extension_id),
        context_type_(context_type),
        is_incognito_context_(is_incognito_context),
        manifest_version_(manifest_version),
        send_request_disabled_(send_request_disabled) {
    RouteFunction("GetExtensionId",
        base::Bind(&ProcessInfoNativeHandler::GetExtensionId,
                   base::Unretained(this)));
    RouteFunction("GetContextType",
        base::Bind(&ProcessInfoNativeHandler::GetContextType,
                   base::Unretained(this)));
    RouteFunction("InIncognitoContext",
        base::Bind(&ProcessInfoNativeHandler::InIncognitoContext,
                   base::Unretained(this)));
    RouteFunction("GetManifestVersion",
        base::Bind(&ProcessInfoNativeHandler::GetManifestVersion,
                   base::Unretained(this)));
    RouteFunction("IsSendRequestDisabled",
        base::Bind(&ProcessInfoNativeHandler::IsSendRequestDisabled,
                   base::Unretained(this)));
    RouteFunction("HasSwitch",
        base::Bind(&ProcessInfoNativeHandler::HasSwitch,
                   base::Unretained(this)));
  }

 private:
  void GetExtensionId(const v8::FunctionCallbackInfo<v8::Value>& args) {
    args.GetReturnValue()
        .Set(v8::String::NewFromUtf8(args.GetIsolate(), extension_id_.c_str()));
  }

  void GetContextType(const v8::FunctionCallbackInfo<v8::Value>& args) {
    args.GetReturnValue()
        .Set(v8::String::NewFromUtf8(args.GetIsolate(), context_type_.c_str()));
  }

  void InIncognitoContext(const v8::FunctionCallbackInfo<v8::Value>& args) {
    args.GetReturnValue().Set(is_incognito_context_);
  }

  void GetManifestVersion(const v8::FunctionCallbackInfo<v8::Value>& args) {
    args.GetReturnValue().Set(static_cast<int32_t>(manifest_version_));
  }

  void IsSendRequestDisabled(const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (send_request_disabled_) {
      args.GetReturnValue().Set(v8::String::NewFromUtf8(args.GetIsolate(),
          "sendRequest and onRequest are obsolete."
          " Please use sendMessage and onMessage instead."));
    }
  }

  void HasSwitch(const v8::FunctionCallbackInfo<v8::Value>& args) {
    CHECK(args.Length() == 1 && args[0]->IsString());
    bool has_switch = CommandLine::ForCurrentProcess()->HasSwitch(
        *v8::String::Utf8Value(args[0]));
    args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), has_switch));
  }

  std::string extension_id_;
  std::string context_type_;
  bool is_incognito_context_;
  int manifest_version_;
  bool send_request_disabled_;
};

void InstallAppBindings(ModuleSystem* module_system,
                        v8::Handle<v8::Object> chrome) {
  module_system->SetLazyField(chrome, "app", "app", "chromeApp");
}

void InstallWebstoreBindings(ModuleSystem* module_system,
                             v8::Handle<v8::Object> chrome) {
  module_system->SetLazyField(chrome, "webstore", "webstore", "chromeWebstore");
}

// Calls a method |method_name| in a module |module_name| belonging to the
// module system from |context|. Intended as a callback target from
// ChromeV8ContextSet::ForEach.
void CallModuleMethod(const std::string& module_name,
                      const std::string& method_name,
                      const base::ListValue* args,
                      ChromeV8Context* context) {
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

}  // namespace

Dispatcher::Dispatcher()
    : content_watcher_(new ContentWatcher()),
      is_webkit_initialized_(false),
      webrequest_adblock_(false),
      webrequest_adblock_plus_(false),
      webrequest_other_(false),
      source_map_(&ResourceBundle::GetSharedInstance()),
      v8_schema_registry_(new V8SchemaRegistry) {
  const CommandLine& command_line = *(CommandLine::ForCurrentProcess());
  is_extension_process_ =
      command_line.HasSwitch(extensions::switches::kExtensionProcess) ||
      command_line.HasSwitch(::switches::kSingleProcess);

  if (is_extension_process_) {
    RenderThread::Get()->SetIdleNotificationDelayInMs(
        kInitialExtensionIdleHandlerDelayMs);
  }

  RenderThread::Get()->RegisterExtension(SafeBuiltins::CreateV8Extension());

  user_script_slave_.reset(new UserScriptSlave(&extensions_));
  request_sender_.reset(new RequestSender(this));
  PopulateSourceMap();
  PopulateLazyBindingsMap();
}

Dispatcher::~Dispatcher() {
}

bool Dispatcher::OnControlMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(Dispatcher, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_SetChannel, OnSetChannel)
    IPC_MESSAGE_HANDLER(ExtensionMsg_MessageInvoke, OnMessageInvoke)
    IPC_MESSAGE_HANDLER(ExtensionMsg_DispatchOnConnect, OnDispatchOnConnect)
    IPC_MESSAGE_HANDLER(ExtensionMsg_DeliverMessage, OnDeliverMessage)
    IPC_MESSAGE_HANDLER(ExtensionMsg_DispatchOnDisconnect,
                        OnDispatchOnDisconnect)
    IPC_MESSAGE_HANDLER(ExtensionMsg_SetFunctionNames, OnSetFunctionNames)
    IPC_MESSAGE_HANDLER(ExtensionMsg_SetSystemFont, OnSetSystemFont)
    IPC_MESSAGE_HANDLER(ExtensionMsg_Loaded, OnLoaded)
    IPC_MESSAGE_HANDLER(ExtensionMsg_Unloaded, OnUnloaded)
    IPC_MESSAGE_HANDLER(ExtensionMsg_SetScriptingWhitelist,
                        OnSetScriptingWhitelist)
    IPC_MESSAGE_HANDLER(ExtensionMsg_ActivateExtension, OnActivateExtension)
    IPC_MESSAGE_HANDLER(ExtensionMsg_UpdatePermissions, OnUpdatePermissions)
    IPC_MESSAGE_HANDLER(ExtensionMsg_UpdateTabSpecificPermissions,
                        OnUpdateTabSpecificPermissions)
    IPC_MESSAGE_HANDLER(ExtensionMsg_ClearTabSpecificPermissions,
                        OnClearTabSpecificPermissions)
    IPC_MESSAGE_HANDLER(ExtensionMsg_UpdateUserScripts, OnUpdateUserScripts)
    IPC_MESSAGE_HANDLER(ExtensionMsg_UsingWebRequestAPI, OnUsingWebRequestAPI)
    IPC_MESSAGE_HANDLER(ExtensionMsg_ShouldSuspend, OnShouldSuspend)
    IPC_MESSAGE_HANDLER(ExtensionMsg_Suspend, OnSuspend)
    IPC_MESSAGE_HANDLER(ExtensionMsg_CancelSuspend, OnCancelSuspend)
    IPC_MESSAGE_FORWARD(ExtensionMsg_WatchPages,
                        content_watcher_.get(), ContentWatcher::OnWatchPages)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void Dispatcher::WebKitInitialized() {
  // For extensions, we want to ensure we call the IdleHandler every so often,
  // even if the extension keeps up activity.
  if (is_extension_process_) {
    forced_idle_timer_.reset(new base::RepeatingTimer<content::RenderThread>);
    forced_idle_timer_->Start(FROM_HERE,
        base::TimeDelta::FromMilliseconds(kMaxExtensionIdleHandlerDelayMs),
        RenderThread::Get(), &RenderThread::IdleHandler);
  }

  // Initialize host permissions for any extensions that were activated before
  // WebKit was initialized.
  for (std::set<std::string>::iterator iter = active_extension_ids_.begin();
       iter != active_extension_ids_.end(); ++iter) {
    const Extension* extension = extensions_.GetByID(*iter);
    CHECK(extension);
  }

  EnableCustomElementWhiteList();

  is_webkit_initialized_ = true;
}

void Dispatcher::IdleNotification() {
  if (is_extension_process_) {
    // Dampen the forced delay as well if the extension stays idle for long
    // periods of time.
    int64 forced_delay_ms = std::max(
        RenderThread::Get()->GetIdleNotificationDelayInMs(),
        kMaxExtensionIdleHandlerDelayMs);
    forced_idle_timer_->Stop();
    forced_idle_timer_->Start(FROM_HERE,
        base::TimeDelta::FromMilliseconds(forced_delay_ms),
        RenderThread::Get(), &RenderThread::IdleHandler);
  }
}

void Dispatcher::OnRenderProcessShutdown() {
  v8_schema_registry_.reset();
  forced_idle_timer_.reset();
}

void Dispatcher::OnSetFunctionNames(
    const std::vector<std::string>& names) {
  function_names_.clear();
  for (size_t i = 0; i < names.size(); ++i)
    function_names_.insert(names[i]);
}

void Dispatcher::OnSetSystemFont(const std::string& font_family,
                                 const std::string& font_size) {
  system_font_family_ = font_family;
  system_font_size_ = font_size;
}

void Dispatcher::OnSetChannel(int channel) {
  SetCurrentChannel(static_cast<chrome::VersionInfo::Channel>(channel));
}

void Dispatcher::OnMessageInvoke(const std::string& extension_id,
                                 const std::string& module_name,
                                 const std::string& function_name,
                                 const base::ListValue& args,
                                 bool user_gesture) {
  InvokeModuleSystemMethod(
      NULL, extension_id, module_name, function_name, args, user_gesture);
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

  MessagingBindings::DispatchOnConnect(
      v8_context_set_.GetAll(),
      target_port_id, channel_name, source_tab,
      info.source_id, info.target_id, info.source_url,
      tls_channel_id,
      NULL);  // All render views.
}

void Dispatcher::OnDeliverMessage(int target_port_id,
                                  const Message& message) {
  scoped_ptr<RequestSender::ScopedTabID> scoped_tab_id;
  std::map<int, int>::const_iterator it =
      port_to_tab_id_map_.find(target_port_id);
  if (it != port_to_tab_id_map_.end()) {
    scoped_tab_id.reset(new RequestSender::ScopedTabID(request_sender(),
                                                       it->second));
  }

  MessagingBindings::DeliverMessage(
      v8_context_set_.GetAll(),
      target_port_id,
      message,
      NULL);  // All render views.
}

void Dispatcher::OnDispatchOnDisconnect(int port_id,
                                        const std::string& error_message) {
  MessagingBindings::DispatchOnDisconnect(
      v8_context_set_.GetAll(),
      port_id, error_message,
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
  AddOrRemoveBindings("");
}

void Dispatcher::OnLoadedInternal(scoped_refptr<const Extension> extension) {
  extensions_.Insert(extension);
}

void Dispatcher::OnUnloaded(const std::string& id) {
  extensions_.Remove(id);
  active_extension_ids_.erase(id);

  // If the extension is later reloaded with a different set of permissions,
  // we'd like it to get a new isolated world ID, so that it can pick up the
  // changed origin whitelist.
  user_script_slave_->RemoveIsolatedWorld(id);

  // Invalidate all of the contexts that were removed.
  // TODO(kalman): add an invalidation observer interface to ChromeV8Context.
  ChromeV8ContextSet::ContextSet removed_contexts =
      v8_context_set_.OnExtensionUnloaded(id);
  for (ChromeV8ContextSet::ContextSet::iterator it = removed_contexts.begin();
       it != removed_contexts.end(); ++it) {
    request_sender_->InvalidateSource(*it);
  }

  // Update the available bindings for the remaining contexts. These may have
  // changed if an externally_connectable extension is unloaded and a webpage
  // is no longer accessible.
  AddOrRemoveBindings("");

  // Invalidates the messages map for the extension in case the extension is
  // reloaded with a new messages map.
  EraseL10nMessagesMap(id);

  // We don't do anything with existing platform-app stylesheets. They will
  // stay resident, but the URL pattern corresponding to the unloaded
  // extension's URL just won't match anything anymore.
}

void Dispatcher::OnSetScriptingWhitelist(
    const ExtensionsClient::ScriptingWhitelist& extension_ids) {
  ExtensionsClient::Get()->SetScriptingWhitelist(extension_ids);
}

bool Dispatcher::IsExtensionActive(
    const std::string& extension_id) const {
  bool is_active =
      active_extension_ids_.find(extension_id) != active_extension_ids_.end();
  if (is_active)
    CHECK(extensions_.Contains(extension_id));
  return is_active;
}

v8::Handle<v8::Object> Dispatcher::GetOrCreateObject(
    v8::Handle<v8::Object> object,
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

void Dispatcher::AddOrRemoveBindingsForContext(ChromeV8Context* context) {
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
      RegisterBinding("app", context);
      RegisterBinding("webstore", context);

      // ... and that the runtime API might be available if any extension can
      // connect to it.
      bool runtime_is_available = false;
      for (ExtensionSet::const_iterator it = extensions_.begin();
           it != extensions_.end(); ++it) {
        ExternallyConnectableInfo* info =
            static_cast<ExternallyConnectableInfo*>((*it)->GetManifestData(
                manifest_keys::kExternallyConnectable));
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
    case Feature::CONTENT_SCRIPT_CONTEXT: {
      // Extension context; iterate through all the APIs and bind the available
      // ones.
      FeatureProvider* api_feature_provider = FeatureProvider::GetAPIFeatures();
      const std::vector<std::string>& apis =
          api_feature_provider->GetAllFeatureNames();
      for (std::vector<std::string>::const_iterator it = apis.begin();
          it != apis.end(); ++it) {
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

v8::Handle<v8::Object> Dispatcher::GetOrCreateBindObjectIfAvailable(
    const std::string& api_name,
    std::string* bind_name,
    ChromeV8Context* context) {
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
  FeatureProvider* api_feature_provider = FeatureProvider::GetAPIFeatures();
  std::string ancestor_name;
  bool only_ancestor_available = false;

  for (size_t i = 0; i < split.size() - 1; ++i) {
    ancestor_name += (i ? ".": "") + split[i];
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

  return bind_object.IsEmpty() ?
      AsObjectOrEmpty(GetOrCreateChrome(context)) : bind_object;
}

void Dispatcher::RegisterBinding(const std::string& api_name,
                                 ChromeV8Context* context) {
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
  if (lazy_bindings_map_.find(api_name) != lazy_bindings_map_.end()) {
    InstallBindings(module_system, context->v8_context(), api_name);
  } else if (!source_map_.Contains(api_name)) {
    module_system->RegisterNativeHandler(
        api_name,
        scoped_ptr<NativeHandler>(new BindingGeneratingNativeHandler(
            module_system,
            api_name,
            "binding")));
    module_system->SetNativeLazyField(bind_object,
                                      bind_name,
                                      api_name,
                                      "binding");
  } else {
    module_system->SetLazyField(bind_object,
                                bind_name,
                                api_name,
                                "binding");
  }
}

// NOTE: please use the naming convention "foo_natives" for these.
void Dispatcher::RegisterNativeHandlers(ModuleSystem* module_system,
                                        ChromeV8Context* context) {
  module_system->RegisterNativeHandler("event_natives",
      scoped_ptr<NativeHandler>(EventBindings::Create(this, context)));
  module_system->RegisterNativeHandler("messaging_natives",
      scoped_ptr<NativeHandler>(MessagingBindings::Get(this, context)));
  module_system->RegisterNativeHandler("apiDefinitions",
      scoped_ptr<NativeHandler>(new ApiDefinitionsNatives(this, context)));
  module_system->RegisterNativeHandler("sendRequest",
      scoped_ptr<NativeHandler>(
          new SendRequestNatives(this, request_sender_.get(), context)));
  module_system->RegisterNativeHandler("setIcon",
      scoped_ptr<NativeHandler>(
          new SetIconNatives(this, request_sender_.get(), context)));
  module_system->RegisterNativeHandler("activityLogger",
      scoped_ptr<NativeHandler>(new APIActivityLogger(this, context)));
  module_system->RegisterNativeHandler("renderViewObserverNatives",
      scoped_ptr<NativeHandler>(new RenderViewObserverNatives(this, context)));

  // Natives used by multiple APIs.
  module_system->RegisterNativeHandler("file_system_natives",
      scoped_ptr<NativeHandler>(new FileSystemNatives(context)));

  // Custom bindings.
  module_system->RegisterNativeHandler("app",
      scoped_ptr<NativeHandler>(new AppBindings(this, context)));
  module_system->RegisterNativeHandler("app_runtime",
      scoped_ptr<NativeHandler>(
          new AppRuntimeCustomBindings(this, context)));
  module_system->RegisterNativeHandler("app_window_natives",
      scoped_ptr<NativeHandler>(
          new AppWindowCustomBindings(this, context)));
  module_system->RegisterNativeHandler("blob_natives",
      scoped_ptr<NativeHandler>(new BlobNativeHandler(context)));
  module_system->RegisterNativeHandler("context_menus",
      scoped_ptr<NativeHandler>(
          new ContextMenusCustomBindings(this, context)));
  module_system->RegisterNativeHandler(
      "css_natives", scoped_ptr<NativeHandler>(new CssNativeHandler(context)));
  module_system->RegisterNativeHandler("document_natives",
      scoped_ptr<NativeHandler>(
          new DocumentCustomBindings(this, context)));
  module_system->RegisterNativeHandler("sync_file_system",
      scoped_ptr<NativeHandler>(
          new SyncFileSystemCustomBindings(this, context)));
  module_system->RegisterNativeHandler("file_browser_handler",
      scoped_ptr<NativeHandler>(new FileBrowserHandlerCustomBindings(
          this, context)));
  module_system->RegisterNativeHandler("file_browser_private",
      scoped_ptr<NativeHandler>(new FileBrowserPrivateCustomBindings(
          this, context)));
  module_system->RegisterNativeHandler("i18n",
      scoped_ptr<NativeHandler>(
          new I18NCustomBindings(this, context)));
  module_system->RegisterNativeHandler(
      "id_generator",
      scoped_ptr<NativeHandler>(new IdGeneratorCustomBindings(this, context)));
  module_system->RegisterNativeHandler("mediaGalleries",
      scoped_ptr<NativeHandler>(
          new MediaGalleriesCustomBindings(this, context)));
  module_system->RegisterNativeHandler("page_actions",
      scoped_ptr<NativeHandler>(
          new PageActionsCustomBindings(this, context)));
  module_system->RegisterNativeHandler("page_capture",
      scoped_ptr<NativeHandler>(
          new PageCaptureCustomBindings(this, context)));
  module_system->RegisterNativeHandler(
      "pepper_request_natives",
      scoped_ptr<NativeHandler>(new PepperRequestNatives(context)));
  module_system->RegisterNativeHandler("runtime",
      scoped_ptr<NativeHandler>(new RuntimeCustomBindings(this, context)));
  module_system->RegisterNativeHandler("tabs",
      scoped_ptr<NativeHandler>(new TabsCustomBindings(this, context)));
  module_system->RegisterNativeHandler("webstore",
      scoped_ptr<NativeHandler>(new WebstoreBindings(this, context)));
#if defined(ENABLE_WEBRTC)
  module_system->RegisterNativeHandler("cast_streaming_natives",
      scoped_ptr<NativeHandler>(new CastStreamingNativeHandler(context)));
#endif
}

void Dispatcher::PopulateSourceMap() {
  // Libraries.
  source_map_.RegisterSource("entryIdManager", IDR_ENTRY_ID_MANAGER);
  source_map_.RegisterSource(kEventBindings, IDR_EVENT_BINDINGS_JS);
  source_map_.RegisterSource("imageUtil", IDR_IMAGE_UTIL_JS);
  source_map_.RegisterSource("json_schema", IDR_JSON_SCHEMA_JS);
  source_map_.RegisterSource("lastError", IDR_LAST_ERROR_JS);
  source_map_.RegisterSource("messaging", IDR_MESSAGING_JS);
  source_map_.RegisterSource("messaging_utils", IDR_MESSAGING_UTILS_JS);
  source_map_.RegisterSource("pepper_request", IDR_PEPPER_REQUEST_JS);
  source_map_.RegisterSource(kSchemaUtils, IDR_SCHEMA_UTILS_JS);
  source_map_.RegisterSource("sendRequest", IDR_SEND_REQUEST_JS);
  source_map_.RegisterSource("setIcon", IDR_SET_ICON_JS);
  source_map_.RegisterSource("test", IDR_TEST_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("unload_event", IDR_UNLOAD_EVENT_JS);
  source_map_.RegisterSource("utils", IDR_UTILS_JS);

  // Custom bindings.
  source_map_.RegisterSource("app", IDR_APP_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("app.runtime", IDR_APP_RUNTIME_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("app.window", IDR_APP_WINDOW_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("automation", IDR_AUTOMATION_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("automationEvent", IDR_AUTOMATION_EVENT_JS);
  source_map_.RegisterSource("automationNode", IDR_AUTOMATION_NODE_JS);
  source_map_.RegisterSource("automationTree", IDR_AUTOMATION_TREE_JS);
  source_map_.RegisterSource("browserAction",
                             IDR_BROWSER_ACTION_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("contextMenus",
                             IDR_CONTEXT_MENUS_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("declarativeContent",
                             IDR_DECLARATIVE_CONTENT_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("declarativeWebRequest",
                             IDR_DECLARATIVE_WEBREQUEST_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("desktopCapture",
                             IDR_DESKTOP_CAPTURE_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("developerPrivate",
                             IDR_DEVELOPER_PRIVATE_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("downloads",
                             IDR_DOWNLOADS_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("extension", IDR_EXTENSION_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("feedbackPrivate",
                             IDR_FEEDBACK_PRIVATE_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("fileBrowserHandler",
                             IDR_FILE_BROWSER_HANDLER_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("fileBrowserPrivate",
                             IDR_FILE_BROWSER_PRIVATE_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("fileSystem",
                             IDR_FILE_SYSTEM_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("fileSystemProvider",
                             IDR_FILE_SYSTEM_PROVIDER_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("gcm",
                             IDR_GCM_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("i18n", IDR_I18N_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("identity", IDR_IDENTITY_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("imageWriterPrivate",
                             IDR_IMAGE_WRITER_PRIVATE_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("input.ime", IDR_INPUT_IME_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("mediaGalleries",
                             IDR_MEDIA_GALLERIES_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("notifications",
                             IDR_NOTIFICATIONS_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("omnibox", IDR_OMNIBOX_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("pageActions",
                             IDR_PAGE_ACTIONS_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("pageAction", IDR_PAGE_ACTION_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("pageCapture",
                             IDR_PAGE_CAPTURE_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("permissions", IDR_PERMISSIONS_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("runtime", IDR_RUNTIME_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("syncFileSystem",
                             IDR_SYNC_FILE_SYSTEM_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("systemIndicator",
                             IDR_SYSTEM_INDICATOR_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("tabCapture", IDR_TAB_CAPTURE_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("tabs", IDR_TABS_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("tts", IDR_TTS_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("ttsEngine", IDR_TTS_ENGINE_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("webRequest", IDR_WEB_REQUEST_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("webRequestInternal",
                             IDR_WEB_REQUEST_INTERNAL_CUSTOM_BINDINGS_JS);
#if defined(ENABLE_WEBRTC)
  source_map_.RegisterSource("cast.streaming.rtpStream",
                             IDR_CAST_STREAMING_RTP_STREAM_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("cast.streaming.session",
                             IDR_CAST_STREAMING_SESSION_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource(
      "cast.streaming.udpTransport",
      IDR_CAST_STREAMING_UDP_TRANSPORT_CUSTOM_BINDINGS_JS);
#endif
  source_map_.RegisterSource("webstore", IDR_WEBSTORE_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("windowControls", IDR_WINDOW_CONTROLS_JS);
  source_map_.RegisterSource("binding", IDR_BINDING_JS);

  // Custom types sources.
  source_map_.RegisterSource("ChromeSetting", IDR_CHROME_SETTING_JS);
  source_map_.RegisterSource("StorageArea", IDR_STORAGE_AREA_JS);
  source_map_.RegisterSource("ContentSetting", IDR_CONTENT_SETTING_JS);
  source_map_.RegisterSource("ChromeDirectSetting",
                             IDR_CHROME_DIRECT_SETTING_JS);

  // Platform app sources that are not API-specific..
  source_map_.RegisterSource("tagWatcher", IDR_TAG_WATCHER_JS);
  source_map_.RegisterSource("webview", IDR_WEBVIEW_CUSTOM_BINDINGS_JS);
  // Note: webView not webview so that this doesn't interfere with the
  // chrome.webview API bindings.
  source_map_.RegisterSource("webView", IDR_WEB_VIEW_JS);
  source_map_.RegisterSource("webViewExperimental",
                             IDR_WEB_VIEW_EXPERIMENTAL_JS);
  source_map_.RegisterSource("webViewRequest",
                             IDR_WEB_VIEW_REQUEST_CUSTOM_BINDINGS_JS);
  source_map_.RegisterSource("denyWebView", IDR_WEB_VIEW_DENY_JS);
  source_map_.RegisterSource("adView", IDR_AD_VIEW_JS);
  source_map_.RegisterSource("denyAdView", IDR_AD_VIEW_DENY_JS);
  source_map_.RegisterSource("platformApp", IDR_PLATFORM_APP_JS);
  source_map_.RegisterSource("injectAppTitlebar", IDR_INJECT_APP_TITLEBAR_JS);
}

void Dispatcher::PopulateLazyBindingsMap() {
  lazy_bindings_map_["app"] = InstallAppBindings;
  lazy_bindings_map_["webstore"] = InstallWebstoreBindings;
}

void Dispatcher::InstallBindings(ModuleSystem* module_system,
                                 v8::Handle<v8::Context> v8_context,
                                 const std::string& api) {
  std::map<std::string, BindingInstaller>::const_iterator lazy_binding =
      lazy_bindings_map_.find(api);
  if (lazy_binding != lazy_bindings_map_.end()) {
    v8::Handle<v8::Object> global(v8_context->Global());
    v8::Handle<v8::Object> chrome =
        global->Get(v8::String::NewFromUtf8(v8_context->GetIsolate(), "chrome"))
            ->ToObject();
    (*lazy_binding->second)(module_system, chrome);
  } else {
    module_system->Require(api);
  }
}

void Dispatcher::DidCreateScriptContext(
    WebFrame* frame, v8::Handle<v8::Context> v8_context, int extension_group,
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

  ChromeV8Context* context =
      new ChromeV8Context(v8_context, frame, extension, context_type);
  v8_context_set_.Add(context);

  if (extension)
    InitOriginPermissions(extension, context_type);

  {
    scoped_ptr<ModuleSystem> module_system(new ModuleSystem(context,
                                                            &source_map_));
    context->set_module_system(module_system.Pass());
  }
  ModuleSystem* module_system = context->module_system();

  // Enable natives in startup.
  ModuleSystem::NativesEnabledScope natives_enabled_scope(
      module_system);

  RegisterNativeHandlers(module_system, context);

  module_system->RegisterNativeHandler("chrome",
      scoped_ptr<NativeHandler>(new ChromeNativeHandler(context)));
  module_system->RegisterNativeHandler("print",
      scoped_ptr<NativeHandler>(new PrintNativeHandler(context)));
  module_system->RegisterNativeHandler("lazy_background_page",
      scoped_ptr<NativeHandler>(
          new LazyBackgroundPageNativeHandler(this, context)));
  module_system->RegisterNativeHandler("logging",
      scoped_ptr<NativeHandler>(new LoggingNativeHandler(context)));
  module_system->RegisterNativeHandler("schema_registry",
      v8_schema_registry_->AsNativeHandler());
  module_system->RegisterNativeHandler("v8_context",
      scoped_ptr<NativeHandler>(new V8ContextNativeHandler(context, this)));
  module_system->RegisterNativeHandler("test_features",
      scoped_ptr<NativeHandler>(new TestFeaturesNativeHandler(context)));
  module_system->RegisterNativeHandler("user_gestures",
      scoped_ptr<NativeHandler>(new UserGesturesNativeHandler(context)));
  module_system->RegisterNativeHandler("utils",
      scoped_ptr<NativeHandler>(new UtilsNativeHandler(context)));

  int manifest_version = extension ? extension->manifest_version() : 1;
  bool send_request_disabled =
      (extension && Manifest::IsUnpackedLocation(extension->location()) &&
       BackgroundInfo::HasLazyBackgroundPage(extension));
  module_system->RegisterNativeHandler("process",
      scoped_ptr<NativeHandler>(new ProcessInfoNativeHandler(
          this, context, context->GetExtensionID(),
          context->GetContextTypeDescription(),
          ChromeRenderProcessObserver::is_incognito_process(),
          manifest_version, send_request_disabled)));

  // chrome.Event is part of the public API (although undocumented). Make it
  // lazily evalulate to Event from event_bindings.js. For extensions only
  // though, not all webpages!
  if (context->extension()) {
    v8::Handle<v8::Object> chrome = AsObjectOrEmpty(GetOrCreateChrome(context));
    if (!chrome.IsEmpty())
      module_system->SetLazyField(chrome, "Event", kEventBindings, "Event");
  }

  AddOrRemoveBindingsForContext(context);

  bool is_within_platform_app = IsWithinPlatformApp();
  // Inject custom JS into the platform app context.
  if (is_within_platform_app) {
    module_system->Require("platformApp");
  }

  if (context_type == Feature::BLESSED_EXTENSION_CONTEXT &&
      is_within_platform_app &&
      GetCurrentChannel() <= chrome::VersionInfo::CHANNEL_DEV &&
      CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kEnableAppWindowControls)) {
    module_system->Require("windowControls");
  }

  // We used to limit WebView to |BLESSED_EXTENSION_CONTEXT| within platform
  // apps. An ext/app runs in a blessed extension context, if it is the active
  // extension in the current process, in other words, if it is loaded in a top
  // frame. To support webview in a non-frame extension, we have to allow
  // unblessed extension context as well.
  // Note: setting up the WebView class here, not the chrome.webview API.
  // The API will be automatically set up when first used.
  if (context_type == Feature::BLESSED_EXTENSION_CONTEXT ||
      context_type == Feature::UNBLESSED_EXTENSION_CONTEXT) {
    if (extension->HasAPIPermission(APIPermission::kWebView)) {
      module_system->Require("webView");
      if (GetCurrentChannel() <= chrome::VersionInfo::CHANNEL_DEV) {
        module_system->Require("webViewExperimental");
      } else {
        // TODO(asargent) We need a whitelist for webview experimental.
        // crbug.com/264852
        std::string id_hash = base::SHA1HashString(extension->id());
        std::string hexencoded_id_hash = base::HexEncode(id_hash.c_str(),
                                                        id_hash.length());
        if (hexencoded_id_hash == "8C3741E3AF0B93B6E8E0DDD499BB0B74839EA578" ||
            hexencoded_id_hash == "E703483CEF33DEC18B4B6DD84B5C776FB9182BDB" ||
            hexencoded_id_hash == "1A26E32DE447A17CBE5E9750CDBA78F58539B39C" ||
            hexencoded_id_hash == "59048028102D7B4C681DBC7BC6CD980C3DC66DA3") {
          module_system->Require("webViewExperimental");
        }
      }
    } else {
      module_system->Require("denyWebView");
    }
  }

  // Same comment as above for <adview> tag.
  if (context_type == Feature::BLESSED_EXTENSION_CONTEXT &&
      is_within_platform_app) {
    if (CommandLine::ForCurrentProcess()->HasSwitch(
            ::switches::kEnableAdview)) {
      if (extension->HasAPIPermission(APIPermission::kAdView)) {
        module_system->Require("adView");
      } else {
        module_system->Require("denyAdView");
      }
    }
  }

  VLOG(1) << "Num tracked contexts: " << v8_context_set_.size();
}

std::string Dispatcher::GetExtensionID(const WebFrame* frame, int world_id) {
  if (world_id != 0) {
    // Isolated worlds (content script).
    return user_script_slave_->GetExtensionIdForIsolatedWorld(world_id);
  }

  // TODO(kalman): Delete this check.
  if (frame->document().securityOrigin().isUnique())
    return std::string();

  // Extension pages (chrome-extension:// URLs).
  GURL frame_url = ScriptContext::GetDataSourceURLForFrame(frame);
  return extensions_.GetExtensionOrAppIDByURL(frame_url);
}

bool Dispatcher::IsWithinPlatformApp() {
  for (std::set<std::string>::iterator iter = active_extension_ids_.begin();
       iter != active_extension_ids_.end(); ++iter) {
    const Extension* extension = extensions_.GetByID(*iter);
    if (extension && extension->is_platform_app())
      return true;
  }
  return false;
}

void Dispatcher::WillReleaseScriptContext(
    WebFrame* frame, v8::Handle<v8::Context> v8_context, int world_id) {
  ChromeV8Context* context = v8_context_set_.GetByV8Context(v8_context);
  if (!context)
    return;

  context->DispatchOnUnloadEvent();
  // TODO(kalman): add an invalidation observer interface to ChromeV8Context.
  request_sender_->InvalidateSource(context);

  v8_context_set_.Remove(context);
  VLOG(1) << "Num tracked contexts: " << v8_context_set_.size();
}

void Dispatcher::DidCreateDocumentElement(blink::WebFrame* frame) {
  if (IsWithinPlatformApp()) {
    // WebKit doesn't let us define an additional user agent stylesheet, so we
    // insert the default platform app stylesheet into all documents that are
    // loaded in each app.
    std::string stylesheet =
        ResourceBundle::GetSharedInstance().
            GetRawDataResource(IDR_PLATFORM_APP_CSS).as_string();
    ReplaceFirstSubstringAfterOffset(&stylesheet, 0,
                                     "$FONTFAMILY", system_font_family_);
    ReplaceFirstSubstringAfterOffset(&stylesheet, 0,
                                     "$FONTSIZE", system_font_size_);
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
    base::snprintf(minidump, arraysize(minidump),
        "e::dispatcher:%s:%s", extension_id.c_str(), error.c_str());
    CHECK(extension) << extension_id << " was never loaded: " << error;
  }

  active_extension_ids_.insert(extension_id);

  // This is called when starting a new extension page, so start the idle
  // handler ticking.
  RenderThread::Get()->ScheduleIdleHandler(kInitialExtensionIdleHandlerDelayMs);

  UpdateActiveExtensions();

  if (is_webkit_initialized_) {
    DOMActivityLogger::AttachToWorld(DOMActivityLogger::kMainWorldId,
                                     extension_id);
  }
}

void Dispatcher::InitOriginPermissions(const Extension* extension,
                                       Feature::Context context_type) {
  // TODO(jstritar): We should try to remove this special case. Also, these
  // whitelist entries need to be updated when the kManagement permission
  // changes.
  if (context_type == Feature::BLESSED_EXTENSION_CONTEXT &&
      extension->HasAPIPermission(APIPermission::kManagement)) {
    WebSecurityPolicy::addOriginAccessWhitelistEntry(
        extension->url(),
        WebString::fromUTF8(content::kChromeUIScheme),
        WebString::fromUTF8(chrome::kChromeUIExtensionIconHost),
        false);
  }

  AddOrRemoveOriginPermissions(
      UpdatedExtensionPermissionsInfo::ADDED,
      extension,
      PermissionsData::GetEffectiveHostPermissions(extension));
}

void Dispatcher::AddOrRemoveOriginPermissions(
    UpdatedExtensionPermissionsInfo::Reason reason,
    const Extension* extension,
    const URLPatternSet& origins) {
  for (URLPatternSet::const_iterator i = origins.begin();
       i != origins.end(); ++i) {
    const char* schemes[] = {
      content::kHttpScheme,
      content::kHttpsScheme,
      content::kFileScheme,
      content::kChromeUIScheme,
      content::kFtpScheme,
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

void Dispatcher::EnableCustomElementWhiteList() {
  blink::WebCustomElement::addEmbedderCustomElementName("webview");
  // TODO(fsamuel): Add <adview> to the whitelist once it has been converted
  // into a custom element.
  blink::WebCustomElement::addEmbedderCustomElementName("browser-plugin");
}

void Dispatcher::AddOrRemoveBindings(const std::string& extension_id) {
  v8_context_set().ForEach(
      extension_id,
      NULL,  // all render views
      base::Bind(&Dispatcher::AddOrRemoveBindingsForContext,
                 base::Unretained(this)));
}

void Dispatcher::OnUpdatePermissions(
  const ExtensionMsg_UpdatePermissions_Params& params) {
  int reason_id = params.reason_id;
  const std::string& extension_id = params.extension_id;
  const APIPermissionSet& apis = params.apis;
  const ManifestPermissionSet& manifest_permissions =
      params.manifest_permissions;
  const URLPatternSet& explicit_hosts = params.explicit_hosts;
  const URLPatternSet& scriptable_hosts = params.scriptable_hosts;

  const Extension* extension = extensions_.GetByID(extension_id);
  if (!extension)
    return;

  scoped_refptr<const PermissionSet> delta =
      new PermissionSet(apis, manifest_permissions,
                        explicit_hosts, scriptable_hosts);
  scoped_refptr<const PermissionSet> old_active =
      extension->GetActivePermissions();
  UpdatedExtensionPermissionsInfo::Reason reason =
      static_cast<UpdatedExtensionPermissionsInfo::Reason>(reason_id);

  const PermissionSet* new_active = NULL;
  switch (reason) {
    case UpdatedExtensionPermissionsInfo::ADDED:
      new_active = PermissionSet::CreateUnion(old_active.get(), delta.get());
      break;
    case UpdatedExtensionPermissionsInfo::REMOVED:
      new_active =
          PermissionSet::CreateDifference(old_active.get(), delta.get());
      break;
  }

  PermissionsData::SetActivePermissions(extension, new_active);
  AddOrRemoveOriginPermissions(reason, extension, explicit_hosts);
  AddOrRemoveBindings(extension->id());
}

void Dispatcher::OnUpdateTabSpecificPermissions(
    int page_id,
    int tab_id,
    const std::string& extension_id,
    const URLPatternSet& origin_set) {
  RenderView* view = TabFinder::Find(tab_id);

  // For now, the message should only be sent to the render view that contains
  // the target tab. This may change. Either way, if this is the target tab it
  // gives us the chance to check against the page ID to avoid races.
  DCHECK(view);
  if (view && view->GetPageId() != page_id)
    return;

  const Extension* extension = extensions_.GetByID(extension_id);
  if (!extension)
    return;

  PermissionsData::UpdateTabSpecificPermissions(
      extension,
      tab_id,
      new PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                        origin_set, URLPatternSet()));
}

void Dispatcher::OnClearTabSpecificPermissions(
    int tab_id,
    const std::vector<std::string>& extension_ids) {
  for (std::vector<std::string>::const_iterator it = extension_ids.begin();
       it != extension_ids.end(); ++it) {
    const Extension* extension = extensions_.GetByID(*it);
    if (extension)
      PermissionsData::ClearTabSpecificPermissions(extension, tab_id);
  }
}

void Dispatcher::OnUpdateUserScripts(
    base::SharedMemoryHandle scripts) {
  DCHECK(base::SharedMemory::IsHandleValid(scripts)) << "Bad scripts handle";
  user_script_slave_->UpdateScripts(scripts);
  UpdateActiveExtensions();
}

void Dispatcher::UpdateActiveExtensions() {
  // In single-process mode, the browser process reports the active extensions.
  if (CommandLine::ForCurrentProcess()->HasSwitch(::switches::kSingleProcess))
    return;

  std::set<std::string> active_extensions = active_extension_ids_;
  user_script_slave_->GetActiveExtensions(&active_extensions);
  crash_keys::SetActiveExtensions(active_extensions);
}

void Dispatcher::OnUsingWebRequestAPI(
    bool adblock, bool adblock_plus, bool other) {
  webrequest_adblock_ = adblock;
  webrequest_adblock_plus_ = adblock_plus;
  webrequest_other_ = other;
}

void Dispatcher::OnShouldSuspend(const std::string& extension_id,
                                 int sequence_id) {
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

void Dispatcher::OnCancelSuspend(const std::string& extension_id) {
  DispatchEvent(extension_id, kOnSuspendCanceledEvent);
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
  DCHECK_GE(extension_group, 0);
  if (extension_group == EXTENSION_GROUP_CONTENT_SCRIPTS) {
    return extension ?  // TODO(kalman): when does this happen?
        Feature::CONTENT_SCRIPT_CONTEXT : Feature::UNSPECIFIED_CONTEXT;
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
            extension->location() != Manifest::COMPONENT) ?
        Feature::BLESSED_WEB_PAGE_CONTEXT : Feature::BLESSED_EXTENSION_CONTEXT;
  }

  // TODO(kalman): This isUnique() check is wrong, it should be performed as
  // part of IsSandboxedPage().
  if (!origin.isUnique() && extensions_.ExtensionBindingsAllowed(url)) {
    if (!extension)  // TODO(kalman): when does this happen?
      return Feature::UNSPECIFIED_CONTEXT;
    return extension->is_hosted_app() ?
        Feature::BLESSED_WEB_PAGE_CONTEXT :
        Feature::UNBLESSED_EXTENSION_CONTEXT;
  }

  if (url.is_valid())
    return Feature::WEB_PAGE_CONTEXT;

  return Feature::UNSPECIFIED_CONTEXT;
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

  if (!context->extension()) {
    context->isolate()->ThrowException(v8::Exception::Error(
        v8::String::NewFromUtf8(context->isolate(), "Not in an extension.")));
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
  v8_context_set_.ForEach(
      extension_id,
      NULL,  // all render views
      base::Bind(&CallModuleMethod,
                 local_event_bindings,
                 kEventDispatchFunction,
                 &args));
}

void Dispatcher::InvokeModuleSystemMethod(
      content::RenderView* render_view,
      const std::string& extension_id,
      const std::string& module_name,
      const std::string& function_name,
      const base::ListValue& args,
      bool user_gesture) {
  scoped_ptr<WebScopedUserGesture> web_user_gesture;
  if (user_gesture)
    web_user_gesture.reset(new WebScopedUserGesture);

  v8_context_set_.ForEach(
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
      background_view->Send(new ExtensionHostMsg_EventAck(
          background_view->GetRoutingID()));
    }
  }
}

void Dispatcher::ClearPortData(int port_id) {
  // Only the target port side has entries in |port_to_tab_id_map_|. If
  // |port_id| is a source port, std::map::erase() will just silently fail
  // here as a no-op.
  port_to_tab_id_map_.erase(port_id);
}

}  // namespace extensions
