// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_content_client.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/render_messages.h"
#include "chrome/default_plugin/plugin_main.h"
#include "content/public/common/pepper_plugin_info.h"
#include "remoting/client/plugin/pepper_entrypoints.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/glue/user_agent.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/plugin_constants.h"

#if defined(OS_WIN)
#include "sandbox/src/sandbox.h"
#endif

namespace {

const char kPDFPluginName[] = "Chrome PDF Viewer";
const char kPDFPluginMimeType[] = "application/pdf";
const char kPDFPluginExtension[] = "pdf";
const char kPDFPluginDescription[] = "Portable Document Format";
const char kPDFPluginPrintPreviewMimeType
   [] = "application/x-google-chrome-print-preview-pdf";

const char kNaClPluginName[] = "Native Client";
const char kNaClPluginMimeType[] = "application/x-nacl";
const char kNaClPluginExtension[] = "nexe";
const char kNaClPluginDescription[] = "Native Client Executable";

const char kNaClOldPluginName[] = "Chrome NaCl";

#if defined(ENABLE_REMOTING)
const char kRemotingViewerPluginName[] = "Remoting Viewer";
const FilePath::CharType kRemotingViewerPluginPath[] =
    FILE_PATH_LITERAL("internal-remoting-viewer");
// Use a consistent MIME-type regardless of branding.
const char kRemotingViewerPluginMimeType[] =
    "application/vnd.chromium.remoting-viewer";
// TODO(wez): Remove the old MIME-type once client code no longer needs it.
const char kRemotingViewerPluginOldMimeType[] =
    "pepper-application/x-chromoting";
#endif

// Appends the known built-in plugins to the given vector. Some built-in
// plugins are "internal" which means they are compiled into the Chrome binary,
// and some are extra shared libraries distributed with the browser (these are
// not marked internal, aside from being automatically registered, they're just
// regular plugins).
void ComputeBuiltInPlugins(std::vector<content::PepperPluginInfo>* plugins) {
  // PDF.
  //
  // Once we're sandboxed, we can't know if the PDF plugin is available or not;
  // but (on Linux) this function is always called once before we're sandboxed.
  // So the first time through test if the file is available and then skip the
  // check on subsequent calls if yes.
  static bool skip_pdf_file_check = false;
  FilePath path;
  if (PathService::Get(chrome::FILE_PDF_PLUGIN, &path)) {
    if (skip_pdf_file_check || file_util::PathExists(path)) {
      content::PepperPluginInfo pdf;
      pdf.path = path;
      pdf.name = kPDFPluginName;
      webkit::WebPluginMimeType pdf_mime_type(kPDFPluginMimeType,
                                              kPDFPluginExtension,
                                              kPDFPluginDescription);
      webkit::WebPluginMimeType print_preview_pdf_mime_type(
          kPDFPluginPrintPreviewMimeType,
          kPDFPluginExtension,
          kPDFPluginDescription);
      pdf.mime_types.push_back(pdf_mime_type);
      pdf.mime_types.push_back(print_preview_pdf_mime_type);
      plugins->push_back(pdf);

      skip_pdf_file_check = true;
    }
  }

  // Handle the Native Client just like the PDF plugin. This means that it is
  // enabled by default. This allows apps installed from the Chrome Web Store
  // to use NaCl even if the command line switch isn't set. For other uses of
  // NaCl we check for the command line switch.
  static bool skip_nacl_file_check = false;
  if (PathService::Get(chrome::FILE_NACL_PLUGIN, &path)) {
    if (skip_nacl_file_check || file_util::PathExists(path)) {
      content::PepperPluginInfo nacl;
      nacl.path = path;
      nacl.name = kNaClPluginName;
      webkit::WebPluginMimeType nacl_mime_type(kNaClPluginMimeType,
                                               kNaClPluginExtension,
                                               kNaClPluginDescription);
      nacl.mime_types.push_back(nacl_mime_type);
      plugins->push_back(nacl);

      skip_nacl_file_check = true;
    }
  }

  // The Remoting Viewer plugin is built-in.
#if defined(ENABLE_REMOTING)
  content::PepperPluginInfo info;
  info.is_internal = true;
  info.name = kRemotingViewerPluginName;
  info.path = FilePath(kRemotingViewerPluginPath);
  webkit::WebPluginMimeType remoting_mime_type(
      kRemotingViewerPluginMimeType,
      std::string(),
      std::string());
  info.mime_types.push_back(remoting_mime_type);
  webkit::WebPluginMimeType old_remoting_mime_type(
      kRemotingViewerPluginOldMimeType,
      std::string(),
      std::string());
  info.mime_types.push_back(old_remoting_mime_type);
  info.internal_entry_points.get_interface = remoting::PPP_GetInterface;
  info.internal_entry_points.initialize_module =
      remoting::PPP_InitializeModule;
  info.internal_entry_points.shutdown_module = remoting::PPP_ShutdownModule;

  plugins->push_back(info);
#endif
}

void AddOutOfProcessFlash(std::vector<content::PepperPluginInfo>* plugins) {
  // Flash being out of process is handled separately than general plugins
  // for testing purposes.
  bool flash_out_of_process = !CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kPpapiFlashInProcess);

  // Handle any Pepper Flash first.
  const CommandLine::StringType flash_path =
      CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          switches::kPpapiFlashPath);
  if (flash_path.empty())
    return;

  content::PepperPluginInfo plugin;
  plugin.is_out_of_process = flash_out_of_process;
  plugin.path = FilePath(flash_path);
  plugin.name = kFlashPluginName;

  const std::string flash_version =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              switches::kPpapiFlashVersion);
  std::vector<std::string> flash_version_numbers;
  base::SplitString(flash_version, '.', &flash_version_numbers);
  if (flash_version_numbers.size() < 1)
    flash_version_numbers.push_back("10");
  // |SplitString()| puts in an empty string given an empty string. :(
  else if (flash_version_numbers[0].empty())
    flash_version_numbers[0] = "10";
  if (flash_version_numbers.size() < 2)
    flash_version_numbers.push_back("2");
  if (flash_version_numbers.size() < 3)
    flash_version_numbers.push_back("999");
  if (flash_version_numbers.size() < 4)
    flash_version_numbers.push_back("999");
  // E.g., "Shockwave Flash 10.2 r154":
  plugin.description = plugin.name + " " + flash_version_numbers[0] + "." +
      flash_version_numbers[1] + " r" + flash_version_numbers[2];
  plugin.version = JoinString(flash_version_numbers, '.');
  webkit::WebPluginMimeType swf_mime_type(kFlashPluginSwfMimeType,
                                          kFlashPluginSwfExtension,
                                          kFlashPluginSwfDescription);
  plugin.mime_types.push_back(swf_mime_type);
  webkit::WebPluginMimeType spl_mime_type(kFlashPluginSplMimeType,
                                          kFlashPluginSplExtension,
                                          kFlashPluginSplDescription);
  plugin.mime_types.push_back(spl_mime_type);
  plugins->push_back(plugin);
}

#if defined(OS_WIN)
// Launches the privileged flash broker, used when flash is sandboxed.
// The broker is the same flash dll, except that it uses a different
// entrypoint (BrokerMain) and it is hosted in windows' generic surrogate
// process rundll32. After launching the broker we need to pass to
// the flash plugin the process id of the broker via the command line
// using --flash-broker=pid.
// More info about rundll32 at http://support.microsoft.com/kb/164787.
bool LoadFlashBroker(const FilePath& plugin_path, CommandLine* cmd_line) {
  FilePath rundll;
  if (!PathService::Get(base::DIR_SYSTEM, &rundll))
    return false;
  rundll = rundll.AppendASCII("rundll32.exe");
  // Rundll32 cannot handle paths with spaces, so we use the short path.
  wchar_t short_path[MAX_PATH];
  if (0 == ::GetShortPathNameW(plugin_path.value().c_str(),
                               short_path, arraysize(short_path)))
    return false;
  // Here is the kicker, if the user has disabled 8.3 (short path) support
  // on the volume GetShortPathNameW does not fail but simply returns the
  // input path. In this case if the path had any spaces then rundll32 will
  // incorrectly interpret its parameters. So we quote the path, even though
  // the kb/164787 says you should not.
  std::wstring cmd_final =
      base::StringPrintf(L"%ls \"%ls\",BrokerMain browser=chrome",
                         rundll.value().c_str(),
                         short_path);
  base::ProcessHandle process;
  base::LaunchOptions options;
  options.start_hidden = true;
  if (!base::LaunchProcess(cmd_final, options, &process))
    return false;

  cmd_line->AppendSwitchASCII("flash-broker",
                              base::Int64ToString(::GetProcessId(process)));

  // The flash broker, unders some circumstances can linger beyond the lifetime
  // of the flash player, so we put it in a job object, when the browser
  // terminates the job object is destroyed (by the OS) and the flash broker
  // is terminated.
  HANDLE job = ::CreateJobObjectW(NULL, NULL);
  if (base::SetJobObjectAsKillOnJobClose(job)) {
    ::AssignProcessToJobObject(job, process);
    // Yes, we are leaking the object here. Read comment above.
  } else {
    ::CloseHandle(job);
    return false;
  }

  ::CloseHandle(process);
  return true;
}
#endif  // OS_WIN

}  // namespace

namespace chrome {

const char* const ChromeContentClient::kPDFPluginName = ::kPDFPluginName;
const char* const ChromeContentClient::kNaClPluginName = ::kNaClPluginName;
const char* const ChromeContentClient::kNaClOldPluginName =
    ::kNaClOldPluginName;

void ChromeContentClient::SetActiveURL(const GURL& url) {
  child_process_logging::SetActiveURL(url);
}

void ChromeContentClient::SetGpuInfo(const content::GPUInfo& gpu_info) {
  child_process_logging::SetGpuInfo(gpu_info);
}

void ChromeContentClient::AddPepperPlugins(
    std::vector<content::PepperPluginInfo>* plugins) {
  ComputeBuiltInPlugins(plugins);
  AddOutOfProcessFlash(plugins);
}

void ChromeContentClient::AddNPAPIPlugins(
    webkit::npapi::PluginList* plugin_list) {
#if defined(OS_WIN) && !defined(USE_AURA)
  // TODO(bauerb): On Windows the default plug-in can download and install
  // missing plug-ins, which we don't support in the browser yet, so keep
  // using the default plug-in on Windows until we do.
  // Aura isn't going to support NPAPI plugins.
  const webkit::npapi::PluginEntryPoints entry_points = {
    default_plugin::NP_GetEntryPoints,
    default_plugin::NP_Initialize,
    default_plugin::NP_Shutdown
  };

  webkit::WebPluginInfo info;
  info.path = FilePath(webkit::npapi::kDefaultPluginLibraryName);
  info.name = ASCIIToUTF16("Default Plug-in");
  info.version = ASCIIToUTF16("1");
  info.desc = ASCIIToUTF16("Provides functionality for installing third-party "
                           "plug-ins");

  webkit::WebPluginMimeType mimeType;
  mimeType.mime_type = "*";
  info.mime_types.push_back(mimeType);

  webkit::npapi::PluginList::Singleton()->RegisterInternalPlugin(
      info,
      entry_points,
      false);
#endif
}

bool ChromeContentClient::CanSendWhileSwappedOut(const IPC::Message* msg) {
  // Any Chrome-specific messages that must be allowed to be sent from swapped
  // out renderers.
  switch (msg->type()) {
    case ChromeViewHostMsg_DomOperationResponse::ID:
      return true;
    default:
      break;
  }
  return false;
}

bool ChromeContentClient::CanHandleWhileSwappedOut(
    const IPC::Message& msg) {
  // Any Chrome-specific messages (apart from those listed in
  // CanSendWhileSwappedOut) that must be handled by the browser when sent from
  // swapped out renderers.
  switch (msg.type()) {
    case ChromeViewHostMsg_Snapshot::ID:
      return true;
    default:
      break;
  }
  return false;
}

std::string ChromeContentClient::GetUserAgent(bool* overriding) const {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kUserAgent)) {
    *overriding = true;
    return CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kUserAgent);
  } else {
    *overriding = false;
    chrome::VersionInfo version_info;
    std::string product("Chrome/");
    product += version_info.is_valid() ? version_info.Version() : "0.0.0.0";
    return webkit_glue::BuildUserAgentFromProduct(product);
  }
}

string16 ChromeContentClient::GetLocalizedString(int message_id) const {
  return l10n_util::GetStringUTF16(message_id);
}

base::StringPiece ChromeContentClient::GetDataResource(int resource_id) const {
  return ResourceBundle::GetSharedInstance().GetRawDataResource(resource_id);
}

#if defined(OS_WIN)
bool ChromeContentClient::SandboxPlugin(CommandLine* command_line,
                                        sandbox::TargetPolicy* policy) {
  std::wstring plugin_dll = command_line->
      GetSwitchValueNative(switches::kPluginPath);

  FilePath builtin_flash;
  if (!PathService::Get(chrome::FILE_FLASH_PLUGIN, &builtin_flash))
    return false;

  FilePath plugin_path(plugin_dll);
  if (plugin_path != builtin_flash)
    return false;

  if (base::win::GetVersion() <= base::win::VERSION_XP ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableFlashSandbox)) {
    return false;
  }

  // Add the policy for the pipes.
  sandbox::ResultCode result = sandbox::SBOX_ALL_OK;
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_NAMED_PIPES,
                           sandbox::TargetPolicy::NAMEDPIPES_ALLOW_ANY,
                           L"\\\\.\\pipe\\chrome.*");
  if (result != sandbox::SBOX_ALL_OK) {
    NOTREACHED();
    return false;
  }

  // Spawn the flash broker and apply sandbox policy.
  if (LoadFlashBroker(plugin_path, command_line)) {
    policy->SetJobLevel(sandbox::JOB_UNPROTECTED, 0);
    policy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                          sandbox::USER_INTERACTIVE);
    // Allow the Flash plugin to forward some messages back to Chrome.
    if (base::win::GetVersion() == base::win::VERSION_VISTA) {
      // Per-window message filters required on Win7 or later must be added to:
      // render_widget_host_view_win.cc RenderWidgetHostViewWin::ReparentWindow
      ::ChangeWindowMessageFilter(WM_MOUSEWHEEL, MSGFLT_ADD);
      ::ChangeWindowMessageFilter(WM_APPCOMMAND, MSGFLT_ADD);
    }
    policy->SetIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
  } else {
    // Could not start the broker, use a very weak policy instead.
    DLOG(WARNING) << "Failed to start flash broker";
    policy->SetJobLevel(sandbox::JOB_UNPROTECTED, 0);
    policy->SetTokenLevel(
        sandbox::USER_UNPROTECTED, sandbox::USER_UNPROTECTED);
  }

  return true;
}
#endif

}  // namespace chrome
