// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/plugins/chrome_plugin_placeholder.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/prerender_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "chrome/renderer/custom_menu_commands.h"
#include "chrome/renderer/plugins/plugin_uma.h"
#include "content/app/strings/grit/content_strings.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "grit/generated_resources.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"

using base::UserMetricsAction;
using blink::WebDocument;
using blink::WebElement;
using blink::WebFrame;
using blink::WebLocalFrame;
using blink::WebMouseEvent;
using blink::WebNode;
using blink::WebPlugin;
using blink::WebPluginContainer;
using blink::WebPluginParams;
using content::RenderThread;
using content::RenderView;

namespace {
const plugins::PluginPlaceholder* g_last_active_menu = NULL;
}  // namespace

const char ChromePluginPlaceholder::kPluginPlaceholderDataURL[] =
    "chrome://pluginplaceholderdata/";

ChromePluginPlaceholder::ChromePluginPlaceholder(
    content::RenderFrame* render_frame,
    blink::WebLocalFrame* frame,
    const blink::WebPluginParams& params,
    const std::string& html_data,
    const base::string16& title)
    : plugins::PluginPlaceholder(render_frame,
                                 frame,
                                 params,
                                 html_data,
                                 GURL(kPluginPlaceholderDataURL)),
      status_(new ChromeViewHostMsg_GetPluginInfo_Status),
      title_(title),
#if defined(ENABLE_PLUGIN_INSTALLATION)
      placeholder_routing_id_(MSG_ROUTING_NONE),
#endif
      has_host_(false),
      context_menu_request_id_(0) {
  RenderThread::Get()->AddObserver(this);
}

ChromePluginPlaceholder::~ChromePluginPlaceholder() {
  RenderThread::Get()->RemoveObserver(this);
  if (context_menu_request_id_ && render_frame())
    render_frame()->CancelContextMenu(context_menu_request_id_);

#if defined(ENABLE_PLUGIN_INSTALLATION)
  if (placeholder_routing_id_ == MSG_ROUTING_NONE)
    return;
  RenderThread::Get()->RemoveRoute(placeholder_routing_id_);
  if (has_host_) {
    RenderThread::Get()->Send(new ChromeViewHostMsg_RemovePluginPlaceholderHost(
        routing_id(), placeholder_routing_id_));
  }
#endif
}

// static
ChromePluginPlaceholder* ChromePluginPlaceholder::CreateMissingPlugin(
    content::RenderFrame* render_frame,
    WebLocalFrame* frame,
    const WebPluginParams& params) {
  const base::StringPiece template_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_BLOCKED_PLUGIN_HTML));

  base::DictionaryValue values;
#if defined(ENABLE_PLUGIN_INSTALLATION)
  values.SetString("message", l10n_util::GetStringUTF8(IDS_PLUGIN_SEARCHING));
#else
  values.SetString("message",
                   l10n_util::GetStringUTF8(IDS_PLUGIN_NOT_SUPPORTED));
#endif

  std::string html_data = webui::GetI18nTemplateHtml(template_html, &values);

  // |missing_plugin| will destroy itself when its WebViewPlugin is going away.
  ChromePluginPlaceholder* missing_plugin = new ChromePluginPlaceholder(
      render_frame, frame, params, html_data, params.mimeType);
  missing_plugin->set_allow_loading(true);
#if defined(ENABLE_PLUGIN_INSTALLATION)
  RenderThread::Get()->Send(
      new ChromeViewHostMsg_FindMissingPlugin(missing_plugin->routing_id(),
                                              missing_plugin->CreateRoutingId(),
                                              params.mimeType.utf8()));
#endif
  return missing_plugin;
}

// static
ChromePluginPlaceholder* ChromePluginPlaceholder::CreateErrorPlugin(
    content::RenderFrame* render_frame,
    const base::FilePath& file_path) {
  base::DictionaryValue values;
  values.SetString("message",
                   l10n_util::GetStringUTF8(IDS_PLUGIN_INITIALIZATION_ERROR));

  const base::StringPiece template_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_BLOCKED_PLUGIN_HTML));
  std::string html_data = webui::GetI18nTemplateHtml(template_html, &values);

  WebPluginParams params;
  // |missing_plugin| will destroy itself when its WebViewPlugin is going away.
  ChromePluginPlaceholder* plugin = new ChromePluginPlaceholder(
      render_frame, NULL, params, html_data, params.mimeType);

  RenderThread::Get()->Send(new ChromeViewHostMsg_CouldNotLoadPlugin(
      plugin->routing_id(), file_path));
  return plugin;
}

// static
ChromePluginPlaceholder* ChromePluginPlaceholder::CreateBlockedPlugin(
    content::RenderFrame* render_frame,
    WebLocalFrame* frame,
    const WebPluginParams& params,
    const content::WebPluginInfo& plugin,
    const std::string& identifier,
    const base::string16& name,
    int template_id,
    const base::string16& message) {
  base::DictionaryValue values;
  values.SetString("message", message);
  values.SetString("name", name);
  values.SetString("hide", l10n_util::GetStringUTF8(IDS_PLUGIN_HIDE));

  const base::StringPiece template_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(template_id));

  DCHECK(!template_html.empty()) << "unable to load template. ID: "
                                 << template_id;
  std::string html_data = webui::GetI18nTemplateHtml(template_html, &values);

  // |blocked_plugin| will destroy itself when its WebViewPlugin is going away.
  ChromePluginPlaceholder* blocked_plugin = new ChromePluginPlaceholder(
      render_frame, frame, params, html_data, name);
  blocked_plugin->SetPluginInfo(plugin);
  blocked_plugin->SetIdentifier(identifier);
  return blocked_plugin;
}

void ChromePluginPlaceholder::SetStatus(
    const ChromeViewHostMsg_GetPluginInfo_Status& status) {
  status_->value = status.value;
}

#if defined(ENABLE_PLUGIN_INSTALLATION)
int32 ChromePluginPlaceholder::CreateRoutingId() {
  placeholder_routing_id_ = RenderThread::Get()->GenerateRoutingID();
  RenderThread::Get()->AddRoute(placeholder_routing_id_, this);
  return placeholder_routing_id_;
}
#endif

bool ChromePluginPlaceholder::OnMessageReceived(const IPC::Message& message) {
#if defined(ENABLE_PLUGIN_INSTALLATION)
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromePluginPlaceholder, message)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_FoundMissingPlugin, OnFoundMissingPlugin)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_DidNotFindMissingPlugin,
                        OnDidNotFindMissingPlugin)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_StartedDownloadingPlugin,
                        OnStartedDownloadingPlugin)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_FinishedDownloadingPlugin,
                        OnFinishedDownloadingPlugin)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_ErrorDownloadingPlugin,
                        OnErrorDownloadingPlugin)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_CancelledDownloadingPlugin,
                        OnCancelledDownloadingPlugin)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (handled)
    return true;
#endif

  // We don't swallow these messages because multiple blocked plugins and other
  // objects have an interest in them.
  IPC_BEGIN_MESSAGE_MAP(ChromePluginPlaceholder, message)
    IPC_MESSAGE_HANDLER(PrerenderMsg_SetIsPrerendering, OnSetIsPrerendering)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_LoadBlockedPlugins, OnLoadBlockedPlugins)
  IPC_END_MESSAGE_MAP()

  return false;
}

void ChromePluginPlaceholder::OnLoadBlockedPlugins(
    const std::string& identifier) {
  plugins::PluginPlaceholder::OnLoadBlockedPlugins(identifier);
}

void ChromePluginPlaceholder::OpenAboutPluginsCallback() {
  RenderThread::Get()->Send(
      new ChromeViewHostMsg_OpenAboutPlugins(routing_id()));
}

void ChromePluginPlaceholder::OnSetIsPrerendering(bool is_prerendering) {
  plugins::PluginPlaceholder::OnSetIsPrerendering(is_prerendering);
}

#if defined(ENABLE_PLUGIN_INSTALLATION)
void ChromePluginPlaceholder::OnDidNotFindMissingPlugin() {
  SetMessage(l10n_util::GetStringUTF16(IDS_PLUGIN_NOT_FOUND));
}

void ChromePluginPlaceholder::OnFoundMissingPlugin(
    const base::string16& plugin_name) {
  if (status_->value == ChromeViewHostMsg_GetPluginInfo_Status::kNotFound)
    SetMessage(l10n_util::GetStringFUTF16(IDS_PLUGIN_FOUND, plugin_name));
  has_host_ = true;
  plugin_name_ = plugin_name;
}

void ChromePluginPlaceholder::OnStartedDownloadingPlugin() {
  SetMessage(l10n_util::GetStringFUTF16(IDS_PLUGIN_DOWNLOADING, plugin_name_));
}

void ChromePluginPlaceholder::OnFinishedDownloadingPlugin() {
  bool is_installing =
      status_->value == ChromeViewHostMsg_GetPluginInfo_Status::kNotFound;
  SetMessage(l10n_util::GetStringFUTF16(
      is_installing ? IDS_PLUGIN_INSTALLING : IDS_PLUGIN_UPDATING,
      plugin_name_));
}

void ChromePluginPlaceholder::OnErrorDownloadingPlugin(
    const std::string& error) {
  SetMessage(l10n_util::GetStringFUTF16(IDS_PLUGIN_DOWNLOAD_ERROR,
                                        base::UTF8ToUTF16(error)));
}

void ChromePluginPlaceholder::OnCancelledDownloadingPlugin() {
  SetMessage(
      l10n_util::GetStringFUTF16(IDS_PLUGIN_DOWNLOAD_CANCELLED, plugin_name_));
}
#endif  // defined(ENABLE_PLUGIN_INSTALLATION)

void ChromePluginPlaceholder::PluginListChanged() {
  if (!GetFrame() || !plugin())
    return;
  WebDocument document = GetFrame()->top()->document();
  if (document.isNull())
    return;

  ChromeViewHostMsg_GetPluginInfo_Output output;
  std::string mime_type(GetPluginParams().mimeType.utf8());
  render_frame()->Send(
      new ChromeViewHostMsg_GetPluginInfo(routing_id(),
                                          GURL(GetPluginParams().url),
                                          document.url(),
                                          mime_type,
                                          &output));
  if (output.status.value == status_->value)
    return;
  WebPlugin* new_plugin = ChromeContentRendererClient::CreatePlugin(
      render_frame(),  GetFrame(), GetPluginParams(), output);
  ReplacePlugin(new_plugin);
  if (!new_plugin) {
    PluginUMAReporter::GetInstance()->ReportPluginMissing(
        GetPluginParams().mimeType.utf8(), GURL(GetPluginParams().url));
  }
}

void ChromePluginPlaceholder::OnMenuAction(int request_id, unsigned action) {
  DCHECK_EQ(context_menu_request_id_, request_id);
  if (g_last_active_menu != this)
    return;
  switch (action) {
    case chrome::MENU_COMMAND_PLUGIN_RUN: {
      RenderThread::Get()->RecordAction(UserMetricsAction("Plugin_Load_Menu"));
      LoadPlugin();
      break;
    }
    case chrome::MENU_COMMAND_PLUGIN_HIDE: {
      RenderThread::Get()->RecordAction(UserMetricsAction("Plugin_Hide_Menu"));
      HidePlugin();
      break;
    }
    default:
      NOTREACHED();
  }
}

void ChromePluginPlaceholder::OnMenuClosed(int request_id) {
  DCHECK_EQ(context_menu_request_id_, request_id);
  context_menu_request_id_ = 0;
}

void ChromePluginPlaceholder::ShowContextMenu(const WebMouseEvent& event) {
#if !defined(OS_ANDROID)  // The context menu is not applicable on Android.
  if (context_menu_request_id_)
    return;  // Don't allow nested context menu requests.

  content::ContextMenuParams params;

  content::MenuItem name_item;
  name_item.label = title_;
  params.custom_items.push_back(name_item);

  content::MenuItem separator_item;
  separator_item.type = content::MenuItem::SEPARATOR;
  params.custom_items.push_back(separator_item);

  if (!GetPluginInfo().path.value().empty()) {
    content::MenuItem run_item;
    run_item.action = chrome::MENU_COMMAND_PLUGIN_RUN;
    // Disable this menu item if the plugin is blocked by policy.
    run_item.enabled = LoadingAllowed();
    run_item.label = l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_PLUGIN_RUN);
    params.custom_items.push_back(run_item);
  }

  content::MenuItem hide_item;
  hide_item.action = chrome::MENU_COMMAND_PLUGIN_HIDE;
  hide_item.enabled = true;
  hide_item.label = l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_PLUGIN_HIDE);
  params.custom_items.push_back(hide_item);

  params.x = event.windowX;
  params.y = event.windowY;

  context_menu_request_id_ = render_frame()->ShowContextMenu(this, params);
  g_last_active_menu = this;
#endif  // OS_ANDROID
}

void ChromePluginPlaceholder::BindWebFrame(blink::WebFrame* frame) {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Handle<v8::Context> context = frame->mainWorldScriptContext();
  DCHECK(!context.IsEmpty());

  v8::Context::Scope context_scope(context);
  v8::Handle<v8::Object> global = context->Global();
  global->Set(gin::StringToV8(isolate, "plugin"),
              gin::CreateHandle(isolate, this).ToV8());
}

gin::ObjectTemplateBuilder ChromePluginPlaceholder::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return PluginPlaceholder::GetObjectTemplateBuilder(isolate).SetMethod(
      "openAboutPlugins", &ChromePluginPlaceholder::OpenAboutPluginsCallback);
}
