// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/renderer/nexe_load_manager.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "components/nacl/common/nacl_host_messages.h"
#include "components/nacl/common/nacl_types.h"
#include "components/nacl/renderer/histogram.h"
#include "components/nacl/renderer/manifest_service_channel.h"
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
#include "ppapi/shared_impl/scoped_pp_var.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/var_tracker.h"
#include "ppapi/thunk/enter.h"
#include "third_party/WebKit/public/web/WebDOMResourceProgressEvent.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "v8/include/v8.h"

namespace {

const char* const kTypeAttribute = "type";
// The "src" attribute of the <embed> tag.  The value is expected to be either
// a URL or URI pointing to the manifest file (which is expected to contain
// JSON matching ISAs with .nexe URLs).
const char* const kSrcManifestAttribute = "src";
// The "nacl" attribute of the <embed> tag.  We use the value of this attribute
// to find the manifest file when NaCl is registered as a plug-in for another
// MIME type because the "src" attribute is used to supply us with the resource
// of that MIME type that we're supposed to display.
const char* const kNaClManifestAttribute = "nacl";
// Define an argument name to enable 'dev' interfaces. To make sure it doesn't
// collide with any user-defined HTML attribute, make the first character '@'.
const char* const kDevAttribute = "@dev";

const char* const kNaClMIMEType = "application/x-nacl";
const char* const kPNaClMIMEType = "application/x-pnacl";

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

std::string LookupAttribute(const std::map<std::string, std::string>& args,
                            const std::string& key) {
  std::map<std::string, std::string>::const_iterator it = args.find(key);
  if (it != args.end())
    return it->second;
  return std::string();
}

}  // namespace

namespace nacl {

NexeLoadManager::NexeLoadManager(
    PP_Instance pp_instance)
    : pp_instance_(pp_instance),
      nacl_ready_state_(PP_NACL_READY_STATE_UNSENT),
      nexe_error_reported_(false),
      is_installed_(false),
      exit_status_(-1),
      nexe_size_(0),
      plugin_instance_(content::PepperPluginInstance::Get(pp_instance)),
      weak_factory_(this) {
  SetLastError("");
  HistogramEnumerateOsArch(GetSandboxArch());
  if (plugin_instance_) {
    plugin_base_url_ =
        plugin_instance_->GetContainer()->element().document().url();
  }
}

NexeLoadManager::~NexeLoadManager() {
  if (!nexe_error_reported_) {
    base::TimeDelta uptime = base::Time::Now() - ready_time_;
    HistogramTimeLarge("NaCl.ModuleUptime.Normal", uptime.InMilliseconds());
  }
}

void NexeLoadManager::NexeFileDidOpen(int32_t pp_error,
                                      int32_t fd,
                                      int32_t http_status,
                                      int64_t nexe_bytes_read,
                                      const std::string& url,
                                      int64_t time_since_open) {
  // Check that we are on the main renderer thread.
  DCHECK(content::RenderThread::Get());
  VLOG(1) << "Plugin::NexeFileDidOpen (pp_error=" << pp_error << ")";
  VLOG(1) << "Plugin::NexeFileDidOpen (file_desc=" << fd << ")";
  HistogramHTTPStatusCode(
      is_installed_ ? "NaCl.HttpStatusCodeClass.Nexe.InstalledApp" :
                      "NaCl.HttpStatusCodeClass.Nexe.NotInstalledApp",
      http_status);
  // TODO(dmichael): fd is only used for error reporting here currently, and
  // the trusted Plugin is responsible for using it and closing it.
  // Note -1 is NACL_NO_FILE_DESC from nacl_macros.h.
  if (pp_error != PP_OK || fd == -1) {
    if (pp_error == PP_ERROR_ABORTED) {
      ReportLoadAbort();
    } else if (pp_error == PP_ERROR_NOACCESS) {
      ReportLoadError(PP_NACL_ERROR_NEXE_NOACCESS_URL,
                      "access to nexe url was denied.");
    } else {
      ReportLoadError(PP_NACL_ERROR_NEXE_LOAD_URL,
                      "could not load nexe url.");
    }
  } else if (nexe_bytes_read == -1) {
    ReportLoadError(PP_NACL_ERROR_NEXE_STAT, "could not stat nexe file.");
  } else {
    // TODO(dmichael): Can we avoid stashing away so much state?
    nexe_size_ = nexe_bytes_read;
    HistogramSizeKB("NaCl.Perf.Size.Nexe",
                    static_cast<int32_t>(nexe_size_ / 1024));
    HistogramStartupTimeMedium(
        "NaCl.Perf.StartupTime.NexeDownload",
        base::TimeDelta::FromMilliseconds(time_since_open),
        nexe_size_);

    // Inform JavaScript that we successfully downloaded the nacl module.
    ProgressEvent progress_event(pp_instance_, PP_NACL_EVENT_PROGRESS, url,
        true, nexe_size_, nexe_size_);
    ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
        FROM_HERE,
        base::Bind(&NexeLoadManager::DispatchEvent,
                   weak_factory_.GetWeakPtr(),
                   progress_event));

    load_start_ = base::Time::Now();
  }
}

void NexeLoadManager::ReportLoadSuccess(const std::string& url,
                                        uint64_t loaded_bytes,
                                        uint64_t total_bytes) {
  ready_time_ = base::Time::Now();
  if (!IsPNaCl()) {
    base::TimeDelta load_module_time = ready_time_ - load_start_;
    HistogramStartupTimeSmall(
        "NaCl.Perf.StartupTime.LoadModule", load_module_time, nexe_size_);
    HistogramStartupTimeMedium(
        "NaCl.Perf.StartupTime.Total", ready_time_ - init_time_, nexe_size_);
  }

  // Check that we are on the main renderer thread.
  DCHECK(content::RenderThread::Get());
  set_nacl_ready_state(PP_NACL_READY_STATE_DONE);

  // Inform JavaScript that loading was successful and is complete.
  ProgressEvent load_event(pp_instance_, PP_NACL_EVENT_LOAD, url, true,
      loaded_bytes, total_bytes);
  ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
      FROM_HERE,
      base::Bind(&NexeLoadManager::DispatchEvent,
                 weak_factory_.GetWeakPtr(),
                 load_event));

  ProgressEvent loadend_event(pp_instance_, PP_NACL_EVENT_LOADEND, url, true,
      loaded_bytes, total_bytes);
  ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
      FROM_HERE,
      base::Bind(&NexeLoadManager::DispatchEvent,
                 weak_factory_.GetWeakPtr(),
                 loadend_event));

  // UMA
  HistogramEnumerateLoadStatus(PP_NACL_ERROR_LOAD_SUCCESS, is_installed_);
}

void NexeLoadManager::ReportLoadError(PP_NaClError error,
                                      const std::string& error_message) {
  ReportLoadError(error, error_message, error_message);
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
  SetLastError(error_string);

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

  HistogramEnumerateLoadStatus(error, is_installed_);
  LogToConsole(console_message);
}

void NexeLoadManager::ReportLoadAbort() {
  // Check that we are on the main renderer thread.
  DCHECK(content::RenderThread::Get());

  // Set the readyState attribute to indicate we need to start over.
  set_nacl_ready_state(PP_NACL_READY_STATE_DONE);
  nexe_error_reported_ = true;

  // Report an error in lastError and on the JavaScript console.
  std::string error_string("NaCl module load failed: user aborted");
  SetLastError(error_string);

  // Inform JavaScript that loading was aborted and is complete.
  ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
      FROM_HERE,
      base::Bind(&NexeLoadManager::DispatchEvent,
                 weak_factory_.GetWeakPtr(),
                 ProgressEvent(PP_NACL_EVENT_ABORT)));

  ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
      FROM_HERE,
      base::Bind(&NexeLoadManager::DispatchEvent,
                 weak_factory_.GetWeakPtr(),
                 ProgressEvent(PP_NACL_EVENT_LOADEND)));

  HistogramEnumerateLoadStatus(PP_NACL_ERROR_LOAD_ABORTED, is_installed_);
  LogToConsole(error_string);
}

void NexeLoadManager::NexeDidCrash(const char* crash_log) {
  VLOG(1) << "Plugin::NexeDidCrash: crash event!";
    // The NaCl module voluntarily exited.  However, this is still a
    // crash from the point of view of Pepper, since PPAPI plugins are
    // event handlers and should never exit.
  VLOG_IF(1, exit_status_ != -1)
      << "Plugin::NexeDidCrash: nexe exited with status " << exit_status_
      << " so this is a \"controlled crash\".";
  // If the crash occurs during load, we just want to report an error
  // that fits into our load progress event grammar.  If the crash
  // occurs after loaded/loadend, then we use ReportDeadNexe to send a
  // "crash" event.
  if (nexe_error_reported_) {
    VLOG(1) << "Plugin::NexeDidCrash: error already reported; suppressing";
  } else {
    if (nacl_ready_state_ == PP_NACL_READY_STATE_DONE) {
      ReportDeadNexe();
    } else {
      ReportLoadError(PP_NACL_ERROR_START_PROXY_CRASH,
                      "Nexe crashed during startup");
    }
  }
  // In all cases, try to grab the crash log.  The first error
  // reported may have come from the start_module RPC reply indicating
  // a validation error or something similar, which wouldn't grab the
  // crash log.  In the event that this is called twice, the second
  // invocation will just be a no-op, since the entire crash log will
  // have been received and we'll just get an EOF indication.
  CopyCrashLogToJsConsole(crash_log);
}

void NexeLoadManager::DispatchEvent(const ProgressEvent &event) {
  blink::WebPluginContainer* container = plugin_instance_->GetContainer();
  // It's possible that container() is NULL if the plugin has been removed from
  // the DOM (but the PluginInstance is not destroyed yet).
  if (!container)
    return;
  blink::WebLocalFrame* frame = container->element().document().frame();
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

void NexeLoadManager::set_manifest_service_channel(
    scoped_ptr<ManifestServiceChannel> channel) {
  manifest_service_channel_ = channel.Pass();
}

PP_NaClReadyState NexeLoadManager::nacl_ready_state() {
  return nacl_ready_state_;
}

void NexeLoadManager::set_nacl_ready_state(PP_NaClReadyState ready_state) {
  nacl_ready_state_ = ready_state;
  ppapi::ScopedPPVar ready_state_name(
      ppapi::ScopedPPVar::PassRef(),
      ppapi::StringVar::StringToPPVar("readyState"));
  SetReadOnlyProperty(ready_state_name.get(), PP_MakeInt32(ready_state));
}

void NexeLoadManager::SetLastError(const std::string& error) {
  ppapi::ScopedPPVar error_name_var(
      ppapi::ScopedPPVar::PassRef(),
      ppapi::StringVar::StringToPPVar("lastError"));
  ppapi::ScopedPPVar error_var(
      ppapi::ScopedPPVar::PassRef(),
      ppapi::StringVar::StringToPPVar(error));
  SetReadOnlyProperty(error_name_var.get(), error_var.get());
}

void NexeLoadManager::SetReadOnlyProperty(PP_Var key, PP_Var value) {
  plugin_instance_->SetEmbedProperty(key, value);
}

void NexeLoadManager::LogToConsole(const std::string& message) {
  ppapi::PpapiGlobals::Get()->LogWithSource(
      pp_instance_, PP_LOGLEVEL_LOG, std::string("NativeClient"), message);
}

void NexeLoadManager::set_exit_status(int exit_status) {
  exit_status_ = exit_status;
  ppapi::ScopedPPVar exit_status_name_var(
      ppapi::ScopedPPVar::PassRef(),
      ppapi::StringVar::StringToPPVar("exitStatus"));
  SetReadOnlyProperty(exit_status_name_var.get(), PP_MakeInt32(exit_status));
}

void NexeLoadManager::InitializePlugin(
    uint32_t argc, const char* argn[], const char* argv[]) {
  init_time_ = base::Time::Now();

  for (size_t i = 0; i < argc; ++i) {
    std::string name(argn[i]);
    std::string value(argv[i]);
    args_[name] = value;
  }

  // Store mime_type_ at initialization time since we make it lowercase.
  mime_type_ = StringToLowerASCII(LookupAttribute(args_, kTypeAttribute));
}

void NexeLoadManager::ReportStartupOverhead() const {
  base::TimeDelta overhead = base::Time::Now() - init_time_;
  HistogramStartupTimeMedium(
      "NaCl.Perf.StartupTime.NaClOverhead", overhead, nexe_size_);
}

bool NexeLoadManager::RequestNaClManifest(const std::string& url,
                                          bool* is_data_uri) {
  if (plugin_base_url_.is_valid()) {
    const GURL& resolved_url = plugin_base_url_.Resolve(url);
    if (resolved_url.is_valid()) {
      manifest_base_url_ = resolved_url;
      is_installed_ = manifest_base_url_.SchemeIs("chrome-extension");
      *is_data_uri = manifest_base_url_.SchemeIs("data");
      HistogramEnumerateManifestIsDataURI(*is_data_uri);

      set_nacl_ready_state(PP_NACL_READY_STATE_OPENED);
      ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
        FROM_HERE,
        base::Bind(&NexeLoadManager::DispatchEvent,
                   weak_factory_.GetWeakPtr(),
                   ProgressEvent(PP_NACL_EVENT_LOADSTART)));
      return true;
    }
  }
  ReportLoadError(PP_NACL_ERROR_MANIFEST_RESOLVE_URL,
                  std::string("could not resolve URL \"") + url +
                  "\" relative to \"" +
                  plugin_base_url_.possibly_invalid_spec() + "\".");
  return false;
}

void NexeLoadManager::ProcessNaClManifest(const std::string& program_url) {
  GURL gurl(program_url);
  DCHECK(gurl.is_valid());
  if (gurl.is_valid())
    is_installed_ = gurl.SchemeIs("chrome-extension");
  set_nacl_ready_state(PP_NACL_READY_STATE_LOADING);
  ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
      FROM_HERE,
      base::Bind(&NexeLoadManager::DispatchEvent,
                 weak_factory_.GetWeakPtr(),
                 ProgressEvent(PP_NACL_EVENT_PROGRESS)));
}

std::string NexeLoadManager::GetManifestURLArgument() const {
  std::string manifest_url;

  // If the MIME type is foreign, then this NEXE is being used as a content
  // type handler rather than directly by an HTML document.
  bool nexe_is_content_handler =
      !mime_type_.empty() &&
      mime_type_ != kNaClMIMEType &&
      mime_type_ != kPNaClMIMEType;

  if (nexe_is_content_handler) {
    // For content handlers 'src' will be the URL for the content
    // and 'nacl' will be the URL for the manifest.
    manifest_url = LookupAttribute(args_, kNaClManifestAttribute);
  } else {
    manifest_url = LookupAttribute(args_, kSrcManifestAttribute);
  }

  if (manifest_url.empty()) {
    VLOG(1) << "WARNING: no 'src' property, so no manifest loaded.";
    if (args_.find(kNaClManifestAttribute) != args_.end())
      VLOG(1) << "WARNING: 'nacl' property is incorrect. Use 'src'.";
  }
  return manifest_url;
}

bool NexeLoadManager::IsPNaCl() const {
  return mime_type_ == kPNaClMIMEType;
}

bool NexeLoadManager::DevInterfacesEnabled() const {
  // Look for the developer attribute; if it's present, enable 'dev'
  // interfaces.
  return args_.find(kDevAttribute) != args_.end();
}

void NexeLoadManager::ReportDeadNexe() {
  if (nacl_ready_state_ == PP_NACL_READY_STATE_DONE &&  // After loadEnd
      !nexe_error_reported_) {
    // Crashes will be more likely near startup, so use a medium histogram
    // instead of a large one.
    base::TimeDelta uptime = base::Time::Now() - ready_time_;
    HistogramTimeMedium("NaCl.ModuleUptime.Crash", uptime.InMilliseconds());

    std::string message("NaCl module crashed");
    SetLastError(message);
    LogToConsole(message);

    ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
        FROM_HERE,
        base::Bind(&NexeLoadManager::DispatchEvent,
                   weak_factory_.GetWeakPtr(),
                   ProgressEvent(PP_NACL_EVENT_CRASH)));
    nexe_error_reported_ = true;
  }
  // else ReportLoadError() and ReportLoadAbort() will be used by loading code
  // to provide error handling.
}

void NexeLoadManager::CopyCrashLogToJsConsole(const std::string& crash_log) {
  base::StringTokenizer t(crash_log, "\n");
  while (t.GetNext())
    LogToConsole(t.token());
}

}  // namespace nacl
