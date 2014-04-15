// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/renderer/ppb_nacl_private_impl.h"

#include "base/command_line.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "components/nacl/common/nacl_host_messages.h"
#include "components/nacl/common/nacl_messages.h"
#include "components/nacl/common/nacl_switches.h"
#include "components/nacl/common/nacl_types.h"
#include "components/nacl/renderer/nexe_load_manager.h"
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
#include "net/http/http_response_headers.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/private/pp_file_handle.h"
#include "ppapi/native_client/src/trusted/plugin/nacl_entry_points.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/ppapi_permissions.h"
#include "ppapi/shared_impl/ppapi_preferences.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/enter.h"

namespace {

base::LazyInstance<scoped_refptr<PnaclTranslationResourceHost> >
    g_pnacl_resource_host = LAZY_INSTANCE_INITIALIZER;

bool InitializePnaclResourceHost() {
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

typedef base::ScopedPtrHashMap<PP_Instance, nacl::NexeLoadManager>
    NexeLoadManagerMap;

base::LazyInstance<NexeLoadManagerMap> g_load_manager_map =
    LAZY_INSTANCE_INITIALIZER;

nacl::NexeLoadManager* GetNexeLoadManager(PP_Instance instance) {
  NexeLoadManagerMap& map = g_load_manager_map.Get();
  NexeLoadManagerMap::iterator iter = map.find(instance);
  if (iter != map.end())
    return iter->second;
  return NULL;
}

int GetRoutingID(PP_Instance instance) {
  // Check that we are on the main renderer thread.
  DCHECK(content::RenderThread::Get());
  content::RendererPpapiHost *host =
      content::RendererPpapiHost::GetForPPInstance(instance);
  if (!host)
    return 0;
  return host->GetRoutingIDForWidget(instance);
}

// Returns whether the channel_handle is valid or not.
bool IsValidChannelHandle(const IPC::ChannelHandle& channel_handle) {
  if (channel_handle.name.empty()) {
    return false;
  }

#if defined(OS_POSIX)
  if (channel_handle.socket.fd == -1) {
    return false;
  }
#endif

  return true;
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
  if (IsValidChannelHandle(instance_info.channel_handle))
    g_instance_info.Get()[instance] = instance_info;

  *(static_cast<NaClHandle*>(imc_handle)) =
      nacl::ToNativeHandle(result_socket);

  // TODO(hidehiko): We'll add EmbedderServiceChannel here, and it will wait
  // for the connection in parallel with TrustedPluginChannel.
  // Thus, the callback will wait for its second invocation to run callback,
  // then.
  // Note that PP_CompletionCallback is not designed to be called twice or
  // more. Thus, it is necessary to create a function to handle multiple
  // invocation.
  base::Callback<void(int32_t)> completion_callback =
      base::Bind(callback.func, callback.user_data);
  nacl::NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);

  // Stash the trusted handle as well.
  if (load_manager &&
      IsValidChannelHandle(launch_result.trusted_ipc_channel_handle)) {
    scoped_ptr<nacl::TrustedPluginChannel> trusted_plugin_channel(
        new nacl::TrustedPluginChannel(
            launch_result.trusted_ipc_channel_handle,
            completion_callback,
            content::RenderThread::Get()->GetShutdownEvent()));
    load_manager->set_trusted_plugin_channel(trusted_plugin_channel.Pass());
  } else {
    completion_callback.Run(PP_ERROR_FAILED);
  }
}

// Forward declaration.
void ReportLoadError(PP_Instance instance,
                     PP_NaClError error,
                     const char* error_message,
                     const char* console_message);

PP_Bool StartPpapiProxy(PP_Instance instance) {
  InstanceInfoMap& map = g_instance_info.Get();
  InstanceInfoMap::iterator it = map.find(instance);
  if (it == map.end()) {
    DLOG(ERROR) << "Could not find instance ID";
    return PP_FALSE;
  }
  InstanceInfo instance_info = it->second;
  map.erase(it);

  content::PepperPluginInstance* plugin_instance =
      content::PepperPluginInstance::Get(instance);
  if (!plugin_instance) {
    DLOG(ERROR) << "GetInstance() failed";
    return PP_FALSE;
  }

  PP_ExternalPluginResult result = plugin_instance->SwitchToOutOfProcessProxy(
      base::FilePath().AppendASCII(instance_info.url.spec()),
      instance_info.permissions,
      instance_info.channel_handle,
      instance_info.plugin_pid,
      instance_info.plugin_child_id);

  if (result == PP_EXTERNAL_PLUGIN_OK) {
    // Log the amound of time that has passed between the trusted plugin being
    // initialized and the untrusted plugin being initialized.  This is
    // (roughly) the cost of using NaCl, in terms of startup time.
    nacl::NexeLoadManager* load_manager = GetNexeLoadManager(instance);
    if (load_manager)
      load_manager->ReportStartupOverhead();
    return PP_TRUE;
  } else if (result == PP_EXTERNAL_PLUGIN_ERROR_MODULE) {
    ReportLoadError(instance,
                    PP_NACL_ERROR_START_PROXY_MODULE,
                    "could not initialize module.",
                    "could not initialize module.");
  } else if (result == PP_EXTERNAL_PLUGIN_ERROR_INSTANCE) {
    ReportLoadError(instance,
                    PP_NACL_ERROR_START_PROXY_MODULE,
                    "could not create instance.",
                    "could not create instance.");
  }
  return PP_FALSE;
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
                  const char* http_headers_param,
                  const char* extra_flags,
                  PP_Bool* is_hit,
                  PP_FileHandle* handle,
                  struct PP_CompletionCallback callback) {
  ppapi::thunk::EnterInstance enter(instance, callback);
  if (enter.failed())
    return enter.retval();
  if (!pexe_url || !is_hit || !handle)
    return enter.SetResult(PP_ERROR_BADARGUMENT);
  if (!InitializePnaclResourceHost())
    return enter.SetResult(PP_ERROR_FAILED);

  scoped_refptr<net::HttpResponseHeaders> http_headers(
      new net::HttpResponseHeaders(http_headers_param));
  std::string last_modified;
  std::string etag;
  http_headers->EnumerateHeader(NULL, "last-modified", &last_modified);
  http_headers->EnumerateHeader(NULL, "etag", &etag);

  std::string cache_control;
  bool has_no_store_header = false;
  if (http_headers->EnumerateHeader(NULL, "cache-control", &cache_control)) {
    if (cache_control.find("no-store") != std::string::npos)
      has_no_store_header = true;
  }

  base::Time last_modified_time;
  // If FromString fails, it doesn't touch last_modified_time and we just send
  // the default-constructed null value.
  base::Time::FromString(last_modified.c_str(), &last_modified_time);

  nacl::PnaclCacheInfo cache_info;
  cache_info.pexe_url = GURL(pexe_url);
  cache_info.abi_version = abi_version;
  cache_info.opt_level = opt_level;
  cache_info.last_modified = last_modified_time;
  cache_info.etag = etag;
  cache_info.has_no_store_header = has_no_store_header;
  cache_info.sandbox_isa = nacl::GetSandboxArch();
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

void DispatchEventOnMainThread(PP_Instance instance,
                               PP_NaClEventType event_type,
                               const std::string& resource_url,
                               PP_Bool length_is_computable,
                               uint64_t loaded_bytes,
                               uint64_t total_bytes);

void DispatchEvent(PP_Instance instance,
                   PP_NaClEventType event_type,
                   const char *resource_url,
                   PP_Bool length_is_computable,
                   uint64_t loaded_bytes,
                   uint64_t total_bytes) {
  ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
      FROM_HERE,
      base::Bind(&DispatchEventOnMainThread,
                 instance,
                 event_type,
                 std::string(resource_url),
                 length_is_computable,
                 loaded_bytes,
                 total_bytes));
}

void DispatchEventOnMainThread(PP_Instance instance,
                               PP_NaClEventType event_type,
                               const std::string& resource_url,
                               PP_Bool length_is_computable,
                               uint64_t loaded_bytes,
                               uint64_t total_bytes) {
  nacl::NexeLoadManager* load_manager =
      GetNexeLoadManager(instance);
  // The instance may have been destroyed after we were scheduled, so do
  // nothing if it's gone.
  if (load_manager) {
    nacl::NexeLoadManager::ProgressEvent event(event_type);
    event.resource_url = resource_url;
    event.length_is_computable = PP_ToBool(length_is_computable);
    event.loaded_bytes = loaded_bytes;
    event.total_bytes = total_bytes;
    load_manager->DispatchEvent(event);
  }
}

void NexeFileDidOpen(PP_Instance instance,
                     int32_t pp_error,
                     int32_t fd,
                     int32_t http_status,
                     int64_t nexe_bytes_read,
                     const char* url,
                     int64_t time_since_open) {
  nacl::NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  if (load_manager) {
    load_manager->NexeFileDidOpen(pp_error,
                                  fd,
                                  http_status,
                                  nexe_bytes_read,
                                  url,
                                  time_since_open);
  }
}

void ReportLoadSuccess(PP_Instance instance,
                       PP_Bool is_pnacl,
                       const char* url,
                       uint64_t loaded_bytes,
                       uint64_t total_bytes) {
  nacl::NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  if (load_manager)
    load_manager->ReportLoadSuccess(
        PP_ToBool(is_pnacl), url, loaded_bytes, total_bytes);
}

void ReportLoadError(PP_Instance instance,
                     PP_NaClError error,
                     const char* error_message,
                     const char* console_message) {
  nacl::NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  if (load_manager)
    load_manager->ReportLoadError(error, error_message, console_message);
}

void ReportLoadAbort(PP_Instance instance) {
  nacl::NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  if (load_manager)
    load_manager->ReportLoadAbort();
}

void NexeDidCrash(PP_Instance instance, const char* crash_log) {
  nacl::NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  if (load_manager)
    load_manager->NexeDidCrash(crash_log);
}

void InstanceCreated(PP_Instance instance) {
  scoped_ptr<nacl::NexeLoadManager> new_load_manager(
      new nacl::NexeLoadManager(instance));
  NexeLoadManagerMap& map = g_load_manager_map.Get();
  DLOG_IF(ERROR, map.count(instance) != 0) << "Instance count should be 0";
  map.add(instance, new_load_manager.Pass());
}

void InstanceDestroyed(PP_Instance instance) {
  NexeLoadManagerMap& map = g_load_manager_map.Get();
  DLOG_IF(ERROR, map.count(instance) == 0) << "Could not find instance ID";
  // The erase may call NexeLoadManager's destructor prior to removing it from
  // the map. In that case, it is possible for the trusted Plugin to re-enter
  // the NexeLoadManager (e.g., by calling ReportLoadError). Passing out the
  // NexeLoadManager to a local scoped_ptr just ensures that its entry is gone
  // from the map prior to the destructor being invoked.
  scoped_ptr<nacl::NexeLoadManager> temp(map.take(instance));
  map.erase(instance);
}

PP_Bool NaClDebugEnabledForURL(const char* alleged_nmf_url) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableNaClDebug))
    return PP_FALSE;
  bool should_debug;
  IPC::Sender* sender = content::RenderThread::Get();
  DCHECK(sender);
  if(!sender->Send(new NaClHostMsg_NaClDebugEnabledForURL(
         GURL(alleged_nmf_url),
         &should_debug))) {
    return PP_FALSE;
  }
  return PP_FromBool(should_debug);
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

void LogToConsole(PP_Instance instance, const char* message) {
  nacl::NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (load_manager)
    load_manager->LogToConsole(std::string(message));
}

PP_NaClReadyState GetNaClReadyState(PP_Instance instance) {
  nacl::NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (load_manager)
    return load_manager->nacl_ready_state();
  return PP_NACL_READY_STATE_UNSENT;
}

void SetNaClReadyState(PP_Instance instance, PP_NaClReadyState ready_state) {
  nacl::NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (load_manager)
    load_manager->set_nacl_ready_state(ready_state);
}

PP_Bool GetIsInstalled(PP_Instance instance) {
  nacl::NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (load_manager)
    return PP_FromBool(load_manager->is_installed());
  return PP_FALSE;
}

void SetIsInstalled(PP_Instance instance, PP_Bool installed) {
  nacl::NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (load_manager)
    load_manager->set_is_installed(PP_ToBool(installed));
}

int32_t GetExitStatus(PP_Instance instance) {
  nacl::NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (load_manager)
    return load_manager->exit_status();
  return -1;
}

void SetExitStatus(PP_Instance instance, int32_t exit_status) {
  nacl::NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (load_manager)
    return load_manager->set_exit_status(exit_status);
}

void Vlog(const char* message) {
  VLOG(1) << message;
}

void SetInitTime(PP_Instance instance) {
  nacl::NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (load_manager)
    return load_manager->set_init_time();
}

int64_t GetNexeSize(PP_Instance instance) {
  nacl::NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (load_manager)
    return load_manager->nexe_size();
  return 0;
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
  &NexeFileDidOpen,
  &ReportLoadSuccess,
  &ReportLoadError,
  &ReportLoadAbort,
  &NexeDidCrash,
  &InstanceCreated,
  &InstanceDestroyed,
  &NaClDebugEnabledForURL,
  &GetSandboxArch,
  &GetUrlScheme,
  &LogToConsole,
  &GetNaClReadyState,
  &SetNaClReadyState,
  &GetIsInstalled,
  &SetIsInstalled,
  &GetExitStatus,
  &SetExitStatus,
  &Vlog,
  &SetInitTime,
  &GetNexeSize
};

}  // namespace

namespace nacl {

const PPB_NaCl_Private* GetNaClPrivateInterface() {
  return &nacl_interface;
}

}  // namespace nacl
