// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_ui.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/accessibility/accessibility_tree_formatter.h"
#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webui/web_ui_data_source_impl.h"
#include "content/common/view_message_enums.h"
#include "content/grit/content_resources.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "net/base/escape.h"

static const char kDataFile[] = "targets-data.json";

static const char kProcessIdField[]  = "processId";
static const char kRouteIdField[]  = "routeId";
static const char kUrlField[]  = "url";
static const char kNameField[]  = "name";
static const char kFaviconUrlField[] = "favicon_url";
static const char kPidField[]  = "pid";
static const char kAccessibilityModeField[] = "a11y_mode";

// Global flags
static const char kInternal[] = "internal";
static const char kNative[] = "native";
static const char kWeb[] = "web";
static const char kText[] = "text";
static const char kScreenReader[] = "screenreader";
static const char kHTML[] = "html";

// Possible global flag values
static const char kOff[]= "off";
static const char kOn[] = "on";
static const char kDisabled[] = "disabled";

namespace content {

namespace {

bool g_show_internal_accessibility_tree = false;

std::unique_ptr<base::DictionaryValue> BuildTargetDescriptor(
    const GURL& url,
    const std::string& name,
    const GURL& favicon_url,
    int process_id,
    int route_id,
    AccessibilityMode accessibility_mode,
    base::ProcessHandle handle = base::kNullProcessHandle) {
  std::unique_ptr<base::DictionaryValue> target_data(
      new base::DictionaryValue());
  target_data->SetInteger(kProcessIdField, process_id);
  target_data->SetInteger(kRouteIdField, route_id);
  target_data->SetString(kUrlField, url.spec());
  target_data->SetString(kNameField, net::EscapeForHTML(name));
  target_data->SetInteger(kPidField, base::GetProcId(handle));
  target_data->SetString(kFaviconUrlField, favicon_url.spec());
  target_data->SetInteger(kAccessibilityModeField, accessibility_mode);
  return target_data;
}

std::unique_ptr<base::DictionaryValue> BuildTargetDescriptor(
    RenderViewHost* rvh) {
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      WebContents::FromRenderViewHost(rvh));
  AccessibilityMode accessibility_mode = AccessibilityModeOff;

  std::string title;
  GURL url;
  GURL favicon_url;
  if (web_contents) {
    // TODO(nasko): Fix the following code to use a consistent set of data
    // across the URL, title, and favicon.
    url = web_contents->GetURL();
    title = base::UTF16ToUTF8(web_contents->GetTitle());
    NavigationController& controller = web_contents->GetController();
    NavigationEntry* entry = controller.GetVisibleEntry();
    if (entry != NULL && entry->GetURL().is_valid())
      favicon_url = entry->GetFavicon().url;
    accessibility_mode = web_contents->GetAccessibilityMode();
  }

  return BuildTargetDescriptor(url,
                               title,
                               favicon_url,
                               rvh->GetProcess()->GetID(),
                               rvh->GetRoutingID(),
                               accessibility_mode);
}

bool HandleRequestCallback(BrowserContext* current_context,
                           const std::string& path,
                           const WebUIDataSource::GotDataCallback& callback) {
  if (path != kDataFile)
    return false;
  std::unique_ptr<base::ListValue> rvh_list(new base::ListValue());

  std::unique_ptr<RenderWidgetHostIterator> widgets(
      RenderWidgetHost::GetRenderWidgetHosts());

  while (RenderWidgetHost* widget = widgets->GetNextHost()) {
    // Ignore processes that don't have a connection, such as crashed tabs.
    if (!widget->GetProcess()->HasConnection())
      continue;
    RenderViewHost* rvh = RenderViewHost::From(widget);
    if (!rvh)
      continue;
    // Ignore views that are never visible, like background pages.
    if (static_cast<RenderViewHostImpl*>(rvh)->GetDelegate()->IsNeverVisible())
      continue;
    BrowserContext* context = rvh->GetProcess()->GetBrowserContext();
    if (context != current_context)
      continue;

    rvh_list->Append(BuildTargetDescriptor(rvh));
  }

  base::DictionaryValue data;
  data.Set("list", rvh_list.release());
  AccessibilityMode mode =
      BrowserAccessibilityStateImpl::GetInstance()->accessibility_mode();
  bool disabled = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableRendererAccessibility);
  bool native = 0 != (mode & ACCESSIBILITY_MODE_FLAG_NATIVE_APIS);
  bool web = 0 != (mode & ACCESSIBILITY_MODE_FLAG_WEB_CONTENTS);
  bool text = 0 != (mode & ACCESSIBILITY_MODE_FLAG_INLINE_TEXT_BOXES);
  bool screenreader = 0 != (mode & ACCESSIBILITY_MODE_FLAG_SCREEN_READER);
  bool html = 0 != (mode & ACCESSIBILITY_MODE_FLAG_HTML);

  // The "native" and "web" flags are disabled if
  // --disable-renderer-accessibility is set.
  data.SetString(kNative, disabled ? kDisabled : (native ? kOn : kOff));
  data.SetString(kWeb, disabled ? kDisabled : (web ? kOn : kOff));

  // The "text", "screenreader", and "html" flags are only meaningful if
  // "web" is enabled.
  data.SetString(kText, web ? (text ? kOn : kOff) : kDisabled);
  data.SetString(kScreenReader, web ? (screenreader ? kOn : kOff) : kDisabled);
  data.SetString(kHTML, web ? (html ? kOn : kOff) : kDisabled);

  data.SetString(kInternal,
                 g_show_internal_accessibility_tree ? kOn : kOff);

  std::string json_string;
  base::JSONWriter::Write(data, &json_string);

  callback.Run(base::RefCountedString::TakeString(&json_string));
  return true;
}

}  // namespace

AccessibilityUI::AccessibilityUI(WebUI* web_ui) : WebUIController(web_ui) {
  // Set up the chrome://accessibility source.
  WebUIDataSourceImpl* html_source = static_cast<WebUIDataSourceImpl*>(
      WebUIDataSource::Create(kChromeUIAccessibilityHost));

  web_ui->RegisterMessageCallback(
      "toggleAccessibility",
      base::Bind(&AccessibilityUI::ToggleAccessibility,
                 base::Unretained(this)));
  web_ui->RegisterMessageCallback(
      "setGlobalFlag",
      base::Bind(&AccessibilityUI::SetGlobalFlag,
                 base::Unretained(this)));
  web_ui->RegisterMessageCallback(
      "requestAccessibilityTree",
      base::Bind(&AccessibilityUI::RequestAccessibilityTree,
                 base::Unretained(this)));

  // Add required resources.
  html_source->SetJsonPath("strings.js");
  html_source->AddResourcePath("accessibility.css", IDR_ACCESSIBILITY_CSS);
  html_source->AddResourcePath("accessibility.js", IDR_ACCESSIBILITY_JS);
  html_source->SetDefaultResource(IDR_ACCESSIBILITY_HTML);
  html_source->SetRequestFilter(
      base::Bind(&HandleRequestCallback,
                 web_ui->GetWebContents()->GetBrowserContext()));

  std::unordered_set<std::string> exclude_from_gzip;
  exclude_from_gzip.insert(kDataFile);
  html_source->UseGzip(exclude_from_gzip);

  BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  WebUIDataSource::Add(browser_context, html_source);
}

AccessibilityUI::~AccessibilityUI() {}

void AccessibilityUI::ToggleAccessibility(const base::ListValue* args) {
  std::string process_id_str;
  std::string route_id_str;
  int process_id;
  int route_id;
  int mode;
  CHECK_EQ(3U, args->GetSize());
  CHECK(args->GetString(0, &process_id_str));
  CHECK(args->GetString(1, &route_id_str));
  CHECK(args->GetInteger(2, &mode));
  CHECK(base::StringToInt(process_id_str, &process_id));
  CHECK(base::StringToInt(route_id_str, &route_id));

  RenderViewHost* rvh = RenderViewHost::FromID(process_id, route_id);
  if (!rvh)
    return;
  auto* web_contents =
      static_cast<WebContentsImpl*>(WebContents::FromRenderViewHost(rvh));
  AccessibilityMode current_mode = web_contents->GetAccessibilityMode();
  // Flip the bits represented by |mode|. See accessibility_mode_enums.h for
  // values.
  current_mode ^= mode;
  web_contents->SetAccessibilityMode(current_mode);
}

void AccessibilityUI::SetGlobalFlag(const base::ListValue* args) {
  std::string flag_name_str;
  bool enabled;
  CHECK_EQ(2U, args->GetSize());
  CHECK(args->GetString(0, &flag_name_str));
  CHECK(args->GetBoolean(1, &enabled));

  if (flag_name_str == kInternal) {
    g_show_internal_accessibility_tree = enabled;
    LOG(ERROR) << "INTERNAL: " << g_show_internal_accessibility_tree;
    return;
  }

  AccessibilityMode new_mode;
  if (flag_name_str == kNative) {
    new_mode = ACCESSIBILITY_MODE_FLAG_NATIVE_APIS;
  } else if (flag_name_str == kWeb) {
    new_mode = ACCESSIBILITY_MODE_FLAG_WEB_CONTENTS;
  } else if (flag_name_str == kText) {
    new_mode = ACCESSIBILITY_MODE_FLAG_INLINE_TEXT_BOXES;
  } else if (flag_name_str == kScreenReader) {
    new_mode = ACCESSIBILITY_MODE_FLAG_SCREEN_READER;
  } else if (flag_name_str == kHTML) {
    new_mode = ACCESSIBILITY_MODE_FLAG_HTML;
  } else {
    NOTREACHED();
    return;
  }

  // It doesn't make sense to enable one of the flags that depends on
  // web contents without enabling web contents accessibility too.
  if (enabled &&
      (new_mode == ACCESSIBILITY_MODE_FLAG_INLINE_TEXT_BOXES ||
       new_mode == ACCESSIBILITY_MODE_FLAG_SCREEN_READER ||
       new_mode == ACCESSIBILITY_MODE_FLAG_HTML)) {
    new_mode |= ACCESSIBILITY_MODE_FLAG_WEB_CONTENTS;
  }

  // Similarly if you disable web accessibility we should remove all
  // flags that depend on it.
  if (!enabled && new_mode == ACCESSIBILITY_MODE_FLAG_WEB_CONTENTS) {
    new_mode |= ACCESSIBILITY_MODE_FLAG_INLINE_TEXT_BOXES;
    new_mode |= ACCESSIBILITY_MODE_FLAG_SCREEN_READER;
    new_mode |= ACCESSIBILITY_MODE_FLAG_HTML;
  }

  BrowserAccessibilityStateImpl* state =
      BrowserAccessibilityStateImpl::GetInstance();
  if (enabled)
    state->AddAccessibilityModeFlags(new_mode);
  else
    state->RemoveAccessibilityModeFlags(new_mode);
}

void AccessibilityUI::RequestAccessibilityTree(const base::ListValue* args) {
  std::string process_id_str;
  std::string route_id_str;
  int process_id;
  int route_id;
  CHECK_EQ(2U, args->GetSize());
  CHECK(args->GetString(0, &process_id_str));
  CHECK(args->GetString(1, &route_id_str));
  CHECK(base::StringToInt(process_id_str, &process_id));
  CHECK(base::StringToInt(route_id_str, &route_id));

  RenderViewHost* rvh = RenderViewHost::FromID(process_id, route_id);
  if (!rvh) {
    std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());
    result->SetInteger(kProcessIdField, process_id);
    result->SetInteger(kRouteIdField, route_id);
    result->Set("error", new base::StringValue("Renderer no longer exists."));
    web_ui()->CallJavascriptFunctionUnsafe("accessibility.showTree",
                                           *(result.get()));
    return;
  }

  std::unique_ptr<base::DictionaryValue> result(BuildTargetDescriptor(rvh));
  auto* web_contents =
      static_cast<WebContentsImpl*>(WebContents::FromRenderViewHost(rvh));
  // No matter the state of the current web_contents, we want to force the mode
  // because we are about to show the accessibility tree
  web_contents->SetAccessibilityMode(ACCESSIBILITY_MODE_FLAG_NATIVE_APIS |
                                     ACCESSIBILITY_MODE_FLAG_WEB_CONTENTS);
  std::unique_ptr<AccessibilityTreeFormatter> formatter;
  if (g_show_internal_accessibility_tree)
    formatter.reset(new AccessibilityTreeFormatterBlink());
  else
    formatter.reset(AccessibilityTreeFormatter::Create());
  base::string16 accessibility_contents_utf16;
  std::vector<AccessibilityTreeFormatter::Filter> filters;
  filters.push_back(AccessibilityTreeFormatter::Filter(
      base::ASCIIToUTF16("*"),
      AccessibilityTreeFormatter::Filter::ALLOW));
  formatter->SetFilters(filters);
  auto* ax_mgr = web_contents->GetOrCreateRootBrowserAccessibilityManager();
  DCHECK(ax_mgr);
  formatter->FormatAccessibilityTree(ax_mgr->GetRoot(),
                                     &accessibility_contents_utf16);
  result->Set("tree",
              new base::StringValue(
                  base::UTF16ToUTF8(accessibility_contents_utf16)));
  web_ui()->CallJavascriptFunctionUnsafe("accessibility.showTree",
                                         *(result.get()));
}

}  // namespace content
