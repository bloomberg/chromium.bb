// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner_util.h"
#include "base/task_scheduler/post_task.h"
#include "build/build_config.h"
#include "content/public/app/content_main.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "headless/app/headless_shell.h"
#include "headless/app/headless_shell_switches.h"
#include "headless/lib/browser/headless_devtools.h"
#include "headless/public/headless_devtools_target.h"
#include "headless/public/util/deterministic_http_protocol_handler.h"
#include "net/base/filename_util.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/http/http_util.h"
#include "net/socket/ssl_client_socket.h"
#include "services/network/public/cpp/network_switches.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/geometry/size.h"

#if defined(OS_WIN)
#include "components/crash/content/app/crash_switches.h"
#include "components/crash/content/app/run_as_crashpad_handler_win.h"
#include "sandbox/win/src/sandbox_types.h"
#endif

namespace headless {

namespace {

// By default listen to incoming DevTools connections on localhost.
const char kUseLocalHostForDevToolsHttpServer[] = "localhost";
// Default file name for screenshot. Can be overriden by "--screenshot" switch.
const char kDefaultScreenshotFileName[] = "screenshot.png";
// Default file name for pdf. Can be overriden by "--print-to-pdf" switch.
const char kDefaultPDFFileName[] = "output.pdf";

bool ParseWindowSize(const std::string& window_size,
                     gfx::Size* parsed_window_size) {
  int width, height = 0;
  if (sscanf(window_size.c_str(), "%d%*[x,]%d", &width, &height) >= 2 &&
      width >= 0 && height >= 0) {
    parsed_window_size->set_width(width);
    parsed_window_size->set_height(height);
    return true;
  }
  return false;
}

bool ParseFontRenderHinting(
    const std::string& font_render_hinting_string,
    gfx::FontRenderParams::Hinting* font_render_hinting) {
  if (font_render_hinting_string == "max") {
    *font_render_hinting = gfx::FontRenderParams::Hinting::HINTING_MAX;
  } else if (font_render_hinting_string == "full") {
    *font_render_hinting = gfx::FontRenderParams::Hinting::HINTING_FULL;
  } else if (font_render_hinting_string == "medium") {
    *font_render_hinting = gfx::FontRenderParams::Hinting::HINTING_MEDIUM;
  } else if (font_render_hinting_string == "slight") {
    *font_render_hinting = gfx::FontRenderParams::Hinting::HINTING_SLIGHT;
  } else if (font_render_hinting_string == "none") {
    *font_render_hinting = gfx::FontRenderParams::Hinting::HINTING_NONE;
  } else {
    return false;
  }
  return true;
}

#if !defined(CHROME_MULTIPLE_DLL_CHILD)
GURL ConvertArgumentToURL(const base::CommandLine::StringType& arg) {
  GURL url(arg);
  if (url.is_valid() && url.has_scheme())
    return url;

  return net::FilePathToFileURL(
      base::MakeAbsoluteFilePath(base::FilePath(arg)));
}

std::vector<GURL> ConvertArgumentsToURLs(
    const base::CommandLine::StringVector& args) {
  std::vector<GURL> urls;
  urls.reserve(args.size());
  for (auto it = args.rbegin(); it != args.rend(); ++it)
    urls.push_back(ConvertArgumentToURL(*it));
  return urls;
}

// Gets file path into ssl_keylog_file from command line argument or
// environment variable. Command line argument has priority when
// both specified.
base::FilePath GetSSLKeyLogFile(const base::CommandLine* command_line) {
  if (command_line->HasSwitch(switches::kSSLKeyLogFile)) {
    base::FilePath path =
        command_line->GetSwitchValuePath(switches::kSSLKeyLogFile);
    if (!path.empty())
      return path;
    LOG(WARNING) << "ssl-key-log-file argument missing";
  }
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  std::string path_str;
  env->GetVar("SSLKEYLOGFILE", &path_str);
#if defined(OS_WIN)
  // base::Environment returns environment variables in UTF-8 on Windows.
  return base::FilePath(base::UTF8ToUTF16(path_str));
#else
  return base::FilePath(path_str);
#endif
}

#endif

}  // namespace

HeadlessShell::HeadlessShell()
    : browser_(nullptr),
      devtools_client_(HeadlessDevToolsClient::Create()),
#if !defined(CHROME_MULTIPLE_DLL_CHILD)
      web_contents_(nullptr),
      browser_context_(nullptr),
#endif
      processed_page_ready_(false),
      weak_factory_(this) {
}

HeadlessShell::~HeadlessShell() = default;

#if !defined(CHROME_MULTIPLE_DLL_CHILD)
void HeadlessShell::OnStart(HeadlessBrowser* browser) {
  browser_ = browser;
  file_task_runner_ = base::CreateSequencedTaskRunnerWithTraits(
      {base::MayBlock(), base::TaskPriority::BACKGROUND});

  HeadlessBrowserContext::Builder context_builder =
      browser_->CreateBrowserContextBuilder();
  // TODO(eseckler): These switches should also affect BrowserContexts that
  // are created via DevTools later.
  base::FilePath ssl_keylog_file =
      GetSSLKeyLogFile(base::CommandLine::ForCurrentProcess());
  if (!ssl_keylog_file.empty())
    net::SSLClientSocket::SetSSLKeyLogFile(ssl_keylog_file);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(::switches::kLang)) {
    context_builder.SetAcceptLanguage(
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            ::switches::kLang));
  }
  DeterministicHttpProtocolHandler* http_handler = nullptr;
  DeterministicHttpProtocolHandler* https_handler = nullptr;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDeterministicFetch)) {
    deterministic_dispatcher_ =
        std::make_unique<DeterministicDispatcher>(browser_->BrowserIOThread());

    ProtocolHandlerMap protocol_handlers;
    protocol_handlers[url::kHttpScheme] =
        std::make_unique<DeterministicHttpProtocolHandler>(
            deterministic_dispatcher_.get(), browser->BrowserIOThread());
    http_handler = static_cast<DeterministicHttpProtocolHandler*>(
        protocol_handlers[url::kHttpScheme].get());
    protocol_handlers[url::kHttpsScheme] =
        std::make_unique<DeterministicHttpProtocolHandler>(
            deterministic_dispatcher_.get(), browser->BrowserIOThread());
    https_handler = static_cast<DeterministicHttpProtocolHandler*>(
        protocol_handlers[url::kHttpsScheme].get());

    context_builder.SetProtocolHandlers(std::move(protocol_handlers));
  }
  browser_context_ = context_builder.Build();
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDeterministicFetch)) {
    http_handler->SetHeadlessBrowserContext(browser_context_);
    https_handler->SetHeadlessBrowserContext(browser_context_);
  }
  browser_->SetDefaultBrowserContext(browser_context_);

  base::CommandLine::StringVector args =
      base::CommandLine::ForCurrentProcess()->GetArgs();

  // TODO(alexclarke): Should we navigate to about:blank first if using
  // virtual time?
  if (args.empty())
#if defined(OS_WIN)
    args.push_back(L"about:blank");
#else
    args.push_back("about:blank");
#endif

  base::PostTaskAndReplyWithResult(
      file_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ConvertArgumentsToURLs, args),
      base::BindOnce(&HeadlessShell::OnGotURLs, weak_factory_.GetWeakPtr()));
}

void HeadlessShell::OnGotURLs(const std::vector<GURL>& urls) {
  HeadlessWebContents::Builder builder(
      browser_context_->CreateWebContentsBuilder());
  for (const auto& url : urls) {
    HeadlessWebContents* web_contents = builder.SetInitialURL(url).Build();
    if (!web_contents) {
      LOG(ERROR) << "Navigation to " << url << " failed";
      browser_->Shutdown();
      return;
    }
    if (!web_contents_ && !RemoteDebuggingEnabled()) {
      // TODO(jzfeng): Support observing multiple targets.
      url_ = url;
      web_contents_ = web_contents;
      web_contents_->AddObserver(this);
    }
  }
}

void HeadlessShell::Shutdown() {
  if (!web_contents_)
    return;
  if (!RemoteDebuggingEnabled()) {
    devtools_client_->GetEmulation()->GetExperimental()->RemoveObserver(this);
    devtools_client_->GetInspector()->GetExperimental()->RemoveObserver(this);
    devtools_client_->GetPage()->GetExperimental()->RemoveObserver(this);
    if (web_contents_->GetDevToolsTarget()) {
      web_contents_->GetDevToolsTarget()->DetachClient(devtools_client_.get());
    }
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDeterministicFetch)) {
    devtools_client_->GetNetwork()->GetExperimental()->RemoveObserver(this);
  }
  web_contents_->RemoveObserver(this);
  web_contents_ = nullptr;
  browser_context_->Close();
  browser_->Shutdown();
}

void HeadlessShell::DevToolsTargetReady() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  web_contents_->GetDevToolsTarget()->AttachClient(devtools_client_.get());
  devtools_client_->GetInspector()->GetExperimental()->AddObserver(this);
  devtools_client_->GetPage()->GetExperimental()->AddObserver(this);
  devtools_client_->GetPage()->Enable();

  devtools_client_->GetEmulation()->GetExperimental()->AddObserver(this);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDeterministicFetch)) {
    devtools_client_->GetNetwork()->GetExperimental()->AddObserver(this);
    std::unique_ptr<headless::network::RequestPattern> match_all =
        headless::network::RequestPattern::Builder().SetUrlPattern("*").Build();
    std::vector<std::unique_ptr<headless::network::RequestPattern>> patterns;
    patterns.push_back(std::move(match_all));
    devtools_client_->GetNetwork()->GetExperimental()->SetRequestInterception(
        network::SetRequestInterceptionParams::Builder()
            .SetPatterns(std::move(patterns))
            .Build());
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDefaultBackgroundColor)) {
    std::string color_hex =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kDefaultBackgroundColor);
    uint32_t color;
    CHECK(base::HexStringToUInt(color_hex, &color))
        << "Expected a hex value for --default-background-color=";
    auto rgba = dom::RGBA::Builder()
                    .SetR((color & 0xff000000) >> 24)
                    .SetG((color & 0x00ff0000) >> 16)
                    .SetB((color & 0x0000ff00) >> 8)
                    .SetA(color & 0x000000ff)
                    .Build();
    devtools_client_->GetEmulation()
        ->GetExperimental()
        ->SetDefaultBackgroundColorOverride(
            emulation::SetDefaultBackgroundColorOverrideParams::Builder()
                .SetColor(std::move(rgba))
                .Build());
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kVirtualTimeBudget)) {
    std::string budget_ms_ascii =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kVirtualTimeBudget);
    int budget_ms;
    CHECK(base::StringToInt(budget_ms_ascii, &budget_ms))
        << "Expected an integer value for --virtual-time-budget=";
    devtools_client_->GetEmulation()->GetExperimental()->SetVirtualTimePolicy(
        emulation::SetVirtualTimePolicyParams::Builder()
            .SetPolicy(
                emulation::VirtualTimePolicy::PAUSE_IF_NETWORK_FETCHES_PENDING)
            .SetBudget(budget_ms)
            .Build());
  } else {
    // Check if the document had already finished loading by the time we
    // attached.
    PollReadyState();
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kTimeout)) {
    std::string timeout_ms_ascii =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kTimeout);
    int timeout_ms;
    CHECK(base::StringToInt(timeout_ms_ascii, &timeout_ms))
        << "Expected an integer value for --timeout=";
    browser_->BrowserMainThread()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&HeadlessShell::FetchTimeout, weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(timeout_ms));
  }

  // TODO(skyostil): Implement more features to demonstrate the devtools API.
}
#endif  // !defined(CHROME_MULTIPLE_DLL_CHILD)

void HeadlessShell::FetchTimeout() {
  LOG(INFO) << "Timeout.";
  devtools_client_->GetPage()->GetExperimental()->StopLoading(
      page::StopLoadingParams::Builder().Build());
}

void HeadlessShell::OnTargetCrashed(
    const inspector::TargetCrashedParams& params) {
  LOG(ERROR) << "Abnormal renderer termination.";
  // NB this never gets called if remote debugging is enabled.
  Shutdown();
}

void HeadlessShell::PollReadyState() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // We need to check the current location in addition to the ready state to
  // be sure the expected page is ready.
  devtools_client_->GetRuntime()->Evaluate(
      "document.readyState + ' ' + document.location.href",
      base::Bind(&HeadlessShell::OnReadyState, weak_factory_.GetWeakPtr()));
}

void HeadlessShell::OnReadyState(
    std::unique_ptr<runtime::EvaluateResult> result) {
  if (result->GetResult()->GetValue()->is_string()) {
    std::stringstream stream(result->GetResult()->GetValue()->GetString());
    std::string ready_state;
    std::string url;
    stream >> ready_state;
    stream >> url;

    if (ready_state == "complete" &&
        (url_.spec() == url || url != "about:blank")) {
      OnPageReady();
      return;
    }
  }
}

// emulation::Observer implementation:
void HeadlessShell::OnVirtualTimeBudgetExpired(
    const emulation::VirtualTimeBudgetExpiredParams& params) {
  OnPageReady();
}

// page::Observer implementation:
void HeadlessShell::OnLoadEventFired(const page::LoadEventFiredParams& params) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kVirtualTimeBudget)) {
    return;
  }
  OnPageReady();
}

// network::Observer implementation:
void HeadlessShell::OnRequestIntercepted(
    const network::RequestInterceptedParams& params) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (params.GetIsNavigationRequest()) {
    deterministic_dispatcher_->NavigationRequested(
        std::make_unique<ShellNavigationRequest>(weak_factory_.GetWeakPtr(),
                                                 params.GetInterceptionId()));
    return;
  }
  devtools_client_->GetNetwork()->GetExperimental()->ContinueInterceptedRequest(
      network::ContinueInterceptedRequestParams::Builder()
          .SetInterceptionId(params.GetInterceptionId())
          .Build());
}

void HeadlessShell::OnPageReady() {
  if (processed_page_ready_)
    return;
  processed_page_ready_ = true;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpDom)) {
    FetchDom();
  } else if (base::CommandLine::ForCurrentProcess()->HasSwitch(
                 switches::kRepl)) {
    LOG(INFO)
        << "Type a Javascript expression to evaluate or \"quit\" to exit.";
    InputExpression();
  } else if (base::CommandLine::ForCurrentProcess()->HasSwitch(
                 switches::kScreenshot)) {
    CaptureScreenshot();
  } else if (base::CommandLine::ForCurrentProcess()->HasSwitch(
                 switches::kPrintToPDF)) {
    PrintToPDF();
  } else {
    Shutdown();
  }
}

void HeadlessShell::FetchDom() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  devtools_client_->GetRuntime()->Evaluate(
      "(document.doctype ? new "
      "XMLSerializer().serializeToString(document.doctype) + '\\n' : '') + "
      "document.documentElement.outerHTML",
      base::Bind(&HeadlessShell::OnDomFetched, weak_factory_.GetWeakPtr()));
}

void HeadlessShell::OnDomFetched(
    std::unique_ptr<runtime::EvaluateResult> result) {
  if (result->HasExceptionDetails()) {
    LOG(ERROR) << "Failed to serialize document: "
               << result->GetExceptionDetails()->GetText();
  } else {
    printf("%s\n", result->GetResult()->GetValue()->GetString().c_str());
  }
  Shutdown();
}

void HeadlessShell::InputExpression() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Note that a real system should read user input asynchronously, because
  // otherwise all other browser activity is suspended (e.g., page loading).
  printf(">>> ");
  std::stringstream expression;
  while (true) {
    int c = fgetc(stdin);
    if (c == EOF || c == '\n') {
      break;
    }
    expression << static_cast<char>(c);
  }
  if (expression.str() == "quit") {
    Shutdown();
    return;
  }
  devtools_client_->GetRuntime()->Evaluate(
      expression.str(), base::Bind(&HeadlessShell::OnExpressionResult,
                                   weak_factory_.GetWeakPtr()));
}

void HeadlessShell::OnExpressionResult(
    std::unique_ptr<runtime::EvaluateResult> result) {
  std::unique_ptr<base::Value> value = result->Serialize();
  std::string result_json;
  base::JSONWriter::Write(*value, &result_json);
  printf("%s\n", result_json.c_str());
  InputExpression();
}

void HeadlessShell::CaptureScreenshot() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  devtools_client_->GetPage()->GetExperimental()->CaptureScreenshot(
      page::CaptureScreenshotParams::Builder().Build(),
      base::Bind(&HeadlessShell::OnScreenshotCaptured,
                 weak_factory_.GetWeakPtr()));
}

void HeadlessShell::OnScreenshotCaptured(
    std::unique_ptr<page::CaptureScreenshotResult> result) {
  if (!result) {
    LOG(ERROR) << "Capture screenshot failed";
    Shutdown();
    return;
  }
  WriteFile(switches::kScreenshot, kDefaultScreenshotFileName,
            result->GetData());
}

void HeadlessShell::PrintToPDF() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  devtools_client_->GetPage()->GetExperimental()->PrintToPDF(
      page::PrintToPDFParams::Builder()
          .SetDisplayHeaderFooter(true)
          .SetPrintBackground(true)
          .SetPreferCSSPageSize(true)
          .Build(),
      base::Bind(&HeadlessShell::OnPDFCreated, weak_factory_.GetWeakPtr()));
}

void HeadlessShell::OnPDFCreated(
    std::unique_ptr<page::PrintToPDFResult> result) {
  if (!result) {
    LOG(ERROR) << "Print to PDF failed";
    Shutdown();
    return;
  }
  WriteFile(switches::kPrintToPDF, kDefaultPDFFileName, result->GetData());
}

void HeadlessShell::WriteFile(const std::string& file_path_switch,
                              const std::string& default_file_name,
                              const std::string& base64_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::FilePath file_name =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          file_path_switch);
  if (file_name.empty())
    file_name = base::FilePath().AppendASCII(default_file_name);

  std::string decoded_data;
  if (!base::Base64Decode(base64_data, &decoded_data)) {
    LOG(ERROR) << "Failed to decode base64 data";
    OnFileOpened(std::string(), file_name, base::File::FILE_ERROR_FAILED);
    return;
  }

  file_proxy_ = std::make_unique<base::FileProxy>(file_task_runner_.get());
  if (!file_proxy_->CreateOrOpen(
          file_name, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE,
          base::Bind(&HeadlessShell::OnFileOpened, weak_factory_.GetWeakPtr(),
                     decoded_data, file_name))) {
    // Operation could not be started.
    OnFileOpened(std::string(), file_name, base::File::FILE_ERROR_FAILED);
  }
}

void HeadlessShell::OnFileOpened(const std::string& decoded_data,
                                 const base::FilePath file_name,
                                 base::File::Error error_code) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!file_proxy_->IsValid()) {
    LOG(ERROR) << "Writing to file " << file_name.value()
               << " was unsuccessful, could not open file: "
               << base::File::ErrorToString(error_code);
    return;
  }

  auto buf = base::MakeRefCounted<net::IOBufferWithSize>(decoded_data.size());
  memcpy(buf->data(), decoded_data.data(), decoded_data.size());

  if (!file_proxy_->Write(
          0, buf->data(), buf->size(),
          base::Bind(&HeadlessShell::OnFileWritten, weak_factory_.GetWeakPtr(),
                     file_name, buf->size()))) {
    // Operation may have completed successfully or failed.
    OnFileWritten(file_name, buf->size(), base::File::FILE_ERROR_FAILED, 0);
  }
}

void HeadlessShell::OnFileWritten(const base::FilePath file_name,
                                  const size_t length,
                                  base::File::Error error_code,
                                  int write_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (write_result < static_cast<int>(length)) {
    // TODO(eseckler): Support recovering from partial writes.
    LOG(ERROR) << "Writing to file " << file_name.value()
               << " was unsuccessful: "
               << base::File::ErrorToString(error_code);
  } else {
    LOG(INFO) << "Written to file " << file_name.value() << ".";
  }
  if (!file_proxy_->Close(base::Bind(&HeadlessShell::OnFileClosed,
                                     weak_factory_.GetWeakPtr()))) {
    // Operation could not be started.
    OnFileClosed(base::File::FILE_ERROR_FAILED);
  }
}

void HeadlessShell::OnFileClosed(base::File::Error error_code) {
  Shutdown();
}

bool HeadlessShell::RemoteDebuggingEnabled() const {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  return (command_line.HasSwitch(switches::kRemoteDebuggingPort) ||
          command_line.HasSwitch(switches::kRemoteDebuggingSocketFd));
}

bool ValidateCommandLine(const base::CommandLine& command_line) {
#if !defined(OS_POSIX)
  if (command_line.HasSwitch(switches::kRemoteDebuggingSocketFd)) {
    LOG(ERROR) << "Remote-debugging-socket can't be set on non-Posix systems";
    return false;
  }
#endif
  if (command_line.HasSwitch(switches::kRemoteDebuggingPort) &&
      command_line.HasSwitch(switches::kRemoteDebuggingSocketFd)) {
    LOG(ERROR) << "Remote-debugging-port and remote-debugging-socket "
               << "can't both be set.";
    return false;
  }
  if (!command_line.HasSwitch(switches::kRemoteDebuggingPort) &&
      !command_line.HasSwitch(switches::kRemoteDebuggingSocketFd)) {
    if (command_line.GetArgs().size() <= 1)
      return true;
    LOG(ERROR) << "Open multiple tabs is only supported when "
               << "remote debugging is enabled.";
    return false;
  }
  if (command_line.HasSwitch(switches::kDefaultBackgroundColor)) {
    LOG(ERROR) << "Setting default background color is disabled "
               << "when remote debugging is enabled.";
    return false;
  }
  if (command_line.HasSwitch(switches::kDumpDom)) {
    LOG(ERROR) << "Dump DOM is disabled when remote debugging is enabled.";
    return false;
  }
  if (command_line.HasSwitch(switches::kPrintToPDF)) {
    LOG(ERROR) << "Print to PDF is disabled "
               << "when remote debugging is enabled.";
    return false;
  }
  if (command_line.HasSwitch(switches::kRepl)) {
    LOG(ERROR) << "Evaluate Javascript is disabled "
               << "when remote debugging is enabled.";
    return false;
  }
  if (command_line.HasSwitch(switches::kScreenshot)) {
    LOG(ERROR) << "Capture screenshot is disabled "
               << "when remote debugging is enabled.";
    return false;
  }
  if (command_line.HasSwitch(switches::kTimeout)) {
    LOG(ERROR) << "Navigation timeout is disabled "
               << "when remote debugging is enabled.";
    return false;
  }
  if (command_line.HasSwitch(switches::kVirtualTimeBudget)) {
    LOG(ERROR) << "Virtual time budget is disabled "
               << "when remote debugging is enabled.";
    return false;
  }
  return true;
}

#if defined(OS_WIN)
int HeadlessShellMain(HINSTANCE instance,
                      sandbox::SandboxInterfaceInfo* sandbox_info) {
  base::CommandLine::Init(0, nullptr);
#if defined(HEADLESS_USE_CRASPHAD)
  std::string process_type =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          ::switches::kProcessType);
  if (process_type == crash_reporter::switches::kCrashpadHandler) {
    return crash_reporter::RunAsCrashpadHandler(
        *base::CommandLine::ForCurrentProcess(), base::FilePath(),
        ::switches::kProcessType, switches::kUserDataDir);
  }
#endif  // defined(HEADLESS_USE_CRASPHAD)
  RunChildProcessIfNeeded(instance, sandbox_info);
  HeadlessBrowser::Options::Builder builder(0, nullptr);
  builder.SetInstance(instance);
  builder.SetSandboxInfo(std::move(sandbox_info));
#else
int HeadlessShellMain(int argc, const char** argv) {
  base::CommandLine::Init(argc, argv);
  RunChildProcessIfNeeded(argc, argv);
  HeadlessBrowser::Options::Builder builder(argc, argv);
#endif  // defined(OS_WIN)
  HeadlessShell shell;

#if defined(OS_FUCHSIA)
  // TODO(fuchsia): Remove this when GPU accelerated compositing is ready.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(::switches::kDisableGpu);
#endif

  base::CommandLine& command_line(*base::CommandLine::ForCurrentProcess());
  if (!ValidateCommandLine(command_line))
    return EXIT_FAILURE;

// Crash reporting in headless mode is enabled by default in official builds.
#if defined(GOOGLE_CHROME_BUILD)
  builder.SetCrashReporterEnabled(true);
  base::FilePath dumps_path;
  base::PathService::Get(base::DIR_TEMP, &dumps_path);
  builder.SetCrashDumpsDir(dumps_path);
#endif

  if (command_line.HasSwitch(switches::kEnableBeginFrameControl))
    builder.SetEnableBeginFrameControl(true);

  if (command_line.HasSwitch(switches::kEnableCrashReporter))
    builder.SetCrashReporterEnabled(true);
  if (command_line.HasSwitch(switches::kDisableCrashReporter))
    builder.SetCrashReporterEnabled(false);
  if (command_line.HasSwitch(switches::kCrashDumpsDir)) {
    builder.SetCrashDumpsDir(
        command_line.GetSwitchValuePath(switches::kCrashDumpsDir));
  }

  // Enable devtools if requested, either by specifying a port (and optional
  // address), or by specifying the fd of an already-open socket.
  if (command_line.HasSwitch(::switches::kRemoteDebuggingPort)) {
    std::string address = kUseLocalHostForDevToolsHttpServer;
    if (command_line.HasSwitch(switches::kRemoteDebuggingAddress)) {
      address =
          command_line.GetSwitchValueASCII(switches::kRemoteDebuggingAddress);
      net::IPAddress parsed_address;
      if (!net::ParseURLHostnameToAddress(address, &parsed_address)) {
        LOG(ERROR) << "Invalid devtools server address";
        return EXIT_FAILURE;
      }
    }
    int parsed_port;
    std::string port_str =
        command_line.GetSwitchValueASCII(::switches::kRemoteDebuggingPort);
    if (!base::StringToInt(port_str, &parsed_port) ||
        !base::IsValueInRangeForNumericType<uint16_t>(parsed_port)) {
      LOG(ERROR) << "Invalid devtools server port";
      return EXIT_FAILURE;
    }
    const net::HostPortPair endpoint(address,
                                     base::checked_cast<uint16_t>(parsed_port));
    builder.EnableDevToolsServer(endpoint);
  } else if (command_line.HasSwitch(switches::kRemoteDebuggingSocketFd)) {
    int parsed_fd;
    std::string fd_str =
        command_line.GetSwitchValueASCII(switches::kRemoteDebuggingSocketFd);
    if (!base::StringToInt(fd_str, &parsed_fd) ||
        !base::IsValueInRangeForNumericType<size_t>(parsed_fd)) {
      LOG(ERROR) << "Invalid devtools server socket fd";
      return EXIT_FAILURE;
    }
    builder.EnableDevToolsServer(base::checked_cast<size_t>(parsed_fd));
  }

  if (command_line.HasSwitch(switches::kProxyServer)) {
    std::string proxy_server =
        command_line.GetSwitchValueASCII(switches::kProxyServer);
    auto proxy_config = std::make_unique<net::ProxyConfig>();
    proxy_config->proxy_rules().ParseFromString(proxy_server);
    if (command_line.HasSwitch(switches::kProxyBypassList)) {
      std::string bypass_list =
          command_line.GetSwitchValueASCII(switches::kProxyBypassList);
      proxy_config->proxy_rules().bypass_rules.ParseFromString(bypass_list);
    }
    builder.SetProxyConfig(std::move(proxy_config));
  }

  if (command_line.HasSwitch(::network::switches::kHostResolverRules)) {
    builder.SetHostResolverRules(command_line.GetSwitchValueASCII(
        ::network::switches::kHostResolverRules));
  }

  if (command_line.HasSwitch(switches::kUseGL)) {
    builder.SetGLImplementation(
        command_line.GetSwitchValueASCII(switches::kUseGL));
  }

  if (command_line.HasSwitch(switches::kUserDataDir)) {
    builder.SetUserDataDir(
        command_line.GetSwitchValuePath(switches::kUserDataDir));
    builder.SetIncognitoMode(false);
  }

  if (command_line.HasSwitch(::switches::kInitialVirtualTime)) {
    double initial_time;
    if (base::StringToDouble(
            command_line.GetSwitchValueASCII(::switches::kInitialVirtualTime),
            &initial_time)) {
      builder.SetInitialVirtualTime(base::Time::FromDoubleT(initial_time));
    }
  }

  if (command_line.HasSwitch(switches::kWindowSize)) {
    std::string window_size =
        command_line.GetSwitchValueASCII(switches::kWindowSize);
    gfx::Size parsed_window_size;
    if (!ParseWindowSize(window_size, &parsed_window_size)) {
      LOG(ERROR) << "Malformed window size";
      return EXIT_FAILURE;
    }
    builder.SetWindowSize(parsed_window_size);
  }

  if (command_line.HasSwitch(switches::kHideScrollbars)) {
    builder.SetOverrideWebPreferencesCallback(
        base::Bind([](WebPreferences* preferences) {
          preferences->hide_scrollbars = true;
        }));
  }

  if (command_line.HasSwitch(switches::kUserAgent)) {
    std::string ua = command_line.GetSwitchValueASCII(switches::kUserAgent);
    if (net::HttpUtil::IsValidHeaderValue(ua))
      builder.SetUserAgent(ua);
  }

  if (command_line.HasSwitch(switches::kFontRenderHinting)) {
    std::string font_render_hinting_string =
        command_line.GetSwitchValueASCII(switches::kFontRenderHinting);
    gfx::FontRenderParams::Hinting font_render_hinting;
    if (ParseFontRenderHinting(font_render_hinting_string,
                               &font_render_hinting)) {
      builder.SetFontRenderHinting(font_render_hinting);
    } else {
      LOG(ERROR) << "Unknown font-render-hinting parameter value";
      return EXIT_FAILURE;
    }
  }

  return HeadlessBrowserMain(
      builder.Build(),
      base::Bind(&HeadlessShell::OnStart, base::Unretained(&shell)));
}

int HeadlessShellMain(const content::ContentMainParams& params) {
#if defined(OS_WIN)
  return HeadlessShellMain(params.instance, params.sandbox_info);
#else
  return HeadlessShellMain(params.argc, params.argv);
#endif
}

}  // namespace headless
