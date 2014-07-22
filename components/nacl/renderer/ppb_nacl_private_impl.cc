// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/renderer/ppb_nacl_private_impl.h"

#include <numeric>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/cpu.h"
#include "base/files/file.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "components/nacl/common/nacl_host_messages.h"
#include "components/nacl/common/nacl_messages.h"
#include "components/nacl/common/nacl_nonsfi_util.h"
#include "components/nacl/common/nacl_switches.h"
#include "components/nacl/common/nacl_types.h"
#include "components/nacl/renderer/file_downloader.h"
#include "components/nacl/renderer/histogram.h"
#include "components/nacl/renderer/json_manifest.h"
#include "components/nacl/renderer/manifest_downloader.h"
#include "components/nacl/renderer/manifest_service_channel.h"
#include "components/nacl/renderer/nexe_load_manager.h"
#include "components/nacl/renderer/platform_info.h"
#include "components/nacl/renderer/pnacl_translation_resource_host.h"
#include "components/nacl/renderer/progress_event.h"
#include "components/nacl/renderer/trusted_plugin_channel.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/sandbox_init.h"
#include "content/public/renderer/pepper_plugin_instance.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "native_client/src/public/imc_types.h"
#include "net/base/data_url.h"
#include "net/base/net_errors.h"
#include "net/http/http_util.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/private/pp_file_handle.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/ppapi_permissions.h"
#include "ppapi/shared_impl/ppapi_preferences.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/var_tracker.h"
#include "ppapi/thunk/enter.h"
#include "third_party/WebKit/public/platform/WebURLLoader.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebURLLoaderOptions.h"
#include "third_party/jsoncpp/source/include/json/reader.h"
#include "third_party/jsoncpp/source/include/json/value.h"

namespace nacl {
namespace {

// The pseudo-architecture used to indicate portable native client.
const char* const kPortableArch = "portable";

// The base URL for resources used by the PNaCl translator processes.
const char* kPNaClTranslatorBaseUrl = "chrome://pnacl-translator/";

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

typedef base::ScopedPtrHashMap<PP_Instance, NexeLoadManager>
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

static const PP_NaClFileInfo kInvalidNaClFileInfo = {
    PP_kInvalidFileHandle,
    0,  // token_lo
    0,  // token_hi
};

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

void PostPPCompletionCallback(PP_CompletionCallback callback,
                              int32_t status) {
  ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
      FROM_HERE,
      base::Bind(callback.func, callback.user_data, status));
}

bool ManifestResolveKey(PP_Instance instance,
                        bool is_helper_process,
                        const std::string& key,
                        std::string* full_url,
                        PP_PNaClOptions* pnacl_options);

typedef base::Callback<void(int32_t, const PP_NaClFileInfo&)>
DownloadFileCallback;

void DownloadFile(PP_Instance instance,
                  const std::string& url,
                  const DownloadFileCallback& callback);

PP_Bool StartPpapiProxy(PP_Instance instance);

// Thin adapter from PPP_ManifestService to ManifestServiceChannel::Delegate.
// Note that user_data is managed by the caller of LaunchSelLdr. Please see
// also PP_ManifestService's comment for more details about resource
// management.
class ManifestServiceProxy : public ManifestServiceChannel::Delegate {
 public:
  ManifestServiceProxy(PP_Instance pp_instance)
      : pp_instance_(pp_instance) {
  }

  virtual ~ManifestServiceProxy() { }

  virtual void StartupInitializationComplete() OVERRIDE {
    if (StartPpapiProxy(pp_instance_) == PP_TRUE) {
      JsonManifest* manifest = GetJsonManifest(pp_instance_);
      NexeLoadManager* load_manager = GetNexeLoadManager(pp_instance_);
      if (load_manager && manifest) {
        std::string full_url;
        PP_PNaClOptions pnacl_options;
        bool uses_nonsfi_mode;
        JsonManifest::ErrorInfo error_info;
        if (manifest->GetProgramURL(&full_url,
                                    &pnacl_options,
                                    &uses_nonsfi_mode,
                                    &error_info)) {
          int64_t nexe_size = load_manager->nexe_size();
          load_manager->ReportLoadSuccess(full_url, nexe_size, nexe_size);
        }
      }
    }
  }

  virtual void OpenResource(
      const std::string& key,
      const ManifestServiceChannel::OpenResourceCallback& callback) OVERRIDE {
    DCHECK(ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->
               BelongsToCurrentThread());

    std::string url;
    // TODO(teravest): Clean up pnacl_options logic in JsonManifest so we don't
    // have to initialize it like this here.
    PP_PNaClOptions pnacl_options;
    pnacl_options.translate = PP_FALSE;
    pnacl_options.is_debug = PP_FALSE;
    pnacl_options.opt_level = 2;
    if (!ManifestResolveKey(pp_instance_, false, key, &url, &pnacl_options)) {
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(callback, base::Passed(base::File())));
      return;
    }

    // We have to call DidDownloadFile, even if this object is destroyed, so
    // that the handle inside PP_NaClFileInfo isn't leaked. This means that the
    // callback passed to this function shouldn't have a weak pointer to an
    // object either.
    //
    // TODO(teravest): Make a type like PP_NaClFileInfo to use for DownloadFile
    // that would close the file handle on destruction.
    DownloadFile(pp_instance_, url,
                 base::Bind(&ManifestServiceProxy::DidDownloadFile, callback));
  }

 private:
  static void DidDownloadFile(
      ManifestServiceChannel::OpenResourceCallback callback,
      int32_t pp_error,
      const PP_NaClFileInfo& file_info) {
    if (pp_error != PP_OK) {
      callback.Run(base::File());
      return;
    }
    callback.Run(base::File(file_info.handle));
  }

  PP_Instance pp_instance_;
  DISALLOW_COPY_AND_ASSIGN(ManifestServiceProxy);
};

blink::WebURLLoader* CreateWebURLLoader(const blink::WebDocument& document,
                                        const GURL& gurl) {
  blink::WebURLLoaderOptions options;
  options.untrustedHTTP = true;

  // Options settings here follow the original behavior in the trusted
  // plugin and PepperURLLoaderHost.
  if (document.securityOrigin().canRequest(gurl)) {
    options.allowCredentials = true;
  } else {
    // Allow CORS.
    options.crossOriginRequestPolicy =
        blink::WebURLLoaderOptions::CrossOriginRequestPolicyUseAccessControl;
  }
  return document.frame()->createAssociatedURLLoader(options);
}

blink::WebURLRequest CreateWebURLRequest(const blink::WebDocument& document,
                                         const GURL& gurl) {
  blink::WebURLRequest request;
  request.initialize();
  request.setURL(gurl);
  request.setFirstPartyForCookies(document.firstPartyForCookies());
  return request;
}

int32_t FileDownloaderToPepperError(FileDownloader::Status status) {
  switch (status) {
    case FileDownloader::SUCCESS:
      return PP_OK;
    case FileDownloader::ACCESS_DENIED:
      return PP_ERROR_NOACCESS;
    case FileDownloader::FAILED:
      return PP_ERROR_FAILED;
    // No default case, to catch unhandled Status values.
  }
  return PP_ERROR_FAILED;
}

// Launch NaCl's sel_ldr process.
void LaunchSelLdr(PP_Instance instance,
                  PP_Bool main_service_runtime,
                  const char* alleged_url,
                  const PP_NaClFileInfo* nexe_file_info,
                  PP_Bool uses_irt,
                  PP_Bool uses_ppapi,
                  PP_Bool uses_nonsfi_mode,
                  PP_Bool enable_ppapi_dev,
                  PP_Bool enable_dyncode_syscalls,
                  PP_Bool enable_exception_handling,
                  PP_Bool enable_crash_throttling,
                  void* imc_handle,
                  PP_CompletionCallback callback) {
  CHECK(ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->
            BelongsToCurrentThread());

  // Create the manifest service proxy here, so on error case, it will be
  // destructed (without passing it to ManifestServiceChannel).
  scoped_ptr<ManifestServiceChannel::Delegate> manifest_service_proxy(
      new ManifestServiceProxy(instance));

  FileDescriptor result_socket;
  IPC::Sender* sender = content::RenderThread::Get();
  DCHECK(sender);
  int routing_id = 0;
  // If the nexe uses ppapi APIs, we need a routing ID.
  // To get the routing ID, we must be on the main thread.
  // Some nexes do not use ppapi and launch from the background thread,
  // so those nexes can skip finding a routing_id.
  if (uses_ppapi) {
    routing_id = GetRoutingID(instance);
    if (!routing_id) {
      if (nexe_file_info->handle != PP_kInvalidFileHandle) {
        base::File closer(nexe_file_info->handle);
      }
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
  NaClLaunchResult launch_result;

  content::RendererPpapiHost* host =
      content::RendererPpapiHost::GetForPPInstance(instance);
  if (!sender->Send(new NaClHostMsg_LaunchNaCl(
          NaClLaunchParams(
              instance_info.url.spec(),
              host->ShareHandleWithRemote(nexe_file_info->handle, true),
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
    if (PP_ToBool(main_service_runtime)) {
      NexeLoadManager* load_manager = GetNexeLoadManager(instance);
      if (load_manager) {
        load_manager->ReportLoadError(PP_NACL_ERROR_SEL_LDR_LAUNCH,
                                      "ServiceRuntime: failed to start",
                                      error_message_string);
      }
    }
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

  *(static_cast<NaClHandle*>(imc_handle)) = ToNativeHandle(result_socket);

  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (!load_manager) {
    PostPPCompletionCallback(callback, PP_ERROR_FAILED);
    return;
  }

  // Create the trusted plugin channel.
  if (IsValidChannelHandle(launch_result.trusted_ipc_channel_handle)) {
    scoped_ptr<TrustedPluginChannel> trusted_plugin_channel(
        new TrustedPluginChannel(
            launch_result.trusted_ipc_channel_handle));
    load_manager->set_trusted_plugin_channel(trusted_plugin_channel.Pass());
  } else {
    PostPPCompletionCallback(callback, PP_ERROR_FAILED);
    return;
  }

  // Create the manifest service handle as well.
  // For security hardening, disable the IPCs for open_resource() when they
  // aren't needed.  PNaCl doesn't expose open_resource(), and the new
  // open_resource() IPCs are currently only used for Non-SFI NaCl so far,
  // not SFI NaCl. Note that enable_dyncode_syscalls is true if and only if
  // the plugin is a non-PNaCl plugin.
  if (load_manager &&
      enable_dyncode_syscalls &&
      uses_nonsfi_mode &&
      IsValidChannelHandle(
          launch_result.manifest_service_ipc_channel_handle)) {
    scoped_ptr<ManifestServiceChannel> manifest_service_channel(
        new ManifestServiceChannel(
            launch_result.manifest_service_ipc_channel_handle,
            base::Bind(&PostPPCompletionCallback, callback),
            manifest_service_proxy.Pass(),
            content::RenderThread::Get()->GetShutdownEvent()));
    load_manager->set_manifest_service_channel(
        manifest_service_channel.Pass());
  } else {
    // Currently, manifest service works only on linux/non-SFI mode.
    // On other platforms, the socket will not be created, and thus this
    // condition needs to be handled as success.
    PostPPCompletionCallback(callback, PP_OK);
  }
}

PP_Bool StartPpapiProxy(PP_Instance instance) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (!load_manager)
    return PP_FALSE;

  content::PepperPluginInstance* plugin_instance =
      content::PepperPluginInstance::Get(instance);
  if (!plugin_instance) {
    DLOG(ERROR) << "GetInstance() failed";
    return PP_FALSE;
  }

  InstanceInfoMap& map = g_instance_info.Get();
  InstanceInfoMap::iterator it = map.find(instance);
  if (it == map.end()) {
    DLOG(ERROR) << "Could not find instance ID";
    return PP_FALSE;
  }
  InstanceInfo instance_info = it->second;
  map.erase(it);

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
    load_manager->ReportStartupOverhead();
    return PP_TRUE;
  } else if (result == PP_EXTERNAL_PLUGIN_ERROR_MODULE) {
    load_manager->ReportLoadError(PP_NACL_ERROR_START_PROXY_MODULE,
                                  "could not initialize module.");
  } else if (result == PP_EXTERNAL_PLUGIN_ERROR_INSTANCE) {
    load_manager->ReportLoadError(PP_NACL_ERROR_START_PROXY_MODULE,
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

// Convert a URL to a filename for GetReadonlyPnaclFd.
// Must be kept in sync with PnaclCanOpenFile() in
// components/nacl/browser/nacl_file_host.cc.
std::string PnaclComponentURLToFilename(const std::string& url) {
  // PNaCl component URLs aren't arbitrary URLs; they are always either
  // generated from ManifestResolveKey or PnaclResources::ReadResourceInfo.
  // So, it's safe to just use string parsing operations here instead of
  // URL-parsing ones.
  DCHECK(StartsWithASCII(url, kPNaClTranslatorBaseUrl, true));
  std::string r = url.substr(std::string(kPNaClTranslatorBaseUrl).length());

  // Use white-listed-chars.
  size_t replace_pos;
  static const char* white_list = "abcdefghijklmnopqrstuvwxyz0123456789_";
  replace_pos = r.find_first_not_of(white_list);
  while(replace_pos != std::string::npos) {
    r = r.replace(replace_pos, 1, "_");
    replace_pos = r.find_first_not_of(white_list);
  }
  return r;
}

PP_FileHandle GetReadonlyPnaclFd(const char* url,
                                 bool is_executable,
                                 uint64_t* nonce_lo,
                                 uint64_t* nonce_hi) {
  std::string filename = PnaclComponentURLToFilename(url);
  IPC::PlatformFileForTransit out_fd = IPC::InvalidPlatformFileForTransit();
  IPC::Sender* sender = content::RenderThread::Get();
  DCHECK(sender);
  if (!sender->Send(new NaClHostMsg_GetReadonlyPnaclFD(
          std::string(filename), is_executable,
          &out_fd, nonce_lo, nonce_hi))) {
    return PP_kInvalidFileHandle;
  }
  if (out_fd == IPC::InvalidPlatformFileForTransit()) {
    return PP_kInvalidFileHandle;
  }
  return IPC::PlatformFileForTransitToPlatformFile(out_fd);
}

void GetReadExecPnaclFd(const char* url,
                        PP_NaClFileInfo* out_file_info) {
  *out_file_info = kInvalidNaClFileInfo;
  out_file_info->handle = GetReadonlyPnaclFd(url, true /* is_executable */,
                                             &out_file_info->token_lo,
                                             &out_file_info->token_hi);
}

PP_FileHandle CreateTemporaryFile(PP_Instance instance) {
  IPC::PlatformFileForTransit transit_fd = IPC::InvalidPlatformFileForTransit();
  IPC::Sender* sender = content::RenderThread::Get();
  DCHECK(sender);
  if (!sender->Send(new NaClHostMsg_NaClCreateTemporaryFile(
          &transit_fd))) {
    return PP_kInvalidFileHandle;
  }

  if (transit_fd == IPC::InvalidPlatformFileForTransit()) {
    return PP_kInvalidFileHandle;
  }

  return IPC::PlatformFileForTransitToPlatformFile(transit_fd);
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

PP_Bool PPIsNonSFIModeEnabled() {
  return PP_FromBool(IsNonSFIModeEnabled());
}

void GetNexeFdContinuation(scoped_refptr<ppapi::TrackedCallback> callback,
                           PP_Bool* out_is_hit,
                           PP_FileHandle* out_handle,
                           int32_t pp_error,
                           bool is_hit,
                           PP_FileHandle handle) {
  if (pp_error == PP_OK) {
    *out_is_hit = PP_FromBool(is_hit);
    *out_handle = handle;
  }
  callback->PostRun(pp_error);
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

  std::string http_headers(http_headers_param);
  net::HttpUtil::HeadersIterator iter(
      http_headers.begin(), http_headers.end(), "\r\n");

  std::string last_modified;
  std::string etag;
  bool has_no_store_header = false;
  while (iter.GetNext()) {
    if (StringToLowerASCII(iter.name()) == "last-modified")
      last_modified = iter.values();
    if (StringToLowerASCII(iter.name()) == "etag")
      etag = iter.values();
    if (StringToLowerASCII(iter.name()) == "cache-control") {
      net::HttpUtil::ValuesIterator values_iter(
          iter.values_begin(), iter.values_end(), ',');
      while (values_iter.GetNext()) {
        if (StringToLowerASCII(values_iter.value()) == "no-store")
          has_no_store_header = true;
      }
    }
  }

  base::Time last_modified_time;
  // If FromString fails, it doesn't touch last_modified_time and we just send
  // the default-constructed null value.
  base::Time::FromString(last_modified.c_str(), &last_modified_time);

  PnaclCacheInfo cache_info;
  cache_info.pexe_url = GURL(pexe_url);
  cache_info.abi_version = abi_version;
  cache_info.opt_level = opt_level;
  cache_info.last_modified = last_modified_time;
  cache_info.etag = etag;
  cache_info.has_no_store_header = has_no_store_header;
  cache_info.sandbox_isa = GetSandboxArch();
  cache_info.extra_flags = std::string(extra_flags);

  g_pnacl_resource_host.Get()->RequestNexeFd(
      GetRoutingID(instance),
      instance,
      cache_info,
      base::Bind(&GetNexeFdContinuation, enter.callback(), is_hit, handle));

  return enter.SetResult(PP_OK_COMPLETIONPENDING);
}

void ReportTranslationFinished(PP_Instance instance,
                               PP_Bool success,
                               int32_t opt_level,
                               int64_t pexe_size,
                               int64_t compile_time_us) {
  if (success == PP_TRUE) {
    static const int32_t kUnknownOptLevel = 4;
    if (opt_level < 0 || opt_level > 3)
      opt_level = kUnknownOptLevel;
    HistogramEnumerate("NaCl.Options.PNaCl.OptLevel",
                       opt_level,
                       kUnknownOptLevel + 1);
    HistogramKBPerSec("NaCl.Perf.PNaClLoadTime.CompileKBPerSec",
                      pexe_size / 1024,
                      compile_time_us);
    HistogramSizeKB("NaCl.Perf.Size.Pexe", pexe_size / 1024);

    NexeLoadManager* load_manager = GetNexeLoadManager(instance);
    if (load_manager) {
      base::TimeDelta total_time = base::Time::Now() -
                                   load_manager->pnacl_start_time();
      HistogramTimeTranslation("NaCl.Perf.PNaClLoadTime.TotalUncachedTime",
                               total_time.InMilliseconds());
      HistogramKBPerSec("NaCl.Perf.PNaClLoadTime.TotalUncachedKBPerSec",
                        pexe_size / 1024,
                        total_time.InMicroseconds());
    }
  }

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
  // Fast path only works for installed file URLs.
  GURL gurl(file_url);
  if (!gurl.SchemeIs("chrome-extension"))
    return PP_kInvalidFileHandle;

  content::PepperPluginInstance* plugin_instance =
      content::PepperPluginInstance::Get(instance);
  if (!plugin_instance)
    return PP_kInvalidFileHandle;
  // IMPORTANT: Make sure the document can request the given URL. If we don't
  // check, a malicious app could probe the extension system. This enforces a
  // same-origin policy which prevents the app from requesting resources from
  // another app.
  blink::WebSecurityOrigin security_origin =
      plugin_instance->GetContainer()->element().document().securityOrigin();
  if (!security_origin.canRequest(gurl))
    return PP_kInvalidFileHandle;

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
    return PP_kInvalidFileHandle;
  }

  if (out_fd == IPC::InvalidPlatformFileForTransit())
    return PP_kInvalidFileHandle;

  return IPC::PlatformFileForTransitToPlatformFile(out_fd);
}

void DispatchEvent(PP_Instance instance,
                   PP_NaClEventType event_type,
                   const char *resource_url,
                   PP_Bool length_is_computable,
                   uint64_t loaded_bytes,
                   uint64_t total_bytes) {
  ProgressEvent event(event_type,
                      resource_url,
                      PP_ToBool(length_is_computable),
                      loaded_bytes,
                      total_bytes);
  DispatchProgressEvent(instance, event);
}

void ReportLoadSuccess(PP_Instance instance,
                       uint64_t loaded_bytes,
                       uint64_t total_bytes) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  if (load_manager) {
    load_manager->ReportLoadSuccess(load_manager->program_url(),
                                    loaded_bytes,
                                    total_bytes);
  }
}

void ReportLoadError(PP_Instance instance,
                     PP_NaClError error,
                     const char* error_message) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  if (load_manager)
    load_manager->ReportLoadError(error, error_message);
}

void ReportLoadAbort(PP_Instance instance) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  if (load_manager)
    load_manager->ReportLoadAbort();
}

void NexeDidCrash(PP_Instance instance, const char* crash_log) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  if (load_manager)
    load_manager->NexeDidCrash(crash_log);
}

void InstanceCreated(PP_Instance instance) {
  scoped_ptr<NexeLoadManager> new_load_manager(new NexeLoadManager(instance));
  NexeLoadManagerMap& map = g_load_manager_map.Get();
  DLOG_IF(ERROR, map.count(instance) != 0) << "Instance count should be 0";
  map.add(instance, new_load_manager.Pass());
}

void InstanceDestroyed(PP_Instance instance) {
  DeleteJsonManifest(instance);

  NexeLoadManagerMap& map = g_load_manager_map.Get();
  DLOG_IF(ERROR, map.count(instance) == 0) << "Could not find instance ID";
  // The erase may call NexeLoadManager's destructor prior to removing it from
  // the map. In that case, it is possible for the trusted Plugin to re-enter
  // the NexeLoadManager (e.g., by calling ReportLoadError). Passing out the
  // NexeLoadManager to a local scoped_ptr just ensures that its entry is gone
  // from the map prior to the destructor being invoked.
  scoped_ptr<NexeLoadManager> temp(map.take(instance));
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

void LogToConsole(PP_Instance instance, const char* message) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (load_manager)
    load_manager->LogToConsole(std::string(message));
}

PP_NaClReadyState GetNaClReadyState(PP_Instance instance) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (load_manager)
    return load_manager->nacl_ready_state();
  return PP_NACL_READY_STATE_UNSENT;
}

int32_t GetExitStatus(PP_Instance instance) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (load_manager)
    return load_manager->exit_status();
  return -1;
}

void SetExitStatus(PP_Instance instance, int32_t exit_status) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (load_manager)
    return load_manager->set_exit_status(exit_status);
}

void Vlog(const char* message) {
  VLOG(1) << message;
}

void InitializePlugin(PP_Instance instance,
                      uint32_t argc,
                      const char* argn[],
                      const char* argv[]) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (load_manager)
    load_manager->InitializePlugin(argc, argn, argv);
}

int64_t GetNexeSize(PP_Instance instance) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (load_manager)
    return load_manager->nexe_size();
  return 0;
}

void DownloadManifestToBuffer(PP_Instance instance,
                              struct PP_CompletionCallback callback);

bool CreateJsonManifest(PP_Instance instance,
                        const std::string& manifest_url,
                        const std::string& manifest_data);

void RequestNaClManifest(PP_Instance instance,
                         PP_CompletionCallback callback) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (!load_manager) {
    ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
        FROM_HERE,
        base::Bind(callback.func, callback.user_data,
                   static_cast<int32_t>(PP_ERROR_FAILED)));
    return;
  }

  std::string url = load_manager->GetManifestURLArgument();
  if (url.empty() || !load_manager->RequestNaClManifest(url)) {
    ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
        FROM_HERE,
        base::Bind(callback.func, callback.user_data,
                   static_cast<int32_t>(PP_ERROR_FAILED)));
    return;
  }

  const GURL& base_url = load_manager->manifest_base_url();
  if (base_url.SchemeIs("data")) {
    GURL gurl(base_url);
    std::string mime_type;
    std::string charset;
    std::string data;
    int32_t error = PP_ERROR_FAILED;
    if (net::DataURL::Parse(gurl, &mime_type, &charset, &data)) {
      if (data.size() <= ManifestDownloader::kNaClManifestMaxFileBytes) {
        if (CreateJsonManifest(instance, base_url.spec(), data))
          error = PP_OK;
      } else {
        load_manager->ReportLoadError(PP_NACL_ERROR_MANIFEST_TOO_LARGE,
                                      "manifest file too large.");
      }
    } else {
      load_manager->ReportLoadError(PP_NACL_ERROR_MANIFEST_LOAD_URL,
                                    "could not load manifest url.");
    }
    ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
        FROM_HERE,
        base::Bind(callback.func, callback.user_data, error));
  } else {
    DownloadManifestToBuffer(instance, callback);
  }
}

PP_Var GetManifestBaseURL(PP_Instance instance) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (!load_manager)
    return PP_MakeUndefined();
  const GURL& gurl = load_manager->manifest_base_url();
  if (!gurl.is_valid())
    return PP_MakeUndefined();
  return ppapi::StringVar::StringToPPVar(gurl.spec());
}

void ProcessNaClManifest(PP_Instance instance, const char* program_url) {
  nacl::NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  if (load_manager)
    load_manager->ProcessNaClManifest(program_url);
}

PP_Bool DevInterfacesEnabled(PP_Instance instance) {
  nacl::NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  if (load_manager)
    return PP_FromBool(load_manager->DevInterfacesEnabled());
  return PP_FALSE;
}

void DownloadManifestToBufferCompletion(PP_Instance instance,
                                        struct PP_CompletionCallback callback,
                                        base::Time start_time,
                                        PP_NaClError pp_nacl_error,
                                        const std::string& data);

void DownloadManifestToBuffer(PP_Instance instance,
                              struct PP_CompletionCallback callback) {
  nacl::NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  content::PepperPluginInstance* plugin_instance =
      content::PepperPluginInstance::Get(instance);
  if (!load_manager || !plugin_instance) {
    ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
        FROM_HERE,
        base::Bind(callback.func, callback.user_data,
                   static_cast<int32_t>(PP_ERROR_FAILED)));
  }
  const blink::WebDocument& document =
      plugin_instance->GetContainer()->element().document();

  const GURL& gurl = load_manager->manifest_base_url();
  scoped_ptr<blink::WebURLLoader> url_loader(
      CreateWebURLLoader(document, gurl));
  blink::WebURLRequest request = CreateWebURLRequest(document, gurl);

  // ManifestDownloader deletes itself after invoking the callback.
  ManifestDownloader* manifest_downloader = new ManifestDownloader(
      url_loader.Pass(),
      load_manager->is_installed(),
      base::Bind(DownloadManifestToBufferCompletion,
                 instance, callback, base::Time::Now()));
  manifest_downloader->Load(request);
}

void DownloadManifestToBufferCompletion(PP_Instance instance,
                                        struct PP_CompletionCallback callback,
                                        base::Time start_time,
                                        PP_NaClError pp_nacl_error,
                                        const std::string& data) {
  base::TimeDelta download_time = base::Time::Now() - start_time;
  HistogramTimeSmall("NaCl.Perf.StartupTime.ManifestDownload",
                     download_time.InMilliseconds());

  nacl::NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  if (!load_manager) {
    callback.func(callback.user_data, PP_ERROR_ABORTED);
    return;
  }

  int32_t pp_error;
  switch (pp_nacl_error) {
    case PP_NACL_ERROR_LOAD_SUCCESS:
      pp_error = PP_OK;
      break;
    case PP_NACL_ERROR_MANIFEST_LOAD_URL:
      pp_error = PP_ERROR_FAILED;
      load_manager->ReportLoadError(PP_NACL_ERROR_MANIFEST_LOAD_URL,
                                    "could not load manifest url.");
      break;
    case PP_NACL_ERROR_MANIFEST_TOO_LARGE:
      pp_error = PP_ERROR_FILETOOBIG;
      load_manager->ReportLoadError(PP_NACL_ERROR_MANIFEST_TOO_LARGE,
                                    "manifest file too large.");
      break;
    case PP_NACL_ERROR_MANIFEST_NOACCESS_URL:
      pp_error = PP_ERROR_NOACCESS;
      load_manager->ReportLoadError(PP_NACL_ERROR_MANIFEST_NOACCESS_URL,
                                    "access to manifest url was denied.");
      break;
    default:
      NOTREACHED();
      pp_error = PP_ERROR_FAILED;
      load_manager->ReportLoadError(PP_NACL_ERROR_MANIFEST_LOAD_URL,
                                    "could not load manifest url.");
  }

  if (pp_error == PP_OK) {
    std::string base_url = load_manager->manifest_base_url().spec();
    if (!CreateJsonManifest(instance, base_url, data))
      pp_error = PP_ERROR_FAILED;
  }
  callback.func(callback.user_data, pp_error);
}

bool CreateJsonManifest(PP_Instance instance,
                        const std::string& manifest_url,
                        const std::string& manifest_data) {
  HistogramSizeKB("NaCl.Perf.Size.Manifest",
                  static_cast<int32_t>(manifest_data.length() / 1024));

  nacl::NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  if (!load_manager)
    return false;

  const char* isa_type;
  if (load_manager->IsPNaCl())
    isa_type = kPortableArch;
  else
    isa_type = GetSandboxArch();

  scoped_ptr<nacl::JsonManifest> j(
      new nacl::JsonManifest(
          manifest_url.c_str(),
          isa_type,
          IsNonSFIModeEnabled(),
          PP_ToBool(NaClDebugEnabledForURL(manifest_url.c_str()))));
  JsonManifest::ErrorInfo error_info;
  if (j->Init(manifest_data.c_str(), &error_info)) {
    AddJsonManifest(instance, j.Pass());
    return true;
  }
  load_manager->ReportLoadError(error_info.error, error_info.string);
  return false;
}

PP_Bool ManifestGetProgramURL(PP_Instance instance,
                              PP_Var* pp_full_url,
                              PP_PNaClOptions* pnacl_options,
                              PP_Bool* pp_uses_nonsfi_mode) {
  nacl::NexeLoadManager* load_manager = GetNexeLoadManager(instance);

  JsonManifest* manifest = GetJsonManifest(instance);
  if (manifest == NULL)
    return PP_FALSE;

  bool uses_nonsfi_mode;
  std::string full_url;
  JsonManifest::ErrorInfo error_info;
  if (manifest->GetProgramURL(&full_url, pnacl_options, &uses_nonsfi_mode,
                              &error_info)) {
    *pp_full_url = ppapi::StringVar::StringToPPVar(full_url);
    *pp_uses_nonsfi_mode = PP_FromBool(uses_nonsfi_mode);
    return PP_TRUE;
  }

  if (load_manager)
    load_manager->ReportLoadError(error_info.error, error_info.string);
  return PP_FALSE;
}

bool ManifestResolveKey(PP_Instance instance,
                        bool is_helper_process,
                        const std::string& key,
                        std::string* full_url,
                        PP_PNaClOptions* pnacl_options) {
  // For "helper" processes (llc and ld, for PNaCl translation), we resolve
  // keys manually as there is no existing .nmf file to parse.
  if (is_helper_process) {
    pnacl_options->translate = PP_FALSE;
    // We can only resolve keys in the files/ namespace.
    const std::string kFilesPrefix = "files/";
    if (key.find(kFilesPrefix) == std::string::npos) {
      nacl::NexeLoadManager* load_manager = GetNexeLoadManager(instance);
      if (load_manager)
        load_manager->ReportLoadError(PP_NACL_ERROR_MANIFEST_RESOLVE_URL,
                                      "key did not start with files/");
      return false;
    }
    std::string key_basename = key.substr(kFilesPrefix.length());
    *full_url = std::string(kPNaClTranslatorBaseUrl) + GetSandboxArch() + "/" +
                key_basename;
    return true;
  }

  JsonManifest* manifest = GetJsonManifest(instance);
  if (manifest == NULL)
    return false;

  return manifest->ResolveKey(key, full_url, pnacl_options);
}

PP_Bool GetPNaClResourceInfo(PP_Instance instance,
                             PP_Var* llc_tool_name,
                             PP_Var* ld_tool_name) {
  static const char kFilename[] = "chrome://pnacl-translator/pnacl.json";
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (!load_manager)
    return PP_FALSE;

  uint64_t nonce_lo = 0;
  uint64_t nonce_hi = 0;
  base::File file(GetReadonlyPnaclFd(kFilename, false /* is_executable */,
                                     &nonce_lo, &nonce_hi));
  if (!file.IsValid()) {
    load_manager->ReportLoadError(
        PP_NACL_ERROR_PNACL_RESOURCE_FETCH,
        "The Portable Native Client (pnacl) component is not "
        "installed. Please consult chrome://components for more "
        "information.");
    return PP_FALSE;
  }

  base::File::Info file_info;
  if (!file.GetInfo(&file_info)) {
    load_manager->ReportLoadError(
        PP_NACL_ERROR_PNACL_RESOURCE_FETCH,
        std::string("GetPNaClResourceInfo, GetFileInfo failed for: ") +
            kFilename);
    return PP_FALSE;
  }

  if (file_info.size > 1 << 20) {
    load_manager->ReportLoadError(
        PP_NACL_ERROR_PNACL_RESOURCE_FETCH,
        std::string("GetPNaClResourceInfo, file too large: ") + kFilename);
    return PP_FALSE;
  }

  scoped_ptr<char[]> buffer(new char[file_info.size + 1]);
  if (buffer.get() == NULL) {
    load_manager->ReportLoadError(
        PP_NACL_ERROR_PNACL_RESOURCE_FETCH,
        std::string("GetPNaClResourceInfo, couldn't allocate for: ") +
            kFilename);
    return PP_FALSE;
  }

  int rc = file.Read(0, buffer.get(), file_info.size);
  if (rc < 0) {
    load_manager->ReportLoadError(
        PP_NACL_ERROR_PNACL_RESOURCE_FETCH,
        std::string("GetPNaClResourceInfo, reading failed for: ") + kFilename);
    return PP_FALSE;
  }

  // Null-terminate the bytes we we read from the file.
  buffer.get()[rc] = 0;

  // Expect the JSON file to contain a top-level object (dictionary).
  Json::Reader json_reader;
  Json::Value json_data;
  if (!json_reader.parse(buffer.get(), json_data)) {
    load_manager->ReportLoadError(
        PP_NACL_ERROR_PNACL_RESOURCE_FETCH,
        std::string("Parsing resource info failed: JSON parse error: ") +
            json_reader.getFormattedErrorMessages());
    return PP_FALSE;
  }

  if (!json_data.isObject()) {
    load_manager->ReportLoadError(
        PP_NACL_ERROR_PNACL_RESOURCE_FETCH,
        "Parsing resource info failed: Malformed JSON dictionary");
    return PP_FALSE;
  }

  if (json_data.isMember("pnacl-llc-name")) {
    Json::Value json_name = json_data["pnacl-llc-name"];
    if (json_name.isString()) {
      std::string llc_tool_name_str = json_name.asString();
      *llc_tool_name = ppapi::StringVar::StringToPPVar(llc_tool_name_str);
    }
  }

  if (json_data.isMember("pnacl-ld-name")) {
    Json::Value json_name = json_data["pnacl-ld-name"];
    if (json_name.isString()) {
      std::string ld_tool_name_str = json_name.asString();
      *ld_tool_name = ppapi::StringVar::StringToPPVar(ld_tool_name_str);
    }
  }
  return PP_TRUE;
}

PP_Var GetCpuFeatureAttrs() {
  return ppapi::StringVar::StringToPPVar(GetCpuFeatures());
}

void PostMessageToJavaScriptMainThread(PP_Instance instance,
                                       const std::string& message) {
  content::PepperPluginInstance* plugin_instance =
      content::PepperPluginInstance::Get(instance);
  if (plugin_instance) {
    PP_Var message_var = ppapi::StringVar::StringToPPVar(message);
    plugin_instance->PostMessageToJavaScript(message_var);
    ppapi::PpapiGlobals::Get()->GetVarTracker()->ReleaseVar(message_var);
  }
}

void PostMessageToJavaScript(PP_Instance instance, const char* message) {
  ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
      FROM_HERE,
      base::Bind(&PostMessageToJavaScriptMainThread,
                 instance,
                 std::string(message)));
}

// Encapsulates some of the state for a call to DownloadNexe to prevent
// argument lists from getting too long.
struct DownloadNexeRequest {
  PP_Instance instance;
  std::string url;
  PP_CompletionCallback callback;
  base::Time start_time;
};

// A utility class to ensure that we don't send progress events more often than
// every 10ms for a given file.
class ProgressEventRateLimiter {
 public:
  explicit ProgressEventRateLimiter(PP_Instance instance)
      : instance_(instance) { }

  void ReportProgress(const std::string& url,
                      int64_t total_bytes_received,
                      int64_t total_bytes_to_be_received) {
    base::Time now = base::Time::Now();
    if (now - last_event_ > base::TimeDelta::FromMilliseconds(10)) {
      DispatchProgressEvent(instance_,
                            ProgressEvent(PP_NACL_EVENT_PROGRESS,
                                          url,
                                          total_bytes_to_be_received >= 0,
                                          total_bytes_received,
                                          total_bytes_to_be_received));
      last_event_ = now;
    }
  }

 private:
  PP_Instance instance_;
  base::Time last_event_;
};

void DownloadNexeCompletion(const DownloadNexeRequest& request,
                            PP_NaClFileInfo* out_file_info,
                            FileDownloader::Status status,
                            base::File target_file,
                            int http_status);

void DownloadNexe(PP_Instance instance,
                  const char* url,
                  PP_NaClFileInfo* out_file_info,
                  PP_CompletionCallback callback) {
  CHECK(url);
  CHECK(out_file_info);
  DownloadNexeRequest request;
  request.instance = instance;
  request.url = url;
  request.callback = callback;
  request.start_time = base::Time::Now();

  // Try the fast path for retrieving the file first.
  PP_FileHandle handle = OpenNaClExecutable(instance,
                                            url,
                                            &out_file_info->token_lo,
                                            &out_file_info->token_hi);
  if (handle != PP_kInvalidFileHandle) {
    DownloadNexeCompletion(request,
                           out_file_info,
                           FileDownloader::SUCCESS,
                           base::File(handle),
                           200);
    return;
  }

  // The fast path didn't work, we'll fetch the file using URLLoader and write
  // it to local storage.
  base::File target_file(CreateTemporaryFile(instance));
  GURL gurl(url);

  content::PepperPluginInstance* plugin_instance =
      content::PepperPluginInstance::Get(instance);
  if (!plugin_instance) {
    ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
        FROM_HERE,
        base::Bind(callback.func, callback.user_data,
                   static_cast<int32_t>(PP_ERROR_FAILED)));
  }
  const blink::WebDocument& document =
      plugin_instance->GetContainer()->element().document();
  scoped_ptr<blink::WebURLLoader> url_loader(
      CreateWebURLLoader(document, gurl));
  blink::WebURLRequest url_request = CreateWebURLRequest(document, gurl);

  ProgressEventRateLimiter* tracker = new ProgressEventRateLimiter(instance);

  // FileDownloader deletes itself after invoking DownloadNexeCompletion.
  FileDownloader* file_downloader = new FileDownloader(
      url_loader.Pass(),
      target_file.Pass(),
      base::Bind(&DownloadNexeCompletion, request, out_file_info),
      base::Bind(&ProgressEventRateLimiter::ReportProgress,
                 base::Owned(tracker), std::string(url)));
  file_downloader->Load(url_request);
}

void DownloadNexeCompletion(const DownloadNexeRequest& request,
                            PP_NaClFileInfo* out_file_info,
                            FileDownloader::Status status,
                            base::File target_file,
                            int http_status) {
  int32_t pp_error = FileDownloaderToPepperError(status);
  int64_t bytes_read = -1;
  if (pp_error == PP_OK && target_file.IsValid()) {
    base::File::Info info;
    if (target_file.GetInfo(&info))
      bytes_read = info.size;
  }

  if (bytes_read == -1) {
    target_file.Close();
    pp_error = PP_ERROR_FAILED;
  }

  base::TimeDelta download_time = base::Time::Now() - request.start_time;

  NexeLoadManager* load_manager = GetNexeLoadManager(request.instance);
  if (load_manager) {
    load_manager->NexeFileDidOpen(pp_error,
                                  target_file,
                                  http_status,
                                  bytes_read,
                                  request.url,
                                  download_time);
  }

  if (pp_error == PP_OK && target_file.IsValid())
    out_file_info->handle = target_file.TakePlatformFile();
  else
    out_file_info->handle = PP_kInvalidFileHandle;

  request.callback.func(request.callback.user_data, pp_error);
}

void DownloadFileCompletion(
    const DownloadFileCallback& callback,
    FileDownloader::Status status,
    base::File file,
    int http_status) {
  int32_t pp_error = FileDownloaderToPepperError(status);
  PP_NaClFileInfo file_info;
  if (pp_error == PP_OK) {
    file_info.handle = file.TakePlatformFile();
    file_info.token_lo = 0;
    file_info.token_hi = 0;
  } else {
    file_info = kInvalidNaClFileInfo;
  }

  callback.Run(pp_error, file_info);
}

void DownloadFile(PP_Instance instance,
                  const std::string& url,
                  const DownloadFileCallback& callback) {
  DCHECK(ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->
             BelongsToCurrentThread());

  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (!load_manager) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   static_cast<int32_t>(PP_ERROR_FAILED),
                   kInvalidNaClFileInfo));
    return;
  }

  // Handle special PNaCl support files which are installed on the user's
  // machine.
  if (url.find(kPNaClTranslatorBaseUrl, 0) == 0) {
    PP_NaClFileInfo file_info = kInvalidNaClFileInfo;
    PP_FileHandle handle = GetReadonlyPnaclFd(url.c_str(),
                                              false /* is_executable */,
                                              &file_info.token_lo,
                                              &file_info.token_hi);
    if (handle == PP_kInvalidFileHandle) {
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(callback,
                     static_cast<int32_t>(PP_ERROR_FAILED),
                     kInvalidNaClFileInfo));
      return;
    }
    file_info.handle = handle;
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, static_cast<int32_t>(PP_OK), file_info));
    return;
  }

  // We have to ensure that this url resolves relative to the plugin base url
  // before downloading it.
  const GURL& test_gurl = load_manager->plugin_base_url().Resolve(url);
  if (!test_gurl.is_valid()) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   static_cast<int32_t>(PP_ERROR_FAILED),
                   kInvalidNaClFileInfo));
    return;
  }

  // Try the fast path for retrieving the file first.
  uint64_t file_token_lo = 0;
  uint64_t file_token_hi = 0;
  PP_FileHandle file_handle = OpenNaClExecutable(instance,
                                                 url.c_str(),
                                                 &file_token_lo,
                                                 &file_token_hi);
  if (file_handle != PP_kInvalidFileHandle) {
    PP_NaClFileInfo file_info;
    file_info.handle = file_handle;
    file_info.token_lo = file_token_lo;
    file_info.token_hi = file_token_hi;
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, static_cast<int32_t>(PP_OK), file_info));
    return;
  }

  // The fast path didn't work, we'll fetch the file using URLLoader and write
  // it to local storage.
  base::File target_file(CreateTemporaryFile(instance));
  GURL gurl(url);

  content::PepperPluginInstance* plugin_instance =
      content::PepperPluginInstance::Get(instance);
  if (!plugin_instance) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   static_cast<int32_t>(PP_ERROR_FAILED),
                   kInvalidNaClFileInfo));
  }
  const blink::WebDocument& document =
      plugin_instance->GetContainer()->element().document();
  scoped_ptr<blink::WebURLLoader> url_loader(
      CreateWebURLLoader(document, gurl));
  blink::WebURLRequest url_request = CreateWebURLRequest(document, gurl);

  ProgressEventRateLimiter* tracker = new ProgressEventRateLimiter(instance);

  // FileDownloader deletes itself after invoking DownloadNexeCompletion.
  FileDownloader* file_downloader = new FileDownloader(
      url_loader.Pass(),
      target_file.Pass(),
      base::Bind(&DownloadFileCompletion, callback),
      base::Bind(&ProgressEventRateLimiter::ReportProgress,
                 base::Owned(tracker), std::string(url)));
  file_downloader->Load(url_request);
}

void ReportSelLdrStatus(PP_Instance instance,
                        int32_t load_status,
                        int32_t max_status) {
  HistogramEnumerate("NaCl.LoadStatus.SelLdr", load_status, max_status);
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (!load_manager)
    return;

  // Gather data to see if being installed changes load outcomes.
  const char* name = load_manager->is_installed() ?
      "NaCl.LoadStatus.SelLdr.InstalledApp" :
      "NaCl.LoadStatus.SelLdr.NotInstalledApp";
  HistogramEnumerate(name, load_status, max_status);
}

void LogTranslateTime(const char* histogram_name,
                      int64_t time_in_us) {
  ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
      FROM_HERE,
      base::Bind(&HistogramTimeTranslation,
                 std::string(histogram_name),
                 time_in_us / 1000));
}

void DidOpenManifestEntry(PP_NaClFileInfo* out_file_info,
                          PP_CompletionCallback callback,
                          int32_t pp_error,
                          const PP_NaClFileInfo& file_info) {
  if (pp_error == PP_OK)
    *out_file_info = file_info;
  callback.func(callback.user_data, pp_error);
}

void OpenManifestEntry(PP_Instance instance,
                       PP_Bool is_helper_process,
                       const char* key,
                       PP_NaClFileInfo* out_file_info,
                       PP_CompletionCallback callback) {
  std::string url;
  PP_PNaClOptions pnacl_options;
  pnacl_options.translate = PP_FALSE;
  pnacl_options.is_debug = PP_FALSE;
  pnacl_options.opt_level = 2;
  if (!ManifestResolveKey(instance,
                          PP_ToBool(is_helper_process),
                          key,
                          &url,
                          &pnacl_options)) {
    PostPPCompletionCallback(callback, PP_ERROR_FAILED);
  }

  // TODO(teravest): Make a type like PP_NaClFileInfo to use for DownloadFile
  // that would close the file handle on destruction.
  DownloadFile(instance, url,
               base::Bind(&DidOpenManifestEntry, out_file_info, callback));
}

void SetPNaClStartTime(PP_Instance instance) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  if (load_manager)
    load_manager->set_pnacl_start_time(base::Time::Now());
}

const PPB_NaCl_Private nacl_interface = {
  &LaunchSelLdr,
  &StartPpapiProxy,
  &UrandomFD,
  &Are3DInterfacesDisabled,
  &BrokerDuplicateHandle,
  &GetReadExecPnaclFd,
  &CreateTemporaryFile,
  &GetNumberOfProcessors,
  &PPIsNonSFIModeEnabled,
  &GetNexeFd,
  &ReportTranslationFinished,
  &DispatchEvent,
  &ReportLoadSuccess,
  &ReportLoadError,
  &ReportLoadAbort,
  &NexeDidCrash,
  &InstanceCreated,
  &InstanceDestroyed,
  &NaClDebugEnabledForURL,
  &GetSandboxArch,
  &LogToConsole,
  &GetNaClReadyState,
  &GetExitStatus,
  &SetExitStatus,
  &Vlog,
  &InitializePlugin,
  &GetNexeSize,
  &RequestNaClManifest,
  &GetManifestBaseURL,
  &ProcessNaClManifest,
  &DevInterfacesEnabled,
  &ManifestGetProgramURL,
  &GetPNaClResourceInfo,
  &GetCpuFeatureAttrs,
  &PostMessageToJavaScript,
  &DownloadNexe,
  &ReportSelLdrStatus,
  &LogTranslateTime,
  &OpenManifestEntry,
  &SetPNaClStartTime
};

}  // namespace

const PPB_NaCl_Private* GetNaClPrivateInterface() {
  return &nacl_interface;
}

}  // namespace nacl
