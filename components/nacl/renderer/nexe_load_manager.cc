// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/renderer/nexe_load_manager.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "components/nacl/common/nacl_host_messages.h"
#include "components/nacl/common/nacl_types.h"
#include "components/nacl/renderer/pnacl_translation_resource_host.h"
#include "components/nacl/renderer/trusted_plugin_channel.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/sandbox_init.h"
#include "content/public/renderer/pepper_plugin_instance.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/private/pp_file_handle.h"
#include "ppapi/native_client/src/trusted/plugin/nacl_entry_points.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/ppapi_permissions.h"
#include "ppapi/shared_impl/ppapi_preferences.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/enter.h"
#include "third_party/WebKit/public/web/WebDOMResourceProgressEvent.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "v8/include/v8.h"

namespace {

void HistogramEnumerate(const std::string& name,
                        int32_t sample,
                        int32_t boundary_value) {
  base::HistogramBase* counter =
      base::LinearHistogram::FactoryGet(
          name,
          1,
          boundary_value,
          boundary_value + 1,
          base::HistogramBase::kUmaTargetedHistogramFlag);
  counter->Add(sample);
}

blink::WebString EventTypeToString(PP_NaClEventType event_type) {
  switch (event_type) {
    case PP_NACL_EVENT_LOADSTART:
      return blink::WebString::fromUTF8("loadstart");
    case PP_NACL_EVENT_PROGRESS:
      return blink::WebString::fromUTF8("progress");
    case PP_NACL_EVENT_ERROR:
      return blink::WebString::fromUTF8("error");
    case PP_NACL_EVENT_ABORT:
      return blink::WebString::fromUTF8("abort");
    case PP_NACL_EVENT_LOAD:
      return blink::WebString::fromUTF8("load");
    case PP_NACL_EVENT_LOADEND:
      return blink::WebString::fromUTF8("loadend");
    case PP_NACL_EVENT_CRASH:
      return blink::WebString::fromUTF8("crash");
  }
  NOTIMPLEMENTED();
  return blink::WebString();
}

static int GetRoutingID(PP_Instance instance) {
  // Check that we are on the main renderer thread.
  DCHECK(content::RenderThread::Get());
  content::RendererPpapiHost *host =
      content::RendererPpapiHost::GetForPPInstance(instance);
  if (!host)
    return 0;
  return host->GetRoutingIDForWidget(instance);
}

}  // namespace

namespace nacl {

NexeLoadManager::NexeLoadManager(
    PP_Instance pp_instance)
    : pp_instance_(pp_instance),
      nacl_ready_state_(PP_NACL_READY_STATE_UNSENT),
      nexe_error_reported_(false),
      plugin_instance_(content::PepperPluginInstance::Get(pp_instance)),
      weak_factory_(this) {
}

NexeLoadManager::~NexeLoadManager() {
}

void NexeLoadManager::ReportLoadError(PP_NaClError error,
                                      const std::string& error_message,
                                      const std::string& console_message) {
  // Check that we are on the main renderer thread.
  DCHECK(content::RenderThread::Get());

  if (error == PP_NACL_ERROR_MANIFEST_PROGRAM_MISSING_ARCH) {
    // A special case: the manifest may otherwise be valid but is missing
    // a program/file compatible with the user's sandbox.
    IPC::Sender* sender = content::RenderThread::Get();
    sender->Send(
        new NaClHostMsg_MissingArchError(GetRoutingID(pp_instance_)));
  }
  set_nacl_ready_state(PP_NACL_READY_STATE_DONE);
  nexe_error_reported_ = true;

  // We must set all properties before calling DispatchEvent so that when an
  // event handler runs, the properties reflect the current load state.
  std::string error_string = std::string("NaCl module load failed: ") +
      std::string(error_message);
  plugin_instance_->SetEmbedProperty(
      ppapi::StringVar::StringToPPVar("lastError"),
      ppapi::StringVar::StringToPPVar(error_string));

  // Inform JavaScript that loading encountered an error and is complete.
  ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
      FROM_HERE,
      base::Bind(&NexeLoadManager::DispatchEvent,
                 weak_factory_.GetWeakPtr(),
                 ProgressEvent(PP_NACL_EVENT_ERROR)));

  ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
      FROM_HERE,
      base::Bind(&NexeLoadManager::DispatchEvent,
                 weak_factory_.GetWeakPtr(),
                 ProgressEvent(PP_NACL_EVENT_LOADEND)));

  HistogramEnumerate("NaCl.LoadStatus.Plugin", error,
                     PP_NACL_ERROR_MAX);
  std::string uma_name = is_installed_ ?
                         "NaCl.LoadStatus.Plugin.InstalledApp" :
                         "NaCl.LoadStatus.Plugin.NotInstalledApp";
  HistogramEnumerate(uma_name, error, PP_NACL_ERROR_MAX);

  LogToConsole(console_message);
}

void NexeLoadManager::DispatchEvent(const ProgressEvent &event) {
  blink::WebPluginContainer* container = plugin_instance_->GetContainer();
  // It's possible that container() is NULL if the plugin has been removed from
  // the DOM (but the PluginInstance is not destroyed yet).
  if (!container)
    return;
  blink::WebFrame* frame = container->element().document().frame();
  if (!frame)
    return;
  v8::HandleScope handle_scope(plugin_instance_->GetIsolate());
  v8::Local<v8::Context> context(
      plugin_instance_->GetIsolate()->GetCurrentContext());
  if (context.IsEmpty()) {
    // If there's no JavaScript on the stack, we have to make a new Context.
    context = v8::Context::New(plugin_instance_->GetIsolate());
  }
  v8::Context::Scope context_scope(context);

  if (!event.resource_url.empty()) {
    blink::WebString url_string = blink::WebString::fromUTF8(
        event.resource_url.data(), event.resource_url.size());
    blink::WebDOMResourceProgressEvent blink_event(
        EventTypeToString(event.event_type),
        event.length_is_computable,
        event.loaded_bytes,
        event.total_bytes,
        url_string);
    container->element().dispatchEvent(blink_event);
  } else {
    blink::WebDOMProgressEvent blink_event(EventTypeToString(event.event_type),
                                           event.length_is_computable,
                                           event.loaded_bytes,
                                           event.total_bytes);
    container->element().dispatchEvent(blink_event);
  }
}

void NexeLoadManager::set_trusted_plugin_channel(
    scoped_ptr<TrustedPluginChannel> channel) {
  trusted_plugin_channel_ = channel.Pass();
}

bool NexeLoadManager::nexe_error_reported() {
  return nexe_error_reported_;
}

void NexeLoadManager::set_nexe_error_reported(bool error_reported) {
  nexe_error_reported_ = error_reported;
}

PP_NaClReadyState NexeLoadManager::nacl_ready_state() {
  return nacl_ready_state_;
}

void NexeLoadManager::set_nacl_ready_state(PP_NaClReadyState ready_state) {
  nacl_ready_state_ = ready_state;
  SetReadOnlyProperty(ppapi::StringVar::StringToPPVar("readyState"),
                      PP_MakeInt32(ready_state));
}

void NexeLoadManager::SetReadOnlyProperty(PP_Var key, PP_Var value) {
  plugin_instance_->SetEmbedProperty(key, value);
}

void NexeLoadManager::LogToConsole(const std::string& message) {
  ppapi::PpapiGlobals::Get()->LogWithSource(
      pp_instance_, PP_LOGLEVEL_LOG, std::string("NativeClient"), message);
}

}  // namespace nacl
