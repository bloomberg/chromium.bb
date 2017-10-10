// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_devtools_manager_delegate.h"

#include <string>
#include <utility>

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_frontend_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "headless/grit/headless_lib_resources.h"
#include "headless/lib/browser/headless_browser_context_impl.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_network_conditions.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/public/devtools/domains/target.h"
#include "printing/units.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_util.h"

namespace headless {

namespace {
const char kIdParam[] = "id";
const char kResultParam[] = "result";
const char kErrorParam[] = "error";
const char kErrorCodeParam[] = "code";
const char kErrorMessageParam[] = "message";

// JSON RPC 2.0 spec: http://www.jsonrpc.org/specification#error_object
enum Error {
  kErrorInvalidParam = -32602,
  kErrorServerError = -32000
};

std::unique_ptr<base::DictionaryValue> CreateSuccessResponse(
    int command_id,
    std::unique_ptr<base::Value> result) {
  if (!result)
    result = base::MakeUnique<base::DictionaryValue>();

  auto response = base::MakeUnique<base::DictionaryValue>();
  response->SetInteger(kIdParam, command_id);
  response->Set(kResultParam, std::move(result));
  return response;
}

std::unique_ptr<base::DictionaryValue> CreateErrorResponse(
    int command_id,
    int error_code,
    const std::string& error_message) {
  auto error_object = base::MakeUnique<base::DictionaryValue>();
  error_object->SetInteger(kErrorCodeParam, error_code);
  error_object->SetString(kErrorMessageParam, error_message);

  auto response = base::MakeUnique<base::DictionaryValue>();
  response->SetInteger(kIdParam, command_id);
  response->Set(kErrorParam, std::move(error_object));
  return response;
}

std::unique_ptr<base::DictionaryValue> CreateInvalidParamResponse(
    int command_id,
    const std::string& param) {
  return CreateErrorResponse(
      command_id, kErrorInvalidParam,
      base::StringPrintf("Missing or invalid '%s' parameter", param.c_str()));
}

std::unique_ptr<base::DictionaryValue> CreateBoundsDict(
    const HeadlessWebContentsImpl* web_contents) {
  auto bounds_object = base::MakeUnique<base::DictionaryValue>();
  gfx::Rect bounds = web_contents->web_contents()->GetContainerBounds();
  bounds_object->SetInteger("left", bounds.x());
  bounds_object->SetInteger("top", bounds.y());
  bounds_object->SetInteger("width", bounds.width());
  bounds_object->SetInteger("height", bounds.height());
  bounds_object->SetString("windowState", web_contents->window_state());
  return bounds_object;
}

#if BUILDFLAG(ENABLE_BASIC_PRINTING)
void PDFCreated(
    const content::DevToolsManagerDelegate::CommandCallback& callback,
    int command_id,
    HeadlessPrintManager::PrintResult print_result,
    const std::string& data) {
  std::unique_ptr<base::DictionaryValue> response;
  if (print_result == HeadlessPrintManager::PRINT_SUCCESS) {
    response = CreateSuccessResponse(
        command_id, HeadlessPrintManager::PDFContentsToDictionaryValue(data));
  } else {
    response = CreateErrorResponse(
        command_id, kErrorServerError,
        HeadlessPrintManager::PrintResultToString(print_result));
  }
  callback.Run(std::move(response));
}
#endif

std::string ToString(std::unique_ptr<base::DictionaryValue> value) {
  std::string json;
  base::JSONWriter::Write(*value, &json);
  return json;
}

constexpr const char kPng[] = "png";
constexpr const char kJpeg[] = "jpeg";
enum class ImageEncoding { kPng, kJpeg };
constexpr int kDefaultScreenshotQuality = 80;

std::string EncodeBitmap(const SkBitmap& bitmap,
                         ImageEncoding encoding,
                         int quality) {
  gfx::Image image = gfx::Image::CreateFrom1xBitmap(bitmap);
  DCHECK(!image.IsEmpty());

  scoped_refptr<base::RefCountedMemory> data;
  if (encoding == ImageEncoding::kPng) {
    data = image.As1xPNGBytes();
  } else if (encoding == ImageEncoding::kJpeg) {
    scoped_refptr<base::RefCountedBytes> bytes(new base::RefCountedBytes());
    if (gfx::JPEG1xEncodedDataFromImage(image, quality, &bytes->data()))
      data = bytes;
  }

  if (!data || !data->front())
    return std::string();

  std::string base_64_data;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(data->front()),
                        data->size()),
      &base_64_data);

  return base_64_data;
}

void OnBeginFrameFinished(
    int command_id,
    const HeadlessDevToolsManagerDelegate::CommandCallback& callback,
    ImageEncoding encoding,
    int quality,
    bool has_damage,
    std::unique_ptr<SkBitmap> bitmap) {
  auto result = base::MakeUnique<base::DictionaryValue>();
  result->SetBoolean("hasDamage", has_damage);

  if (bitmap && !bitmap->drawsNothing()) {
    result->SetString("screenshotData",
                      EncodeBitmap(*bitmap, encoding, quality));
  }

  callback.Run(CreateSuccessResponse(command_id, std::move(result)));
}
}  // namespace

#if BUILDFLAG(ENABLE_BASIC_PRINTING)
namespace {
// The max and min value should match the ones in scaling_settings.html.
// Update both files at the same time.
const double kScaleMaxVal = 200;
const double kScaleMinVal = 10;
}

std::unique_ptr<base::DictionaryValue> ParsePrintSettings(
    int command_id,
    const base::DictionaryValue* params,
    HeadlessPrintSettings* settings) {
  // We can safely ignore the return values of the following Get methods since
  // the defaults are already set in |settings|.
  params->GetBoolean("landscape", &settings->landscape);
  params->GetBoolean("displayHeaderFooter", &settings->display_header_footer);
  params->GetBoolean("printBackground", &settings->should_print_backgrounds);
  params->GetDouble("scale", &settings->scale);
  if (settings->scale > kScaleMaxVal / 100 ||
      settings->scale < kScaleMinVal / 100)
    return CreateInvalidParamResponse(command_id, "scale");
  params->GetString("pageRanges", &settings->page_ranges);
  params->GetBoolean("ignoreInvalidPageRanges",
                     &settings->ignore_invalid_page_ranges);

  double paper_width_in_inch = printing::kLetterWidthInch;
  double paper_height_in_inch = printing::kLetterHeightInch;
  params->GetDouble("paperWidth", &paper_width_in_inch);
  params->GetDouble("paperHeight", &paper_height_in_inch);
  if (paper_width_in_inch <= 0)
    return CreateInvalidParamResponse(command_id, "paperWidth");
  if (paper_height_in_inch <= 0)
    return CreateInvalidParamResponse(command_id, "paperHeight");
  settings->paper_size_in_points =
      gfx::Size(paper_width_in_inch * printing::kPointsPerInch,
                paper_height_in_inch * printing::kPointsPerInch);

  // Set default margin to 1.0cm = ~2/5 of an inch.
  double default_margin_in_inch = 1000.0 / printing::kHundrethsMMPerInch;
  double margin_top_in_inch = default_margin_in_inch;
  double margin_bottom_in_inch = default_margin_in_inch;
  double margin_left_in_inch = default_margin_in_inch;
  double margin_right_in_inch = default_margin_in_inch;
  params->GetDouble("marginTop", &margin_top_in_inch);
  params->GetDouble("marginBottom", &margin_bottom_in_inch);
  params->GetDouble("marginLeft", &margin_left_in_inch);
  params->GetDouble("marginRight", &margin_right_in_inch);
  if (margin_top_in_inch < 0)
    return CreateInvalidParamResponse(command_id, "marginTop");
  if (margin_bottom_in_inch < 0)
    return CreateInvalidParamResponse(command_id, "marginBottom");
  if (margin_left_in_inch < 0)
    return CreateInvalidParamResponse(command_id, "marginLeft");
  if (margin_right_in_inch < 0)
    return CreateInvalidParamResponse(command_id, "marginRight");
  settings->margins_in_points.top =
      margin_top_in_inch * printing::kPointsPerInch;
  settings->margins_in_points.bottom =
      margin_bottom_in_inch * printing::kPointsPerInch;
  settings->margins_in_points.left =
      margin_left_in_inch * printing::kPointsPerInch;
  settings->margins_in_points.right =
      margin_right_in_inch * printing::kPointsPerInch;

  return nullptr;
}
#endif

HeadlessDevToolsManagerDelegate::HeadlessDevToolsManagerDelegate(
    base::WeakPtr<HeadlessBrowserImpl> browser)
    : browser_(std::move(browser)) {
  // TODO(eseckler): Use third_party/inspector_protocol to generate harnesses
  // for commands, rather than binding commands here manually.
  command_map_["Target.createTarget"] = base::Bind(
      &HeadlessDevToolsManagerDelegate::CreateTarget, base::Unretained(this));
  command_map_["Target.closeTarget"] = base::Bind(
      &HeadlessDevToolsManagerDelegate::CloseTarget, base::Unretained(this));
  command_map_["Target.createBrowserContext"] =
      base::Bind(&HeadlessDevToolsManagerDelegate::CreateBrowserContext,
                 base::Unretained(this));
  command_map_["Target.disposeBrowserContext"] =
      base::Bind(&HeadlessDevToolsManagerDelegate::DisposeBrowserContext,
                 base::Unretained(this));
  command_map_["Browser.getWindowForTarget"] =
      base::Bind(&HeadlessDevToolsManagerDelegate::GetWindowForTarget,
                 base::Unretained(this));
  command_map_["Browser.getWindowBounds"] =
      base::Bind(&HeadlessDevToolsManagerDelegate::GetWindowBounds,
                 base::Unretained(this));
  command_map_["Browser.setWindowBounds"] =
      base::Bind(&HeadlessDevToolsManagerDelegate::SetWindowBounds,
                 base::Unretained(this));
  command_map_["HeadlessExperimental.enable"] =
      base::Bind(&HeadlessDevToolsManagerDelegate::EnableHeadlessExperimental,
                 base::Unretained(this));
  command_map_["HeadlessExperimental.disable"] =
      base::Bind(&HeadlessDevToolsManagerDelegate::DisableHeadlessExperimental,
                 base::Unretained(this));

  unhandled_command_map_["Network.emulateNetworkConditions"] =
      base::Bind(&HeadlessDevToolsManagerDelegate::EmulateNetworkConditions,
                 base::Unretained(this));
  unhandled_command_map_["Network.disable"] = base::Bind(
      &HeadlessDevToolsManagerDelegate::NetworkDisable, base::Unretained(this));

  async_command_map_["Page.printToPDF"] = base::Bind(
      &HeadlessDevToolsManagerDelegate::PrintToPDF, base::Unretained(this));
  async_command_map_["HeadlessExperimental.beginFrame"] = base::Bind(
      &HeadlessDevToolsManagerDelegate::BeginFrame, base::Unretained(this));
}

HeadlessDevToolsManagerDelegate::~HeadlessDevToolsManagerDelegate() {}

bool HeadlessDevToolsManagerDelegate::HandleCommand(
    content::DevToolsAgentHost* agent_host,
    int session_id,
    base::DictionaryValue* command) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!browser_)
    return false;

  int id;
  std::string method;
  if (!command->GetInteger("id", &id) || !command->GetString("method", &method))
    return false;

  const base::DictionaryValue* params = nullptr;
  command->GetDictionary("params", &params);

  auto find_it = command_map_.find(method);
  if (find_it == command_map_.end()) {
    // Check for any commands that are actioned then passed on to devtools to
    // handle.
    find_it = unhandled_command_map_.find(method);
    if (find_it != unhandled_command_map_.end())
      find_it->second.Run(agent_host, session_id, id, params);
    return false;
  }

  // Handle Browser domain commands only from Browser DevToolsAgentHost.
  if (method.find("Browser.") == 0 &&
      agent_host->GetType() != content::DevToolsAgentHost::kTypeBrowser)
    return false;

  auto cmd_result = find_it->second.Run(agent_host, session_id, id, params);
  if (!cmd_result)
    return false;
  agent_host->SendProtocolMessageToClient(session_id,
                                          ToString(std::move(cmd_result)));
  return true;
}

bool HeadlessDevToolsManagerDelegate::HandleAsyncCommand(
    content::DevToolsAgentHost* agent_host,
    int session_id,
    base::DictionaryValue* command,
    const CommandCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!browser_)
    return false;

  int id;
  std::string method;
  if (!command->GetInteger("id", &id) || !command->GetString("method", &method))
    return false;

  auto find_it = async_command_map_.find(method);
  if (find_it == async_command_map_.end())
    return false;

  const base::DictionaryValue* params = nullptr;
  command->GetDictionary("params", &params);
  find_it->second.Run(agent_host, session_id, id, params, callback);
  return true;
}

scoped_refptr<content::DevToolsAgentHost>
HeadlessDevToolsManagerDelegate::CreateNewTarget(const GURL& url) {
  if (!browser_)
    return nullptr;

  HeadlessBrowserContext* context = browser_->GetDefaultBrowserContext();
  HeadlessWebContentsImpl* web_contents_impl = HeadlessWebContentsImpl::From(
      context->CreateWebContentsBuilder()
          .SetInitialURL(url)
          .SetWindowSize(browser_->options()->window_size)
          .Build());
  return content::DevToolsAgentHost::GetOrCreateFor(
      web_contents_impl->web_contents());
}

std::string HeadlessDevToolsManagerDelegate::GetDiscoveryPageHTML() {
  return ui::ResourceBundle::GetSharedInstance()
      .GetRawDataResource(IDR_HEADLESS_LIB_DEVTOOLS_DISCOVERY_PAGE)
      .as_string();
}

std::string HeadlessDevToolsManagerDelegate::GetFrontendResource(
    const std::string& path) {
  return content::DevToolsFrontendHost::GetFrontendResource(path).as_string();
}

void HeadlessDevToolsManagerDelegate::SessionDestroyed(
    content::DevToolsAgentHost* agent_host,
    int session_id) {
  if (!browser_)
    return;

  content::WebContents* web_contents = agent_host->GetWebContents();
  if (!web_contents)
    return;

  HeadlessWebContentsImpl* headless_contents =
      HeadlessWebContentsImpl::From(browser_.get(), web_contents);
  if (!headless_contents)
    return;

  headless_contents->SetBeginFrameEventsEnabled(session_id, false);
}

void HeadlessDevToolsManagerDelegate::PrintToPDF(
    content::DevToolsAgentHost* agent_host,
    int session_id,
    int command_id,
    const base::DictionaryValue* params,
    const CommandCallback& callback) {
  DCHECK(callback);

#if BUILDFLAG(ENABLE_BASIC_PRINTING)
  content::WebContents* web_contents = agent_host->GetWebContents();
  if (!web_contents) {
    callback.Run(CreateErrorResponse(command_id, kErrorServerError,
                                     "Command not supported on this endpoint"));
    return;
  }
  content::RenderFrameHost* rfh = web_contents->GetMainFrame();

  HeadlessPrintSettings settings;
  std::unique_ptr<base::DictionaryValue> response =
      ParsePrintSettings(command_id, params, &settings);
  if (response) {
    callback.Run(std::move(response));
    return;
  }
  HeadlessPrintManager::FromWebContents(web_contents)
      ->GetPDFContents(rfh, settings,
                       base::Bind(&PDFCreated, callback, command_id));
#else
  callback.Run(CreateErrorResponse(command_id, kErrorServerError,
                                   "Printing is not enabled"));
#endif
}

std::unique_ptr<base::DictionaryValue>
HeadlessDevToolsManagerDelegate::CreateTarget(
    content::DevToolsAgentHost* agent_host,
    int session_id,
    int command_id,
    const base::DictionaryValue* params) {
  std::string url;
  std::string browser_context_id;
  int width = browser_->options()->window_size.width();
  int height = browser_->options()->window_size.height();
  if (!params || !params->GetString("url", &url))
    return CreateInvalidParamResponse(command_id, "url");
  bool enable_begin_frame_control = false;
  params->GetString("browserContextId", &browser_context_id);
  params->GetInteger("width", &width);
  params->GetInteger("height", &height);
  params->GetBoolean("enableBeginFrameControl", &enable_begin_frame_control);

#if defined(OS_MACOSX)
  if (enable_begin_frame_control) {
    return CreateErrorResponse(
        command_id, kErrorServerError,
        "BeginFrameControl is not supported on MacOS yet");
  }
#endif

  HeadlessBrowserContext* context =
      browser_->GetBrowserContextForId(browser_context_id);
  if (!browser_context_id.empty()) {
    context = browser_->GetBrowserContextForId(browser_context_id);
    if (!context)
      return CreateInvalidParamResponse(command_id, "browserContextId");
  } else {
    context = browser_->GetDefaultBrowserContext();
    if (!context) {
      return CreateErrorResponse(command_id, kErrorServerError,
                                 "You specified no |browserContextId|, but "
                                 "there is no default browser context set on "
                                 "HeadlessBrowser");
    }
  }

  HeadlessWebContentsImpl* web_contents_impl = HeadlessWebContentsImpl::From(
      context->CreateWebContentsBuilder()
          .SetInitialURL(GURL(url))
          .SetWindowSize(gfx::Size(width, height))
          .SetEnableBeginFrameControl(enable_begin_frame_control)
          .Build());

  std::unique_ptr<base::Value> result(
      target::CreateTargetResult::Builder()
          .SetTargetId(web_contents_impl->GetDevToolsAgentHostId())
          .Build()
          ->Serialize());
  return CreateSuccessResponse(command_id, std::move(result));
}

std::unique_ptr<base::DictionaryValue>
HeadlessDevToolsManagerDelegate::CloseTarget(
    content::DevToolsAgentHost* agent_host,
    int session_id,
    int command_id,
    const base::DictionaryValue* params) {
  std::string target_id;
  if (!params || !params->GetString("targetId", &target_id))
    return CreateInvalidParamResponse(command_id, "targetId");
  HeadlessWebContents* web_contents =
      browser_->GetWebContentsForDevToolsAgentHostId(target_id);
  bool success = false;
  if (web_contents) {
    web_contents->Close();
    success = true;
  }
  std::unique_ptr<base::Value> result(target::CloseTargetResult::Builder()
                                          .SetSuccess(success)
                                          .Build()
                                          ->Serialize());
  return CreateSuccessResponse(command_id, std::move(result));
}

std::unique_ptr<base::DictionaryValue>
HeadlessDevToolsManagerDelegate::CreateBrowserContext(
    content::DevToolsAgentHost* agent_host,
    int session_id,
    int command_id,
    const base::DictionaryValue* params) {
  HeadlessBrowserContext* browser_context =
      browser_->CreateBrowserContextBuilder().Build();

  std::unique_ptr<base::Value> result(
      target::CreateBrowserContextResult::Builder()
          .SetBrowserContextId(browser_context->Id())
          .Build()
          ->Serialize());
  return CreateSuccessResponse(command_id, std::move(result));
}

std::unique_ptr<base::DictionaryValue>
HeadlessDevToolsManagerDelegate::DisposeBrowserContext(
    content::DevToolsAgentHost* agent_host,
    int session_id,
    int command_id,
    const base::DictionaryValue* params) {
  std::string browser_context_id;
  if (!params || !params->GetString("browserContextId", &browser_context_id))
    return CreateInvalidParamResponse(command_id, "browserContextId");
  HeadlessBrowserContext* context =
      browser_->GetBrowserContextForId(browser_context_id);

  bool success = false;
  if (context && context != browser_->GetDefaultBrowserContext() &&
      context->GetAllWebContents().empty()) {
    success = true;
    context->Close();
  }

  std::unique_ptr<base::Value> result(
      target::DisposeBrowserContextResult::Builder()
          .SetSuccess(success)
          .Build()
          ->Serialize());
  return CreateSuccessResponse(command_id, std::move(result));
}

std::unique_ptr<base::DictionaryValue>
HeadlessDevToolsManagerDelegate::GetWindowForTarget(
    content::DevToolsAgentHost* agent_host,
    int session_id,
    int command_id,
    const base::DictionaryValue* params) {
  std::string target_id;
  if (!params->GetString("targetId", &target_id))
    return CreateInvalidParamResponse(command_id, "targetId");

  HeadlessWebContentsImpl* web_contents = HeadlessWebContentsImpl::From(
      browser_->GetWebContentsForDevToolsAgentHostId(target_id));
  if (!web_contents) {
    return CreateErrorResponse(command_id, kErrorServerError,
                               "No web contents for the given target id");
  }

  auto result = base::MakeUnique<base::DictionaryValue>();
  result->SetInteger("windowId", web_contents->window_id());
  result->Set("bounds", CreateBoundsDict(web_contents));
  return CreateSuccessResponse(command_id, std::move(result));
}

std::unique_ptr<base::DictionaryValue>
HeadlessDevToolsManagerDelegate::GetWindowBounds(
    content::DevToolsAgentHost* agent_host,
    int session_id,
    int command_id,
    const base::DictionaryValue* params) {
  int window_id;
  if (!params->GetInteger("windowId", &window_id))
    return CreateInvalidParamResponse(command_id, "windowId");
  HeadlessWebContentsImpl* web_contents =
      browser_->GetWebContentsForWindowId(window_id);
  if (!web_contents) {
    return CreateErrorResponse(command_id, kErrorServerError,
                               "Browser window not found");
  }

  auto result = base::MakeUnique<base::DictionaryValue>();
  result->Set("bounds", CreateBoundsDict(web_contents));
  return CreateSuccessResponse(command_id, std::move(result));
}

std::unique_ptr<base::DictionaryValue>
HeadlessDevToolsManagerDelegate::SetWindowBounds(
    content::DevToolsAgentHost* agent_host,
    int session_id,
    int command_id,
    const base::DictionaryValue* params) {
  int window_id;
  if (!params->GetInteger("windowId", &window_id))
    return CreateInvalidParamResponse(command_id, "windowId");
  HeadlessWebContentsImpl* web_contents =
      browser_->GetWebContentsForWindowId(window_id);
  if (!web_contents) {
    return CreateErrorResponse(command_id, kErrorServerError,
                               "Browser window not found");
  }

  const base::Value* value = nullptr;
  const base::DictionaryValue* bounds_dict = nullptr;
  if (!params->Get("bounds", &value) || !value->GetAsDictionary(&bounds_dict))
    return CreateInvalidParamResponse(command_id, "bounds");

  std::string window_state;
  if (!bounds_dict->GetString("windowState", &window_state)) {
    window_state = "normal";
  } else if (window_state != "normal" && window_state != "minimized" &&
             window_state != "maximized" && window_state != "fullscreen") {
    return CreateInvalidParamResponse(command_id, "windowState");
  }

  // Compute updated bounds when window state is normal.
  bool set_bounds = false;
  gfx::Rect bounds = web_contents->web_contents()->GetContainerBounds();
  int left, top, width, height;
  if (bounds_dict->GetInteger("left", &left)) {
    bounds.set_x(left);
    set_bounds = true;
  }
  if (bounds_dict->GetInteger("top", &top)) {
    bounds.set_y(top);
    set_bounds = true;
  }
  if (bounds_dict->GetInteger("width", &width)) {
    if (width < 0)
      return CreateInvalidParamResponse(command_id, "width");
    bounds.set_width(width);
    set_bounds = true;
  }
  if (bounds_dict->GetInteger("height", &height)) {
    if (height < 0)
      return CreateInvalidParamResponse(command_id, "height");
    bounds.set_height(height);
    set_bounds = true;
  }

  if (set_bounds && window_state != "normal") {
    return CreateErrorResponse(
        command_id, kErrorServerError,
        "The 'minimized', 'maximized' and 'fullscreen' states cannot be "
        "combined with 'left', 'top', 'width' or 'height'");
  }

  if (set_bounds && web_contents->window_state() != "normal") {
    return CreateErrorResponse(
        command_id, kErrorServerError,
        "To resize minimized/maximized/fullscreen window, restore it to normal "
        "state first.");
  }

  web_contents->set_window_state(window_state);
  web_contents->SetBounds(bounds);
  return CreateSuccessResponse(command_id, nullptr);
}

std::unique_ptr<base::DictionaryValue>
HeadlessDevToolsManagerDelegate::EmulateNetworkConditions(
    content::DevToolsAgentHost* agent_host,
    int session_id,
    int command_id,
    const base::DictionaryValue* params) {
  // Associate NetworkConditions to context
  HeadlessBrowserContextImpl* browser_context =
      static_cast<HeadlessBrowserContextImpl*>(
          browser_->GetDefaultBrowserContext());
  bool offline = false;
  double latency = 0, download_throughput = 0, upload_throughput = 0;
  params->GetBoolean("offline", &offline);
  params->GetDouble("latency", &latency);
  params->GetDouble("downloadThroughput", &download_throughput);
  params->GetDouble("uploadThroughput", &upload_throughput);
  HeadlessNetworkConditions conditions(HeadlessNetworkConditions(
      offline, std::max(latency, 0.0), std::max(download_throughput, 0.0),
      std::max(upload_throughput, 0.0)));
  browser_context->SetNetworkConditions(conditions);
  return CreateSuccessResponse(command_id, nullptr);
}

std::unique_ptr<base::DictionaryValue>
HeadlessDevToolsManagerDelegate::NetworkDisable(
    content::DevToolsAgentHost* agent_host,
    int session_id,
    int command_id,
    const base::DictionaryValue* params) {
  HeadlessBrowserContextImpl* browser_context =
      static_cast<HeadlessBrowserContextImpl*>(
          browser_->GetDefaultBrowserContext());
  browser_context->SetNetworkConditions(HeadlessNetworkConditions());
  return CreateSuccessResponse(command_id, nullptr);
}

std::unique_ptr<base::DictionaryValue>
HeadlessDevToolsManagerDelegate::EnableHeadlessExperimental(
    content::DevToolsAgentHost* agent_host,
    int session_id,
    int command_id,
    const base::DictionaryValue* params) {
  content::WebContents* web_contents = agent_host->GetWebContents();
  if (!web_contents) {
    return CreateErrorResponse(command_id, kErrorServerError,
                               "Command not supported on this endpoint");
  }

  HeadlessWebContentsImpl* headless_contents =
      HeadlessWebContentsImpl::From(browser_.get(), web_contents);
  headless_contents->SetBeginFrameEventsEnabled(session_id, true);
  return CreateSuccessResponse(command_id, nullptr);
}

std::unique_ptr<base::DictionaryValue>
HeadlessDevToolsManagerDelegate::DisableHeadlessExperimental(
    content::DevToolsAgentHost* agent_host,
    int session_id,
    int command_id,
    const base::DictionaryValue* params) {
  content::WebContents* web_contents = agent_host->GetWebContents();
  if (!web_contents) {
    return CreateErrorResponse(command_id, kErrorServerError,
                               "Command not supported on this endpoint");
  }

  HeadlessWebContentsImpl* headless_contents =
      HeadlessWebContentsImpl::From(browser_.get(), web_contents);
  headless_contents->SetBeginFrameEventsEnabled(session_id, false);
  return CreateSuccessResponse(command_id, nullptr);
}

void HeadlessDevToolsManagerDelegate::BeginFrame(
    content::DevToolsAgentHost* agent_host,
    int session_id,
    int command_id,
    const base::DictionaryValue* params,
    const CommandCallback& callback) {
  DCHECK(callback);

  content::WebContents* web_contents = agent_host->GetWebContents();
  if (!web_contents) {
    callback.Run(CreateErrorResponse(command_id, kErrorServerError,
                                     "Command not supported on this endpoint"));
    return;
  }

  HeadlessWebContentsImpl* headless_contents =
      HeadlessWebContentsImpl::From(browser_.get(), web_contents);
  if (!headless_contents->begin_frame_control_enabled()) {
    callback.Run(CreateErrorResponse(
        command_id, kErrorServerError,
        "Command is only supported if BeginFrameControl is enabled."));
    return;
  }

  double frame_time_double = 0;
  double deadline_double = 0;
  double interval_double = 0;

  base::Time frame_time;
  base::TimeTicks frame_timeticks;
  base::TimeTicks deadline;
  base::TimeDelta interval;

  if (params->GetDouble("frameTime", &frame_time_double)) {
    frame_time = base::Time::FromDoubleT(frame_time_double);
    base::TimeDelta delta = frame_time - base::Time::UnixEpoch();
    frame_timeticks = base::TimeTicks::UnixEpoch() + delta;
  } else {
    frame_timeticks = base::TimeTicks::Now();
  }

  if (params->GetDouble("interval", &interval_double)) {
    if (interval_double <= 0) {
      callback.Run(CreateErrorResponse(command_id, kErrorInvalidParam,
                                       "interval has to be greater than 0"));
      return;
    }
    interval = base::TimeDelta::FromMillisecondsD(interval_double);
  } else {
    interval = viz::BeginFrameArgs::DefaultInterval();
  }

  if (params->GetDouble("deadline", &deadline_double)) {
    base::TimeDelta delta =
        base::Time::FromDoubleT(deadline_double) - frame_time;
    if (delta <= base::TimeDelta()) {
      callback.Run(CreateErrorResponse(command_id, kErrorInvalidParam,
                                       "deadline has to be after frameTime"));
      return;
    }
    deadline = frame_timeticks + delta;
  } else {
    deadline = frame_timeticks + interval;
  }

  bool capture_screenshot = false;
  ImageEncoding encoding = ImageEncoding::kPng;
  int quality = kDefaultScreenshotQuality;

  const base::Value* value = nullptr;
  const base::DictionaryValue* screenshot_dict = nullptr;
  if (params->Get("screenshot", &value)) {
    if (!value->GetAsDictionary(&screenshot_dict)) {
      callback.Run(CreateInvalidParamResponse(command_id, "screenshot"));
      return;
    }

    capture_screenshot = true;

    std::string format;
    if (screenshot_dict->GetString("format", &format)) {
      if (format == kPng) {
        encoding = ImageEncoding::kPng;
      } else if (format == kJpeg) {
        encoding = ImageEncoding::kJpeg;
      } else {
        callback.Run(
            CreateInvalidParamResponse(command_id, "screenshot.format"));
        return;
      }
    }

    if (screenshot_dict->GetInteger("quality", &quality) &&
        (quality < 0 || quality > 100)) {
      callback.Run(
          CreateErrorResponse(command_id, kErrorInvalidParam,
                              "screenshot.quality has to be in range 0..100"));
      return;
    }
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          cc::switches::kRunAllCompositorStagesBeforeDraw) &&
      headless_contents->HasPendingFrame()) {
    LOG(WARNING) << "A BeginFrame is already in flight. In "
                    "--run-all-compositor-stages-before-draw mode, only a "
                    "single BeginFrame should be active at the same time.";
  }

  headless_contents->BeginFrame(frame_timeticks, deadline, interval,
                                capture_screenshot,
                                base::Bind(&OnBeginFrameFinished, command_id,
                                           callback, encoding, quality));
}

}  // namespace headless
