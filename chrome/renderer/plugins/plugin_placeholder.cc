// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/plugins/plugin_placeholder.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/string_escape.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/prerender_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "chrome/renderer/custom_menu_commands.h"
#include "chrome/renderer/plugins/plugin_uma.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "grit/generated_resources.h"
#include "grit/renderer_resources.h"
#include "grit/webkit_strings.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebData.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebPoint.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebContextMenuData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMenuItemInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRegularExpression.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScriptSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextCaseSensitivity.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/webview_plugin.h"

#if defined(ENABLE_MOBILE_YOUTUBE_PLUGIN)
#include "webkit/plugins/plugin_constants.h"
#endif

using content::RenderThread;
using content::RenderView;
using WebKit::WebContextMenuData;
using WebKit::WebDocument;
using WebKit::WebElement;
using WebKit::WebFrame;
using WebKit::WebMenuItemInfo;
using WebKit::WebMouseEvent;
using WebKit::WebNode;
using WebKit::WebPlugin;
using WebKit::WebPluginContainer;
using webkit::WebPluginInfo;
using WebKit::WebPluginParams;
using WebKit::WebPoint;
using WebKit::WebRegularExpression;
using WebKit::WebScriptSource;
using WebKit::WebString;
using WebKit::WebURLRequest;
using WebKit::WebVector;
using webkit::WebViewPlugin;
using webkit_glue::CppArgumentList;
using webkit_glue::CppVariant;

const char* const kPluginPlaceholderDataURL =
    "chrome://pluginplaceholderdata/";

#if defined(ENABLE_MOBILE_YOUTUBE_PLUGIN)
// Strings we used to parse the youtube plugin url.
const char* const kSlashVSlash = "/v/";
const char* const kSlashESlash = "/e/";
#endif

namespace {
const PluginPlaceholder* g_last_active_menu = NULL;

#if defined(ENABLE_MOBILE_YOUTUBE_PLUGIN)
// Helper function to get the youtube id from plugin params for old style
// embedded youtube video.
std::string GetYoutubeVideoId(const WebPluginParams& params) {
  GURL url(params.url);
  std::string video_id = url.path().substr(strlen(kSlashVSlash));

  // Extract just the video id
  size_t video_id_end = video_id.find('&');
  if (video_id_end != std::string::npos)
    video_id = video_id.substr(0, video_id_end);
  return video_id;
}
#endif
}

// static
PluginPlaceholder* PluginPlaceholder::CreateMissingPlugin(
    RenderView* render_view,
    WebFrame* frame,
    const WebPluginParams& params) {
  const base::StringPiece template_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_BLOCKED_PLUGIN_HTML));

  DictionaryValue values;
#if defined(ENABLE_PLUGIN_INSTALLATION)
  values.SetString("message", l10n_util::GetStringUTF8(IDS_PLUGIN_SEARCHING));
#else
  values.SetString("message",
      l10n_util::GetStringUTF8(IDS_PLUGIN_NOT_SUPPORTED));
#endif

  std::string html_data =
      jstemplate_builder::GetI18nTemplateHtml(template_html, &values);

  // |missing_plugin| will destroy itself when its WebViewPlugin is going away.
  PluginPlaceholder* missing_plugin = new PluginPlaceholder(
      render_view, frame, params, html_data, params.mimeType);
  missing_plugin->set_allow_loading(true);
#if defined(ENABLE_PLUGIN_INSTALLATION)
  RenderThread::Get()->Send(new ChromeViewHostMsg_FindMissingPlugin(
      missing_plugin->routing_id(), missing_plugin->CreateRoutingId(),
      params.mimeType.utf8()));
#endif
  return missing_plugin;
}

PluginPlaceholder* PluginPlaceholder::CreateErrorPlugin(
    RenderView* render_view,
    const FilePath& file_path) {
  DictionaryValue values;
  values.SetString("message",
                   l10n_util::GetStringUTF8(IDS_PLUGIN_INITIALIZATION_ERROR));

  const base::StringPiece template_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_BLOCKED_PLUGIN_HTML));
  std::string html_data =
      jstemplate_builder::GetI18nTemplateHtml(template_html, &values);

  WebPluginParams params;
  // |missing_plugin| will destroy itself when its WebViewPlugin is going away.
  PluginPlaceholder* plugin = new PluginPlaceholder(
      render_view, NULL, params, html_data, params.mimeType);

  RenderThread::Get()->Send(
      new ChromeViewHostMsg_CouldNotLoadPlugin(plugin->routing_id(),
                                               file_path));
  return plugin;
}

// static
PluginPlaceholder* PluginPlaceholder::CreateBlockedPlugin(
    RenderView* render_view,
    WebFrame* frame,
    const WebPluginParams& params,
    const WebPluginInfo& plugin,
    const std::string& identifier,
    const string16& name,
    int template_id,
    const string16& message) {
  DictionaryValue values;
  values.SetString("message", message);
  values.SetString("name", name);
  values.SetString("hide", l10n_util::GetStringUTF8(IDS_PLUGIN_HIDE));

  const base::StringPiece template_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          template_id));

  DCHECK(!template_html.empty()) << "unable to load template. ID: "
                                 << template_id;
  std::string html_data = jstemplate_builder::GetI18nTemplateHtml(
      template_html, &values);

  // |blocked_plugin| will destroy itself when its WebViewPlugin is going away.
  PluginPlaceholder* blocked_plugin = new PluginPlaceholder(
      render_view, frame, params, html_data, name);
  blocked_plugin->plugin_info_ = plugin;
  blocked_plugin->identifier_ = identifier;
  return blocked_plugin;
}

#if defined(ENABLE_MOBILE_YOUTUBE_PLUGIN)
// static
PluginPlaceholder* PluginPlaceholder::CreateMobileYoutubePlugin(
    content::RenderView* render_view,
    WebFrame* frame,
    const WebPluginParams& params) {
  const base::StringPiece template_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_MOBILE_YOUTUBE_PLUGIN_HTML));

  DictionaryValue values;
  values.SetString("video_id", GetYoutubeVideoId(params));
  std::string html_data = jstemplate_builder::GetI18nTemplateHtml(
      template_html, &values);

  // |youtube_plugin| will destroy itself when its WebViewPlugin is going away.
  PluginPlaceholder* youtube_plugin = new PluginPlaceholder(
      render_view, frame, params, html_data, params.mimeType);
  return youtube_plugin;
}
#endif

PluginPlaceholder::PluginPlaceholder(content::RenderView* render_view,
                                     WebFrame* frame,
                                     const WebPluginParams& params,
                                     const std::string& html_data,
                                     const string16& title)
    : content::RenderViewObserver(render_view),
      frame_(frame),
      plugin_params_(params),
      plugin_(WebViewPlugin::Create(
          this, render_view->GetWebkitPreferences(), html_data,
          GURL(kPluginPlaceholderDataURL))),
      title_(title),
      status_(new ChromeViewHostMsg_GetPluginInfo_Status),
      is_blocked_for_prerendering_(false),
      allow_loading_(false),
#if defined(ENABLE_PLUGIN_INSTALLATION)
      placeholder_routing_id_(MSG_ROUTING_NONE),
#endif
      hidden_(false),
      has_host_(false),
      finished_loading_(false),
      context_menu_request_id_(0) {
  RenderThread::Get()->AddObserver(this);
}

PluginPlaceholder::~PluginPlaceholder() {
  RenderThread::Get()->RemoveObserver(this);
  if (context_menu_request_id_)
    render_view()->CancelContextMenu(context_menu_request_id_);

#if defined(ENABLE_PLUGIN_INSTALLATION)
  if (placeholder_routing_id_ == MSG_ROUTING_NONE)
    return;
  RenderThread::Get()->RemoveRoute(placeholder_routing_id_);
  if (has_host_) {
    RenderThread::Get()->Send(
        new ChromeViewHostMsg_RemovePluginPlaceholderHost(
            routing_id(), placeholder_routing_id_));
  }
#endif
}

#if defined(ENABLE_PLUGIN_INSTALLATION)
int32 PluginPlaceholder::CreateRoutingId() {
  placeholder_routing_id_ = RenderThread::Get()->GenerateRoutingID();
  RenderThread::Get()->AddRoute(placeholder_routing_id_, this);
  return placeholder_routing_id_;
}
#endif

void PluginPlaceholder::SetStatus(
    const ChromeViewHostMsg_GetPluginInfo_Status& status) {
  status_->value = status.value;
}

void PluginPlaceholder::BindWebFrame(WebFrame* frame) {
  BindToJavascript(frame, "plugin");
  BindCallback("load", base::Bind(&PluginPlaceholder::LoadCallback,
                                  base::Unretained(this)));
  BindCallback("hide", base::Bind(&PluginPlaceholder::HideCallback,
                                  base::Unretained(this)));
  BindCallback("openAboutPlugins",
               base::Bind(&PluginPlaceholder::OpenAboutPluginsCallback,
                          base::Unretained(this)));
  BindCallback("didFinishLoading",
               base::Bind(&PluginPlaceholder::DidFinishLoadingCallback,
               base::Unretained(this)));
#if defined(ENABLE_MOBILE_YOUTUBE_PLUGIN)
  BindCallback("openYoutubeURL",
               base::Bind(&PluginPlaceholder::OpenYoutubeUrlCallback,
               base::Unretained(this)));
#endif
}

bool PluginPlaceholder::OnMessageReceived(const IPC::Message& message) {
#if defined(ENABLE_PLUGIN_INSTALLATION)
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PluginPlaceholder, message)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_FoundMissingPlugin,
                        OnFoundMissingPlugin)
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

  // We don't swallow these messages because multiple blocked plugins have an
  // interest in them.
  IPC_BEGIN_MESSAGE_MAP(PluginPlaceholder, message)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_LoadBlockedPlugins, OnLoadBlockedPlugins)
    IPC_MESSAGE_HANDLER(PrerenderMsg_SetIsPrerendering, OnSetIsPrerendering)
  IPC_END_MESSAGE_MAP()

  return false;
}

void PluginPlaceholder::ReplacePlugin(WebPlugin* new_plugin) {
  CHECK(plugin_);
  if (!new_plugin) {
    MissingPluginReporter::GetInstance()->ReportPluginMissing(
        plugin_params_.mimeType.utf8(),
        plugin_params_.url);
    return;
  }

  WebPluginContainer* container = plugin_->container();
  // Set the new plug-in on the container before initializing it.
  container->setPlugin(new_plugin);
  // Save the element in case the plug-in is removed from the page during
  // initialization.
  WebElement element = container->element();
  if (!new_plugin->initialize(container)) {
    // We couldn't initialize the new plug-in. Restore the old one and abort.
    container->setPlugin(plugin_);
    return;
  }

  // The plug-in has been removed from the page. Destroy the old plug-in
  // (which will destroy us).
  if (element.parentNode().isNull()) {
    plugin_->destroy();
    return;
  }

  // During initialization, the new plug-in might have replaced itself in turn
  // with another plug-in. Make sure not to use the passed in |new_plugin| after
  // this point.
  new_plugin = container->plugin();

  plugin_->RestoreTitleText();
  container->invalidate();
  container->reportGeometry();
  plugin_->ReplayReceivedData(new_plugin);
  plugin_->destroy();
}

void PluginPlaceholder::HidePlugin() {
  hidden_ = true;
  WebPluginContainer* container = plugin_->container();
  WebElement element = container->element();
  element.setAttribute("style", "display: none;");
  // If we have a width and height, search for a parent (often <div>) with the
  // same dimensions. If we find such a parent, hide that as well.
  // This makes much more uncovered page content usable (including clickable)
  // as opposed to merely visible.
  // TODO(cevans) -- it's a foul heurisitc but we're going to tolerate it for
  // now for these reasons:
  // 1) Makes the user experience better.
  // 2) Foulness is encapsulated within this single function.
  // 3) Confidence in no fasle positives.
  // 4) Seems to have a good / low false negative rate at this time.
  if (element.hasAttribute("width") && element.hasAttribute("height")) {
    std::string width_str("width:[\\s]*");
    width_str += element.getAttribute("width").utf8().data();
    if (EndsWith(width_str, "px", false)) {
      width_str = width_str.substr(0, width_str.length() - 2);
    }
    TrimWhitespace(width_str, TRIM_TRAILING, &width_str);
    width_str += "[\\s]*px";
    WebRegularExpression width_regex(WebString::fromUTF8(width_str.c_str()),
                                     WebKit::WebTextCaseSensitive);
    std::string height_str("height:[\\s]*");
    height_str += element.getAttribute("height").utf8().data();
    if (EndsWith(height_str, "px", false)) {
      height_str = height_str.substr(0, height_str.length() - 2);
    }
    TrimWhitespace(height_str, TRIM_TRAILING, &height_str);
    height_str += "[\\s]*px";
    WebRegularExpression height_regex(WebString::fromUTF8(height_str.c_str()),
                                      WebKit::WebTextCaseSensitive);
    WebNode parent = element;
    while (!parent.parentNode().isNull()) {
      parent = parent.parentNode();
      if (!parent.isElementNode())
        continue;
      element = parent.toConst<WebElement>();
      if (element.hasAttribute("style")) {
        WebString style_str = element.getAttribute("style");
        if (width_regex.match(style_str) >= 0 &&
            height_regex.match(style_str) >= 0)
          element.setAttribute("style", "display: none;");
      }
    }
  }
}

void PluginPlaceholder::WillDestroyPlugin() {
  delete this;
}

#if defined(ENABLE_PLUGIN_INSTALLATION)
void PluginPlaceholder::OnDidNotFindMissingPlugin() {
  SetMessage(l10n_util::GetStringUTF16(IDS_PLUGIN_NOT_FOUND));
}

void PluginPlaceholder::OnFoundMissingPlugin(const string16& plugin_name) {
  if (status_->value == ChromeViewHostMsg_GetPluginInfo_Status::kNotFound)
    SetMessage(l10n_util::GetStringFUTF16(IDS_PLUGIN_FOUND, plugin_name));
  has_host_ = true;
  plugin_name_ = plugin_name;
}

void PluginPlaceholder::OnStartedDownloadingPlugin() {
  SetMessage(l10n_util::GetStringFUTF16(IDS_PLUGIN_DOWNLOADING, plugin_name_));
}

void PluginPlaceholder::OnFinishedDownloadingPlugin() {
  bool is_installing =
      status_->value == ChromeViewHostMsg_GetPluginInfo_Status::kNotFound;
  SetMessage(l10n_util::GetStringFUTF16(
      is_installing ? IDS_PLUGIN_INSTALLING : IDS_PLUGIN_UPDATING,
      plugin_name_));
}

void PluginPlaceholder::OnErrorDownloadingPlugin(const std::string& error) {
  SetMessage(l10n_util::GetStringFUTF16(IDS_PLUGIN_DOWNLOAD_ERROR,
                                        UTF8ToUTF16(error)));
}

void PluginPlaceholder::OnCancelledDownloadingPlugin() {
  SetMessage(l10n_util::GetStringFUTF16(IDS_PLUGIN_DOWNLOAD_CANCELLED,
                                        plugin_name_));
}
#endif  // defined(ENABLE_PLUGIN_INSTALLATION)

void PluginPlaceholder::PluginListChanged() {
  if (!frame_)
    return;
  WebDocument document = frame_->top()->document();
  if (document.isNull())
    return;

  ChromeViewHostMsg_GetPluginInfo_Output output;
  std::string mime_type(plugin_params_.mimeType.utf8());
  render_view()->Send(new ChromeViewHostMsg_GetPluginInfo(
      routing_id(), GURL(plugin_params_.url), document.url(),
      mime_type, &output));
  if (output.status.value == status_->value)
    return;
  chrome::ChromeContentRendererClient* client =
      static_cast<chrome::ChromeContentRendererClient*>(
          content::GetContentClient()->renderer());
  WebPlugin* new_plugin =
      client->CreatePlugin(render_view(), frame_, plugin_params_, output);
  ReplacePlugin(new_plugin);
}

void PluginPlaceholder::OnMenuAction(int request_id, unsigned action) {
  DCHECK_EQ(context_menu_request_id_, request_id);
  if (g_last_active_menu != this)
    return;
  switch (action) {
    case chrome::MENU_COMMAND_PLUGIN_RUN: {
      RenderThread::Get()->RecordUserMetrics("Plugin_Load_Menu");
      LoadPlugin();
      break;
    }
    case chrome::MENU_COMMAND_PLUGIN_HIDE: {
      RenderThread::Get()->RecordUserMetrics("Plugin_Hide_Menu");
      HidePlugin();
      break;
    }
    default:
      NOTREACHED();
  }
}

void PluginPlaceholder::OnMenuClosed(int request_id) {
  DCHECK_EQ(context_menu_request_id_, request_id);
  context_menu_request_id_ = 0;
}

void PluginPlaceholder::SetMessage(const string16& message) {
  message_ = message;
  if (finished_loading_)
    UpdateMessage();
}

void PluginPlaceholder::UpdateMessage() {
  std::string script = "window.setMessage(" +
                       base::GetDoubleQuotedJson(message_) + ")";
  plugin_->web_view()->mainFrame()->executeScript(
      WebScriptSource(ASCIIToUTF16(script)));
}

void PluginPlaceholder::ShowContextMenu(const WebMouseEvent& event) {
  if (context_menu_request_id_)
    return;  // Don't allow nested context menu requests.

  content::ContextMenuParams params;

  WebMenuItemInfo name_item;
  name_item.label = title_;
  name_item.hasTextDirectionOverride = false;
  name_item.textDirection =  WebKit::WebTextDirectionDefault;
  params.custom_items.push_back(WebMenuItem(name_item));

  WebMenuItemInfo separator_item;
  separator_item.type = WebMenuItemInfo::Separator;
  params.custom_items.push_back(WebMenuItem(separator_item));

  if (!plugin_info_.path.value().empty()) {
    WebMenuItemInfo run_item;
    run_item.action = chrome::MENU_COMMAND_PLUGIN_RUN;
    // Disable this menu item if the plugin is blocked by policy.
    run_item.enabled = allow_loading_;
    run_item.label = WebString::fromUTF8(
        l10n_util::GetStringUTF8(IDS_CONTENT_CONTEXT_PLUGIN_RUN).c_str());
    run_item.hasTextDirectionOverride = false;
    run_item.textDirection =  WebKit::WebTextDirectionDefault;
    params.custom_items.push_back(WebMenuItem(run_item));
  }

  WebMenuItemInfo hide_item;
  hide_item.action = chrome::MENU_COMMAND_PLUGIN_HIDE;
  hide_item.enabled = true;
  hide_item.label = WebString::fromUTF8(
      l10n_util::GetStringUTF8(IDS_CONTENT_CONTEXT_PLUGIN_HIDE).c_str());
  hide_item.hasTextDirectionOverride = false;
  hide_item.textDirection =  WebKit::WebTextDirectionDefault;
  params.custom_items.push_back(WebMenuItem(hide_item));

  params.x = event.windowX;
  params.y = event.windowY;

  context_menu_request_id_ = render_view()->ShowContextMenu(this, params);
  g_last_active_menu = this;
}

void PluginPlaceholder::OnLoadBlockedPlugins(const std::string& identifier) {
  if (!identifier.empty() && identifier != identifier_)
    return;

  RenderThread::Get()->RecordUserMetrics("Plugin_Load_UI");
  LoadPlugin();
}

void PluginPlaceholder::OnSetIsPrerendering(bool is_prerendering) {
  // Prerendering can only be enabled prior to a RenderView's first navigation,
  // so no BlockedPlugin should see the notification that enables prerendering.
  DCHECK(!is_prerendering);
  if (is_blocked_for_prerendering_ && !is_prerendering)
    LoadPlugin();
}

void PluginPlaceholder::LoadPlugin() {
  // This is not strictly necessary but is an important defense in case the
  // event propagation changes between "close" vs. "click-to-play".
  if (hidden_)
    return;
  if (!allow_loading_) {
    NOTREACHED();
    return;
  }

  // TODO(mmenke):  In the case of prerendering, feed into
  //                ChromeContentRendererClient::CreatePlugin instead, to
  //                reduce the chance of future regressions.
  WebPlugin* plugin =
      render_view()->CreatePlugin(frame_, plugin_info_, plugin_params_);
  ReplacePlugin(plugin);
}

void PluginPlaceholder::LoadCallback(const CppArgumentList& args,
                                     CppVariant* result) {
  RenderThread::Get()->RecordUserMetrics("Plugin_Load_Click");
  LoadPlugin();
}

void PluginPlaceholder::HideCallback(const CppArgumentList& args,
                                     CppVariant* result) {
  RenderThread::Get()->RecordUserMetrics("Plugin_Hide_Click");
  HidePlugin();
}

void PluginPlaceholder::OpenAboutPluginsCallback(const CppArgumentList& args,
                                                 CppVariant* result) {
  RenderThread::Get()->Send(
      new ChromeViewHostMsg_OpenAboutPlugins(routing_id()));
}

void PluginPlaceholder::DidFinishLoadingCallback(const CppArgumentList& args,
                                                 CppVariant* result) {
  finished_loading_ = true;
  if (message_.length() > 0)
    UpdateMessage();
}

#if defined(ENABLE_MOBILE_YOUTUBE_PLUGIN)
void PluginPlaceholder::OpenYoutubeUrlCallback(const CppArgumentList& args,
                                               CppVariant* result) {
  std::string youtube("vnd.youtube:");
  GURL url(youtube.append(GetYoutubeVideoId(plugin_params_)));
  WebURLRequest request;
  request.initialize();
  request.setURL(url);
  render_view()->LoadURLExternally(
      frame_, request, WebKit::WebNavigationPolicyNewForegroundTab);
}

bool PluginPlaceholder::IsValidYouTubeVideo(const std::string& path) {
  unsigned len = strlen(kSlashVSlash);

  // check for more than just /v/ or /e/.
  if (path.length() <= len)
    return false;

  std::string str = StringToLowerASCII(path);
  // Youtube flash url can start with /v/ or /e/.
  if (strncmp(str.data(), kSlashVSlash, len) != 0 &&
      strncmp(str.data(), kSlashESlash, len) != 0)
    return false;

  // Start after /v/
  for (unsigned i = len; i < path.length(); i++) {
    char c = str[i];
    if (isalpha(c) || isdigit(c) || c == '_' || c == '-')
      continue;
    // The url can have more parameters such as &hl=en after the video id.
    // Once we start seeing extra parameters we can return true.
    return c == '&' && i > len;
  }
  return true;
}

bool PluginPlaceholder::IsYouTubeURL(const GURL& url,
                                       const std::string& mime_type) {
  std::string host = url.host();
  bool is_youtube = EndsWith(host, "youtube.com", true) ||
      EndsWith(host, "youtube-nocookie.com", true);

  return is_youtube && IsValidYouTubeVideo(url.path()) &&
      LowerCaseEqualsASCII(mime_type, kFlashPluginSwfMimeType);
}
#endif
