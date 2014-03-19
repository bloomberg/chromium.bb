// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/renderer/ppb_nacl_private_impl.h"

#ifndef DISABLE_NACL

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/rand_util.h"
#include "components/nacl/common/nacl_host_messages.h"
#include "components/nacl/common/nacl_messages.h"
#include "components/nacl/common/nacl_switches.h"
#include "components/nacl/common/nacl_types.h"
#include "components/nacl/renderer/pnacl_translation_resource_host.h"
#include "components/nacl/renderer/sandbox_arch.h"
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
#include "url/gurl.h"
#include "v8/include/v8.h"

namespace {

base::LazyInstance<scoped_refptr<PnaclTranslationResourceHost> >
    g_pnacl_resource_host = LAZY_INSTANCE_INITIALIZER;

static bool InitializePnaclResourceHost() {
  // Must run on the main thread.
  content::RenderThread* render_thread = content::RenderThread::Get();
  if (!render_thread)
    return false;
  if (!g_pnacl_resource_host.Get()) {
    g_pnacl_resource_host.Get() = new PnaclTranslationResourceHost(
        render_thread->GetIOMessageLoopProxy());
    render_thread->AddFilter(g_pnacl_resource_host.Get());
  }
  return true;
}

struct InstanceInfo {
  InstanceInfo() : plugin_pid(base::kNullProcessId), plugin_child_id(0) {}
  GURL url;
  ppapi::PpapiPermissions permissions;
  base::ProcessId plugin_pid;
  int plugin_child_id;
  IPC::ChannelHandle channel_handle;
};

typedef std::map<PP_Instance, InstanceInfo> InstanceInfoMap;

base::LazyInstance<InstanceInfoMap> g_instance_info =
    LAZY_INSTANCE_INITIALIZER;

typedef std::map<PP_Instance, nacl::TrustedPluginChannel*>
    InstanceTrustedChannelMap;

base::LazyInstance<InstanceTrustedChannelMap> g_channel_map =
    LAZY_INSTANCE_INITIALIZER;

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

static int GetRoutingID(PP_Instance instance) {
  // Check that we are on the main renderer thread.
  DCHECK(content::RenderThread::Get());
  content::RendererPpapiHost *host =
      content::RendererPpapiHost::GetForPPInstance(instance);
  if (!host)
    return 0;
  return host->GetRoutingIDForWidget(instance);
}

// Launch NaCl's sel_ldr process.
void LaunchSelLdr(PP_Instance instance,
                  const char* alleged_url,
                  PP_Bool uses_irt,
                  PP_Bool uses_ppapi,
                  PP_Bool uses_nonsfi_mode,
                  PP_Bool enable_ppapi_dev,
                  PP_Bool enable_dyncode_syscalls,
                  PP_Bool enable_exception_handling,
                  PP_Bool enable_crash_throttling,
                  void* imc_handle,
                  struct PP_Var* error_message,
                  PP_CompletionCallback callback) {
  CHECK(ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->
            BelongsToCurrentThread());

  nacl::FileDescriptor result_socket;
  IPC::Sender* sender = content::RenderThread::Get();
  DCHECK(sender);
  *error_message = PP_MakeUndefined();
  int routing_id = 0;
  // If the nexe uses ppapi APIs, we need a routing ID.
  // To get the routing ID, we must be on the main thread.
  // Some nexes do not use ppapi and launch from the background thread,
  // so those nexes can skip finding a routing_id.
  if (uses_ppapi) {
    routing_id = GetRoutingID(instance);
    if (!routing_id) {
      ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
          FROM_HERE,
          base::Bind(callback.func, callback.user_data,
                     static_cast<int32_t>(PP_ERROR_FAILED)));
      return;
    }
  }

  InstanceInfo instance_info;
  instance_info.url = GURL(alleged_url);

  uint32_t perm_bits = ppapi::PERMISSION_NONE;
  // Conditionally block 'Dev' interfaces. We do this for the NaCl process, so
  // it's clearer to developers when they are using 'Dev' inappropriately. We
  // must also check on the trusted side of the proxy.
  if (enable_ppapi_dev)
    perm_bits |= ppapi::PERMISSION_DEV;
  instance_info.permissions =
      ppapi::PpapiPermissions::GetForCommandLine(perm_bits);
  std::string error_message_string;
  nacl::NaClLaunchResult launch_result;

  if (!sender->Send(new NaClHostMsg_LaunchNaCl(
          nacl::NaClLaunchParams(instance_info.url.spec(),
                                 routing_id,
                                 perm_bits,
                                 PP_ToBool(uses_irt),
                                 PP_ToBool(uses_nonsfi_mode),
                                 PP_ToBool(enable_dyncode_syscalls),
                                 PP_ToBool(enable_exception_handling),
                                 PP_ToBool(enable_crash_throttling)),
          &launch_result,
          &error_message_string))) {
    ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
        FROM_HERE,
        base::Bind(callback.func, callback.user_data,
                   static_cast<int32_t>(PP_ERROR_FAILED)));
    return;
  }
  if (!error_message_string.empty()) {
    *error_message = ppapi::StringVar::StringToPPVar(error_message_string);
    ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
        FROM_HERE,
        base::Bind(callback.func, callback.user_data,
                   static_cast<int32_t>(PP_ERROR_FAILED)));
    return;
  }
  result_socket = launch_result.imc_channel_handle;
  instance_info.channel_handle = launch_result.ppapi_ipc_channel_handle;
  instance_info.plugin_pid = launch_result.plugin_pid;
  instance_info.plugin_child_id = launch_result.plugin_child_id;
  // Don't save instance_info if channel handle is invalid.
  bool invalid_handle = instance_info.channel_handle.name.empty();
#if defined(OS_POSIX)
  if (!invalid_handle)
    invalid_handle = (instance_info.channel_handle.socket.fd == -1);
#endif
  if (!invalid_handle)
    g_instance_info.Get()[instance] = instance_info;

  // Stash the trusted handle as well.
  invalid_handle = launch_result.trusted_ipc_channel_handle.name.empty();
#if defined(OS_POSIX)
  if (!invalid_handle)
    invalid_handle = (launch_result.trusted_ipc_channel_handle.socket.fd == -1);
#endif
  if (!invalid_handle) {
    g_channel_map.Get()[instance] = new nacl::TrustedPluginChannel(
        launch_result.trusted_ipc_channel_handle, callback,
        content::RenderThread::Get()->GetShutdownEvent());
  }

  *(static_cast<NaClHandle*>(imc_handle)) =
      nacl::ToNativeHandle(result_socket);
}

PP_ExternalPluginResult StartPpapiProxy(PP_Instance instance) {
  InstanceInfoMap& map = g_instance_info.Get();
  InstanceInfoMap::iterator it = map.find(instance);
  if (it == map.end()) {
    DLOG(ERROR) << "Could not find instance ID";
    return PP_EXTERNAL_PLUGIN_FAILED;
  }
  InstanceInfo instance_info = it->second;
  map.erase(it);

  content::PepperPluginInstance* plugin_instance =
      content::PepperPluginInstance::Get(instance);
  if (!plugin_instance) {
    DLOG(ERROR) << "GetInstance() failed";
    return PP_EXTERNAL_PLUGIN_ERROR_MODULE;
  }

  return plugin_instance->SwitchToOutOfProcessProxy(
      base::FilePath().AppendASCII(instance_info.url.spec()),
      instance_info.permissions,
      instance_info.channel_handle,
      instance_info.plugin_pid,
      instance_info.plugin_child_id);
}

int UrandomFD(void) {
#if defined(OS_POSIX)
  return base::GetUrandomFD();
#else
  return -1;
#endif
}

PP_Bool Are3DInterfacesDisabled() {
  return PP_FromBool(CommandLine::ForCurrentProcess()->HasSwitch(
                         switches::kDisable3DAPIs));
}

int32_t BrokerDuplicateHandle(PP_FileHandle source_handle,
                              uint32_t process_id,
                              PP_FileHandle* target_handle,
                              uint32_t desired_access,
                              uint32_t options) {
#if defined(OS_WIN)
  return content::BrokerDuplicateHandle(source_handle, process_id,
                                        target_handle, desired_access,
                                        options);
#else
  return 0;
#endif
}

PP_FileHandle GetReadonlyPnaclFD(const char* filename) {
  IPC::PlatformFileForTransit out_fd = IPC::InvalidPlatformFileForTransit();
  IPC::Sender* sender = content::RenderThread::Get();
  DCHECK(sender);
  if (!sender->Send(new NaClHostMsg_GetReadonlyPnaclFD(
          std::string(filename),
          &out_fd))) {
    return base::kInvalidPlatformFileValue;
  }
  if (out_fd == IPC::InvalidPlatformFileForTransit()) {
    return base::kInvalidPlatformFileValue;
  }
  base::PlatformFile handle =
      IPC::PlatformFileForTransitToPlatformFile(out_fd);
  return handle;
}

PP_FileHandle CreateTemporaryFile(PP_Instance instance) {
  IPC::PlatformFileForTransit transit_fd = IPC::InvalidPlatformFileForTransit();
  IPC::Sender* sender = content::RenderThread::Get();
  DCHECK(sender);
  if (!sender->Send(new NaClHostMsg_NaClCreateTemporaryFile(
          &transit_fd))) {
    return base::kInvalidPlatformFileValue;
  }

  if (transit_fd == IPC::InvalidPlatformFileForTransit()) {
    return base::kInvalidPlatformFileValue;
  }

  base::PlatformFile handle = IPC::PlatformFileForTransitToPlatformFile(
      transit_fd);
  return handle;
}

int32_t GetNumberOfProcessors() {
  int32_t num_processors;
  IPC::Sender* sender = content::RenderThread::Get();
  DCHECK(sender);
  if(!sender->Send(new NaClHostMsg_NaClGetNumProcessors(&num_processors))) {
    return 1;
  }
  return num_processors;
}

PP_Bool IsNonSFIModeEnabled() {
#if defined(OS_LINUX)
  return PP_FromBool(CommandLine::ForCurrentProcess()->HasSwitch(
                         switches::kEnableNaClNonSfiMode));
#else
  return PP_FALSE;
#endif
}

int32_t GetNexeFd(PP_Instance instance,
                  const char* pexe_url,
                  uint32_t abi_version,
                  uint32_t opt_level,
                  const char* last_modified,
                  const char* etag,
                  PP_Bool has_no_store_header,
                  const char* sandbox_isa,
                  const char* extra_flags,
                  PP_Bool* is_hit,
                  PP_FileHandle* handle,
                  struct PP_CompletionCallback callback) {
  ppapi::thunk::EnterInstance enter(instance, callback);
  if (enter.failed())
    return enter.retval();
  if (!pexe_url || !last_modified || !etag || !is_hit || !handle)
    return enter.SetResult(PP_ERROR_BADARGUMENT);
  if (!InitializePnaclResourceHost())
    return enter.SetResult(PP_ERROR_FAILED);

  base::Time last_modified_time;
  // If FromString fails, it doesn't touch last_modified_time and we just send
  // the default-constructed null value.
  base::Time::FromString(last_modified, &last_modified_time);

  nacl::PnaclCacheInfo cache_info;
  cache_info.pexe_url = GURL(pexe_url);
  cache_info.abi_version = abi_version;
  cache_info.opt_level = opt_level;
  cache_info.last_modified = last_modified_time;
  cache_info.etag = std::string(etag);
  cache_info.has_no_store_header = PP_ToBool(has_no_store_header);
  cache_info.sandbox_isa = std::string(sandbox_isa);
  cache_info.extra_flags = std::string(extra_flags);

  g_pnacl_resource_host.Get()->RequestNexeFd(
      GetRoutingID(instance),
      instance,
      cache_info,
      is_hit,
      handle,
      enter.callback());

  return enter.SetResult(PP_OK_COMPLETIONPENDING);
}

void ReportTranslationFinished(PP_Instance instance, PP_Bool success) {
  // If the resource host isn't initialized, don't try to do that here.
  // Just return because something is already very wrong.
  if (g_pnacl_resource_host.Get() == NULL)
    return;
  g_pnacl_resource_host.Get()->ReportTranslationFinished(instance, success);
}

PP_FileHandle OpenNaClExecutable(PP_Instance instance,
                                 const char* file_url,
                                 uint64_t* nonce_lo,
                                 uint64_t* nonce_hi) {
  IPC::PlatformFileForTransit out_fd = IPC::InvalidPlatformFileForTransit();
  IPC::Sender* sender = content::RenderThread::Get();
  DCHECK(sender);
  *nonce_lo = 0;
  *nonce_hi = 0;
  base::FilePath file_path;
  if (!sender->Send(
      new NaClHostMsg_OpenNaClExecutable(GetRoutingID(instance),
                                               GURL(file_url),
                                               &out_fd,
                                               nonce_lo,
                                               nonce_hi))) {
    return base::kInvalidPlatformFileValue;
  }

  if (out_fd == IPC::InvalidPlatformFileForTransit()) {
    return base::kInvalidPlatformFileValue;
  }

  base::PlatformFile handle =
      IPC::PlatformFileForTransitToPlatformFile(out_fd);
  return handle;
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

struct ProgressEvent {
  explicit ProgressEvent(PP_Instance instance_param,
                         PP_NaClEventType event_type_param)
      : instance(instance_param),
        event_type(event_type_param),
        length_is_computable(false),
        loaded_bytes(0),
        total_bytes(0) {
  }
  PP_Instance instance;
  PP_NaClEventType event_type;
  std::string resource_url;
  bool length_is_computable;
  uint64_t loaded_bytes;
  uint64_t total_bytes;
};

void DispatchEventOnMainThread(const ProgressEvent &event);

void DispatchEvent(PP_Instance instance,
                   PP_NaClEventType event_type,
                   const char *resource_url,
                   PP_Bool length_is_computable,
                   uint64_t loaded_bytes,
                   uint64_t total_bytes) {
  ProgressEvent p(instance, event_type);
  p.length_is_computable = PP_ToBool(length_is_computable);
  p.loaded_bytes = loaded_bytes;
  p.total_bytes = total_bytes;

  // We have to copy resource_url into our struct manually since we don't have
  // guarantees about the PP_Var lifetime.
  p.resource_url = std::string();
  if (resource_url)
    p.resource_url = std::string(resource_url);

  ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
      FROM_HERE,
      base::Bind(&DispatchEventOnMainThread, p));
}

void DispatchEventOnMainThread(const ProgressEvent &event) {
  content::PepperPluginInstance* plugin_instance =
      content::PepperPluginInstance::Get(event.instance);
  // The instance may have been destroyed after we were scheduled, so just
  // return if it's gone.
  if (!plugin_instance)
    return;

  blink::WebPluginContainer* container = plugin_instance->GetContainer();
  // It's possible that container() is NULL if the plugin has been removed from
  // the DOM (but the PluginInstance is not destroyed yet).
  if (!container)
    return;
  blink::WebFrame* frame = container->element().document().frame();
  if (!frame)
    return;
  v8::HandleScope handle_scope(plugin_instance->GetIsolate());
  v8::Local<v8::Context> context(
      plugin_instance->GetIsolate()->GetCurrentContext());
  if (context.IsEmpty()) {
    // If there's no JavaScript on the stack, we have to make a new Context.
    context = v8::Context::New(plugin_instance->GetIsolate());
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

void SetReadOnlyProperty(PP_Instance instance,
                         struct PP_Var key,
                         struct PP_Var value) {
  content::PepperPluginInstance* plugin_instance =
      content::PepperPluginInstance::Get(instance);
  plugin_instance->SetEmbedProperty(key, value);
}

void ReportLoadError(PP_Instance instance,
                     PP_NaClError error,
                     PP_Bool is_installed) {
  // Check that we are on the main renderer thread.
  DCHECK(content::RenderThread::Get());

  if (error == PP_NACL_ERROR_MANIFEST_PROGRAM_MISSING_ARCH) {
    // A special case: the manifest may otherwise be valid but is missing
    // a program/file compatible with the user's sandbox.
    IPC::Sender* sender = content::RenderThread::Get();
    sender->Send(
        new NaClHostMsg_MissingArchError(GetRoutingID(instance)));
  }
  // TODO(dmichael): Move the following actions here:
  // - Set ready state to DONE.
  // - Set last error string.
  // - Print error message to JavaScript console.

  // Inform JavaScript that loading encountered an error and is complete.
  DispatchEvent(instance, PP_NACL_EVENT_ERROR, NULL, PP_FALSE, 0, 0);
  DispatchEvent(instance, PP_NACL_EVENT_LOADEND, NULL, PP_FALSE, 0, 0);

  HistogramEnumerate("NaCl.LoadStatus.Plugin", error,
                     PP_NACL_ERROR_MAX);
  std::string uma_name = (is_installed == PP_TRUE) ?
                         "NaCl.LoadStatus.Plugin.InstalledApp" :
                         "NaCl.LoadStatus.Plugin.NotInstalledApp";
  HistogramEnumerate(uma_name, error, PP_NACL_ERROR_MAX);
}

void InstanceDestroyed(PP_Instance instance) {
  InstanceTrustedChannelMap& map = g_channel_map.Get();
  InstanceTrustedChannelMap::iterator it = map.find(instance);
  if (it == map.end()) {
    DLOG(ERROR) << "Could not find instance ID";
    return;
  }
  nacl::TrustedPluginChannel* instance_info = it->second;
  map.erase(it);
  delete instance_info;
}

PP_Bool NaClDebugStubEnabled() {
  return PP_FromBool(CommandLine::ForCurrentProcess()->HasSwitch(
                         switches::kEnableNaClDebug));
}

const char* GetSandboxArch() {
  return nacl::GetSandboxArch();
}

PP_UrlSchemeType GetUrlScheme(PP_Var url) {
  scoped_refptr<ppapi::StringVar> url_string = ppapi::StringVar::FromPPVar(url);
  if (!url_string)
    return PP_SCHEME_OTHER;

  GURL gurl(url_string->value());
  if (gurl.SchemeIs("chrome-extension"))
    return PP_SCHEME_CHROME_EXTENSION;
  if (gurl.SchemeIs("data"))
    return PP_SCHEME_DATA;
  return PP_SCHEME_OTHER;
}

const PPB_NaCl_Private nacl_interface = {
  &LaunchSelLdr,
  &StartPpapiProxy,
  &UrandomFD,
  &Are3DInterfacesDisabled,
  &BrokerDuplicateHandle,
  &GetReadonlyPnaclFD,
  &CreateTemporaryFile,
  &GetNumberOfProcessors,
  &IsNonSFIModeEnabled,
  &GetNexeFd,
  &ReportTranslationFinished,
  &OpenNaClExecutable,
  &DispatchEvent,
  &SetReadOnlyProperty,
  &ReportLoadError,
  &InstanceDestroyed,
  &NaClDebugStubEnabled,
  &GetSandboxArch,
  &GetUrlScheme
};

}  // namespace

namespace nacl {

const PPB_NaCl_Private* GetNaClPrivateInterface() {
  return &nacl_interface;
}

}  // namespace nacl

#endif  // DISABLE_NACL
