// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_content_renderer_client.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/string_tokenizer.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_settings_pattern.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/extension_process_policy.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/extensions/manifest_handler.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "chrome/common/extensions/web_accessible_resources_handler.h"
#include "chrome/common/external_ipc_fuzzer.h"
#include "chrome/common/localized_error.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/autofill/autofill_agent.h"
#include "chrome/renderer/autofill/password_autofill_manager.h"
#include "chrome/renderer/autofill/password_generation_manager.h"
#include "chrome/renderer/automation/automation_renderer_helper.h"
#include "chrome/renderer/benchmarking_extension.h"
#include "chrome/renderer/chrome_render_process_observer.h"
#include "chrome/renderer/chrome_render_view_observer.h"
#include "chrome/renderer/content_settings_observer.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "chrome/renderer/extensions/chrome_v8_extension.h"
#include "chrome/renderer/extensions/dispatcher.h"
#include "chrome/renderer/extensions/extension_helper.h"
#include "chrome/renderer/extensions/miscellaneous_bindings.h"
#include "chrome/renderer/extensions/resource_request_policy.h"
#include "chrome/renderer/external_extension.h"
#include "chrome/renderer/loadtimes_extension_bindings.h"
#include "chrome/renderer/net/renderer_net_predictor.h"
#include "chrome/renderer/page_click_tracker.h"
#include "chrome/renderer/page_load_histograms.h"
#include "chrome/renderer/pepper/chrome_ppapi_interfaces.h"
#include "chrome/renderer/pepper/pepper_helper.h"
#include "chrome/renderer/playback_extension.h"
#include "chrome/renderer/plugins/plugin_placeholder.h"
#include "chrome/renderer/plugins/plugin_uma.h"
#include "chrome/renderer/prerender/prerender_dispatcher.h"
#include "chrome/renderer/prerender/prerender_helper.h"
#include "chrome/renderer/prerender/prerender_webmediaplayer.h"
#include "chrome/renderer/prerender/prerenderer_client.h"
#include "chrome/renderer/printing/print_web_view_helper.h"
#include "chrome/renderer/safe_browsing/malware_dom_details.h"
#include "chrome/renderer/safe_browsing/phishing_classifier_delegate.h"
#include "chrome/renderer/searchbox/searchbox.h"
#include "chrome/renderer/searchbox/searchbox_extension.h"
#include "chrome/renderer/spellchecker/spellcheck.h"
#include "chrome/renderer/spellchecker/spellcheck_provider.h"
#include "components/visitedlink/renderer/visitedlink_slave.h"
#include "content/public/common/content_constants.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_view_visitor.h"
#include "extensions/common/constants.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/renderer_resources.h"
#include "ipc/ipc_sync_channel.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLError.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCache.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDataSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginParams.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityPolicy.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/webui/jstemplate_builder.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_interface_factory.h"

using autofill::AutofillAgent;
using autofill::PasswordAutofillManager;
using autofill::PasswordGenerationManager;
using content::RenderThread;
using extensions::Extension;
using WebKit::WebCache;
using WebKit::WebConsoleMessage;
using WebKit::WebDataSource;
using WebKit::WebDocument;
using WebKit::WebFrame;
using WebKit::WebPlugin;
using webkit::WebPluginInfo;
using webkit::WebPluginMimeType;
using WebKit::WebPluginParams;
using WebKit::WebSecurityOrigin;
using WebKit::WebSecurityPolicy;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebURLError;
using WebKit::WebURLRequest;
using WebKit::WebURLResponse;
using WebKit::WebVector;

namespace {

// Explicitly register all extension ManifestHandlers needed to parse
// fields used in the renderer.
void RegisterExtensionManifestHandlers() {
  extensions::ManifestHandler::Register(
      extension_manifest_keys::kDevToolsPage,
      new extensions::DevToolsPageHandler);
  extensions::ManifestHandler::Register(
      extension_manifest_keys::kWebAccessibleResources,
      new extensions::WebAccessibleResourcesHandler);
}

static void AppendParams(const std::vector<string16>& additional_names,
                         const std::vector<string16>& additional_values,
                         WebVector<WebString>* existing_names,
                         WebVector<WebString>* existing_values) {
  DCHECK(additional_names.size() == additional_values.size());
  DCHECK(existing_names->size() == existing_values->size());

  size_t existing_size = existing_names->size();
  size_t total_size = existing_size + additional_names.size();

  WebVector<WebString> names(total_size);
  WebVector<WebString> values(total_size);

  for (size_t i = 0; i < existing_size; ++i) {
    names[i] = (*existing_names)[i];
    values[i] = (*existing_values)[i];
  }

  for (size_t i = 0; i < additional_names.size(); ++i) {
    names[existing_size + i] = additional_names[i];
    values[existing_size + i] = additional_values[i];
  }

  existing_names->swap(names);
  existing_values->swap(values);
}

class SpellCheckReplacer : public content::RenderViewVisitor {
 public:
  explicit SpellCheckReplacer(SpellCheck* spellcheck)
      : spellcheck_(spellcheck) {}
  virtual bool Visit(content::RenderView* render_view) OVERRIDE;

 private:
  SpellCheck* spellcheck_;  // New shared spellcheck for all views. Weak Ptr.
  DISALLOW_COPY_AND_ASSIGN(SpellCheckReplacer);
};

bool SpellCheckReplacer::Visit(content::RenderView* render_view) {
  SpellCheckProvider* provider = SpellCheckProvider::Get(render_view);
  DCHECK(provider);
  provider->set_spellcheck(spellcheck_);
  return true;
}

}  // namespace

namespace chrome {

ChromeContentRendererClient::ChromeContentRendererClient() {
}

ChromeContentRendererClient::~ChromeContentRendererClient() {
}

void ChromeContentRendererClient::RenderThreadStarted() {
  chrome_observer_.reset(new ChromeRenderProcessObserver(this));
  extension_dispatcher_.reset(new extensions::Dispatcher());
  net_predictor_.reset(new RendererNetPredictor());
  spellcheck_.reset(new SpellCheck());
  visited_link_slave_.reset(new components::VisitedLinkSlave());
#if defined(FULL_SAFE_BROWSING)
  phishing_classifier_.reset(safe_browsing::PhishingClassifierFilter::Create());
#endif
  prerender_dispatcher_.reset(new prerender::PrerenderDispatcher());

  RenderThread* thread = RenderThread::Get();

  thread->AddObserver(chrome_observer_.get());
  thread->AddObserver(extension_dispatcher_.get());
#if defined(FULL_SAFE_BROWSING)
  thread->AddObserver(phishing_classifier_.get());
#endif
  thread->AddObserver(spellcheck_.get());
  thread->AddObserver(visited_link_slave_.get());
  thread->AddObserver(prerender_dispatcher_.get());

  thread->RegisterExtension(extensions_v8::ExternalExtension::Get());
  thread->RegisterExtension(extensions_v8::LoadTimesExtension::Get());
  thread->RegisterExtension(extensions_v8::SearchBoxExtension::Get());

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableBenchmarking))
    thread->RegisterExtension(extensions_v8::BenchmarkingExtension::Get());

  if (command_line->HasSwitch(switches::kPlaybackMode) ||
      command_line->HasSwitch(switches::kRecordMode) ||
      command_line->HasSwitch(switches::kNoJsRandomness)) {
    thread->RegisterExtension(extensions_v8::PlaybackExtension::Get());
  }

  if (command_line->HasSwitch(switches::kEnableIPCFuzzing)) {
    thread->GetChannel()->set_outgoing_message_filter(LoadExternalIPCFuzzer());
  }
  // chrome:, chrome-devtools:, and chrome-internal: pages should not be
  // accessible by normal content, and should also be unable to script
  // anything but themselves (to help limit the damage that a corrupt
  // page could cause).
  WebString chrome_ui_scheme(ASCIIToUTF16(chrome::kChromeUIScheme));
  WebSecurityPolicy::registerURLSchemeAsDisplayIsolated(chrome_ui_scheme);

  WebString dev_tools_scheme(ASCIIToUTF16(chrome::kChromeDevToolsScheme));
  WebSecurityPolicy::registerURLSchemeAsDisplayIsolated(dev_tools_scheme);

  WebString internal_scheme(ASCIIToUTF16(chrome::kChromeInternalScheme));
  WebSecurityPolicy::registerURLSchemeAsDisplayIsolated(internal_scheme);

#if defined(OS_CHROMEOS)
  WebString drive_scheme(ASCIIToUTF16(chrome::kDriveScheme));
  WebSecurityPolicy::registerURLSchemeAsLocal(drive_scheme);
#endif

  // chrome: pages should not be accessible by bookmarklets or javascript:
  // URLs typed in the omnibox.
  WebSecurityPolicy::registerURLSchemeAsNotAllowingJavascriptURLs(
      chrome_ui_scheme);

  // chrome:, and chrome-extension: resources shouldn't trigger insecure
  // content warnings.
  WebSecurityPolicy::registerURLSchemeAsSecure(chrome_ui_scheme);

  WebString extension_scheme(ASCIIToUTF16(extensions::kExtensionScheme));
  WebSecurityPolicy::registerURLSchemeAsSecure(extension_scheme);

  // chrome-extension: resources should be allowed to receive CORS requests.
  WebSecurityPolicy::registerURLSchemeAsCORSEnabled(extension_scheme);

  WebString extension_resource_scheme(
      ASCIIToUTF16(chrome::kExtensionResourceScheme));
  WebSecurityPolicy::registerURLSchemeAsSecure(extension_resource_scheme);

  // chrome-extension-resource: resources should be allowed to receive CORS
  // requests.
  WebSecurityPolicy::registerURLSchemeAsCORSEnabled(extension_resource_scheme);

  // chrome-extension: resources should bypass Content Security Policy checks
  // when included in protected resources.
  WebSecurityPolicy::registerURLSchemeAsBypassingContentSecurityPolicy(
      extension_scheme);
  WebSecurityPolicy::registerURLSchemeAsBypassingContentSecurityPolicy(
      extension_resource_scheme);

  RegisterExtensionManifestHandlers();
}

void ChromeContentRendererClient::RenderViewCreated(
    content::RenderView* render_view) {
  ContentSettingsObserver* content_settings =
      new ContentSettingsObserver(render_view);
  if (chrome_observer_.get()) {
    content_settings->SetContentSettingRules(
        chrome_observer_->content_setting_rules());
  }
  new extensions::ExtensionHelper(render_view, extension_dispatcher_.get());
  new PageLoadHistograms(render_view);
#if defined(ENABLE_PRINTING)
  new printing::PrintWebViewHelper(render_view);
#endif
  new SearchBox(render_view);
  new SpellCheckProvider(render_view, spellcheck_.get());
  new prerender::PrerendererClient(render_view);
#if defined(FULL_SAFE_BROWSING)
  safe_browsing::MalwareDOMDetails::Create(render_view);
#endif

  PasswordAutofillManager* password_autofill_manager =
      new PasswordAutofillManager(render_view);
  AutofillAgent* autofill_agent = new AutofillAgent(render_view,
                                                    password_autofill_manager);
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnablePasswordGeneration)) {
    new PasswordGenerationManager(render_view);
  }
  PageClickTracker* page_click_tracker = new PageClickTracker(render_view);
  // Note that the order of insertion of the listeners is important.
  // The password_autocomplete_manager takes the first shot at processing the
  // notification and can stop the propagation.
  page_click_tracker->AddListener(password_autofill_manager);
  page_click_tracker->AddListener(autofill_agent);

  new ChromeRenderViewObserver(
      render_view, content_settings, chrome_observer_.get(),
      extension_dispatcher_.get());

#if defined(ENABLE_PLUGINS)
  new PepperHelper(render_view);
#endif

  // Used only for testing/automation.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDomAutomationController)) {
    new AutomationRendererHelper(render_view);
  }
}

void ChromeContentRendererClient::SetNumberOfViews(int number_of_views) {
  child_process_logging::SetNumberOfViews(number_of_views);
}

SkBitmap* ChromeContentRendererClient::GetSadPluginBitmap() {
  return const_cast<SkBitmap*>(ResourceBundle::GetSharedInstance().
      GetImageNamed(IDR_SAD_PLUGIN).ToSkBitmap());
}

SkBitmap* ChromeContentRendererClient::GetSadWebViewBitmap() {
  return const_cast<SkBitmap*>(ResourceBundle::GetSharedInstance().
      GetImageNamed(IDR_SAD_WEBVIEW).ToSkBitmap());
}

std::string ChromeContentRendererClient::GetDefaultEncoding() {
  return l10n_util::GetStringUTF8(IDS_DEFAULT_ENCODING);
}

const extensions::Extension* ChromeContentRendererClient::GetExtension(
    const WebSecurityOrigin& origin) const {
  if (!EqualsASCII(origin.protocol(), extensions::kExtensionScheme))
    return NULL;

  const std::string extension_id = origin.host().utf8().data();
  if (!extension_dispatcher_->IsExtensionActive(extension_id))
    return NULL;

  return extension_dispatcher_->extensions()->GetByID(extension_id);
}

bool ChromeContentRendererClient::OverrideCreatePlugin(
    content::RenderView* render_view,
    WebFrame* frame,
    const WebPluginParams& params,
    WebPlugin** plugin) {
  std::string orig_mime_type = params.mimeType.utf8();
  if (orig_mime_type == content::kBrowserPluginMimeType) {
    if (CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableBrowserPluginForAllViewTypes))
      return false;
    WebDocument document = frame->document();
    const extensions::Extension* extension =
        GetExtension(document.securityOrigin());
    if (extension && extension->HasAPIPermission(
        extensions::APIPermission::kWebView))
      return false;
  }

  ChromeViewHostMsg_GetPluginInfo_Output output;
#if defined(ENABLE_PLUGINS)
  render_view->Send(new ChromeViewHostMsg_GetPluginInfo(
      render_view->GetRoutingID(), GURL(params.url),
      frame->top()->document().url(), orig_mime_type, &output));
#else
  output.status.value = ChromeViewHostMsg_GetPluginInfo_Status::kNotFound;
#endif
  *plugin = CreatePlugin(render_view, frame, params, output);
  return true;
}

WebPlugin* ChromeContentRendererClient::CreatePluginReplacement(
    content::RenderView* render_view,
    const FilePath& plugin_path) {
  PluginPlaceholder* placeholder =
      PluginPlaceholder::CreateErrorPlugin(render_view, plugin_path);
  return placeholder->plugin();
}

webkit_media::WebMediaPlayerImpl*
ChromeContentRendererClient::OverrideCreateWebMediaPlayer(
    content::RenderView* render_view,
    WebKit::WebFrame* frame,
    WebKit::WebMediaPlayerClient* client,
    base::WeakPtr<webkit_media::WebMediaPlayerDelegate> delegate,
    const webkit_media::WebMediaPlayerParams& params) {
#if defined(OS_ANDROID)
  // Chromium for Android doesn't support prerender yet.
  return NULL;
#else
  if (!prerender::PrerenderHelper::IsPrerendering(render_view))
    return NULL;

  return new prerender::PrerenderWebMediaPlayer(
      render_view, frame, client, delegate, params);
#endif
}

WebPlugin* ChromeContentRendererClient::CreatePlugin(
    content::RenderView* render_view,
    WebFrame* frame,
    const WebPluginParams& original_params,
    const ChromeViewHostMsg_GetPluginInfo_Output& output) {
  const ChromeViewHostMsg_GetPluginInfo_Status& status = output.status;
  const webkit::WebPluginInfo& plugin = output.plugin;
  const std::string& actual_mime_type = output.actual_mime_type;
  const string16& group_name = output.group_name;
  const std::string& identifier = output.group_identifier;
  ChromeViewHostMsg_GetPluginInfo_Status::Value status_value = status.value;
  GURL url(original_params.url);
  std::string orig_mime_type = original_params.mimeType.utf8();
  PluginPlaceholder* placeholder = NULL;

  // If the browser plugin is to be enabled, this should be handled by the
  // renderer, so the code won't reach here due to the early exit in
  // OverrideCreatePlugin.
  if (status_value == ChromeViewHostMsg_GetPluginInfo_Status::kNotFound ||
      orig_mime_type == content::kBrowserPluginMimeType) {
#if defined(ENABLE_MOBILE_YOUTUBE_PLUGIN)
    if (PluginPlaceholder::IsYouTubeURL(url, orig_mime_type))
      return PluginPlaceholder::CreateMobileYoutubePlugin(render_view, frame,
          original_params)->plugin();
#endif
    MissingPluginReporter::GetInstance()->ReportPluginMissing(
        orig_mime_type, url);
    placeholder = PluginPlaceholder::CreateMissingPlugin(
        render_view, frame, original_params);
  } else {
    // TODO(bauerb): This should be in content/.
    WebPluginParams params(original_params);
    for (size_t i = 0; i < plugin.mime_types.size(); ++i) {
      if (plugin.mime_types[i].mime_type == actual_mime_type) {
        AppendParams(plugin.mime_types[i].additional_param_names,
                     plugin.mime_types[i].additional_param_values,
                     &params.attributeNames,
                     &params.attributeValues);
        break;
      }
    }
    if (params.mimeType.isNull() && (actual_mime_type.size() > 0)) {
      // Webkit might say that mime type is null while we already know the
      // actual mime type via ChromeViewHostMsg_GetPluginInfo. In that case
      // we should use what we know since WebpluginDelegateProxy does some
      // specific initializations based on this information.
      params.mimeType = WebString::fromUTF8(actual_mime_type.c_str());
    }

    ContentSettingsObserver* observer =
        ContentSettingsObserver::Get(render_view);

    bool is_nacl_plugin =
        plugin.name ==
            ASCIIToUTF16(chrome::ChromeContentClient::kNaClPluginName);
    ContentSettingsType content_type =
        is_nacl_plugin ? CONTENT_SETTINGS_TYPE_JAVASCRIPT :
                         CONTENT_SETTINGS_TYPE_PLUGINS;

    if ((status_value ==
             ChromeViewHostMsg_GetPluginInfo_Status::kUnauthorized ||
         status_value == ChromeViewHostMsg_GetPluginInfo_Status::kClickToPlay ||
         status_value == ChromeViewHostMsg_GetPluginInfo_Status::kBlocked) &&
        observer->IsPluginTemporarilyAllowed(identifier)) {
      status_value = ChromeViewHostMsg_GetPluginInfo_Status::kAllowed;
    }

    // Allow full-page plug-ins for click-to-play.
    if (status_value == ChromeViewHostMsg_GetPluginInfo_Status::kClickToPlay &&
        !frame->parent() &&
        !frame->opener() &&
        frame->document().isPluginDocument()) {
      status_value = ChromeViewHostMsg_GetPluginInfo_Status::kAllowed;
    }

    switch (status_value) {
      case ChromeViewHostMsg_GetPluginInfo_Status::kNotFound: {
        NOTREACHED();
        break;
      }
      case ChromeViewHostMsg_GetPluginInfo_Status::kAllowed: {
        const char* kNaClMimeType = "application/x-nacl";
        bool is_nacl_mime_type = actual_mime_type == kNaClMimeType;
        bool is_nacl_unrestricted;
        if (is_nacl_plugin) {
          is_nacl_unrestricted = CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kEnableNaCl);
        } else {
          // If this is an external plugin that handles the NaCl mime type, we
          // allow Native Client, so Native Client's integration tests work.
          is_nacl_unrestricted = true;
        }
        if (is_nacl_plugin || is_nacl_mime_type) {
          GURL manifest_url = is_nacl_mime_type ?
              url : GetNaClContentHandlerURL(actual_mime_type, plugin);
          const Extension* extension =
              extension_dispatcher_->extensions()->GetExtensionOrAppByURL(
                  ExtensionURLInfo(manifest_url));
          bool is_extension_from_webstore =
              extension && extension->from_webstore();
          // Allow built-in extensions and extensions under development.
          bool is_extension_unrestricted = extension &&
              (extension->location() == Extension::COMPONENT ||
              extension->location() == Extension::LOAD);
          GURL top_url = frame->top()->document().url();
          if (!IsNaClAllowed(manifest_url,
                             top_url,
                             is_nacl_unrestricted,
                             is_extension_unrestricted,
                             is_extension_from_webstore,
                             &params)) {
            frame->addMessageToConsole(
                WebConsoleMessage(
                    WebConsoleMessage::LevelError,
                    "Only unpacked extensions and apps installed from the "
                    "Chrome Web Store can load NaCl modules without enabling "
                    "Native Client in about:flags."));
            placeholder = PluginPlaceholder::CreateBlockedPlugin(
                render_view, frame, params, plugin, identifier, group_name,
                IDR_BLOCKED_PLUGIN_HTML,
#if defined(OS_CHROMEOS)
                l10n_util::GetStringUTF16(IDS_NACL_PLUGIN_BLOCKED));
#else
                l10n_util::GetStringFUTF16(IDS_PLUGIN_BLOCKED, group_name));
#endif
            break;
          }
        }

        // Delay loading plugins if prerendering.
        // TODO(mmenke):  In the case of prerendering, feed into
        //                ChromeContentRendererClient::CreatePlugin instead, to
        //                reduce the chance of future regressions.
        if (prerender::PrerenderHelper::IsPrerendering(render_view)) {
          placeholder = PluginPlaceholder::CreateBlockedPlugin(
              render_view, frame, params, plugin, identifier, group_name,
              IDR_CLICK_TO_PLAY_PLUGIN_HTML,
              l10n_util::GetStringFUTF16(IDS_PLUGIN_LOAD, group_name));
          placeholder->set_blocked_for_prerendering(true);
          placeholder->set_allow_loading(true);
          break;
        }

        return render_view->CreatePlugin(frame, plugin, params);
      }
      case ChromeViewHostMsg_GetPluginInfo_Status::kNPAPINotSupported: {
        RenderThread::Get()->RecordUserMetrics("Plugin_NPAPINotSupported");
        placeholder = PluginPlaceholder::CreateBlockedPlugin(
            render_view, frame, params, plugin, identifier, group_name,
            IDR_BLOCKED_PLUGIN_HTML,
            l10n_util::GetStringUTF16(IDS_PLUGIN_NOT_SUPPORTED_METRO));
        render_view->Send(new ChromeViewHostMsg_NPAPINotSupported(
            render_view->GetRoutingID(), identifier));
        break;
      }
      case ChromeViewHostMsg_GetPluginInfo_Status::kDisabled: {
        placeholder = PluginPlaceholder::CreateBlockedPlugin(
            render_view, frame, params, plugin, identifier, group_name,
            IDR_DISABLED_PLUGIN_HTML,
            l10n_util::GetStringFUTF16(IDS_PLUGIN_DISABLED, group_name));
        break;
      }
      case ChromeViewHostMsg_GetPluginInfo_Status::kOutdatedBlocked: {
#if defined(ENABLE_PLUGIN_INSTALLATION)
        placeholder = PluginPlaceholder::CreateBlockedPlugin(
            render_view, frame, params, plugin, identifier, group_name,
            IDR_BLOCKED_PLUGIN_HTML,
            l10n_util::GetStringFUTF16(IDS_PLUGIN_OUTDATED, group_name));
        placeholder->set_allow_loading(true);
        render_view->Send(new ChromeViewHostMsg_BlockedOutdatedPlugin(
            render_view->GetRoutingID(), placeholder->CreateRoutingId(),
            identifier));
#else
        NOTREACHED();
#endif
        break;
      }
      case ChromeViewHostMsg_GetPluginInfo_Status::kOutdatedDisallowed: {
        placeholder = PluginPlaceholder::CreateBlockedPlugin(
            render_view, frame, params, plugin, identifier, group_name,
            IDR_BLOCKED_PLUGIN_HTML,
            l10n_util::GetStringFUTF16(IDS_PLUGIN_OUTDATED, group_name));
        break;
      }
      case ChromeViewHostMsg_GetPluginInfo_Status::kUnauthorized: {
        placeholder = PluginPlaceholder::CreateBlockedPlugin(
            render_view, frame, params, plugin, identifier, group_name,
            IDR_BLOCKED_PLUGIN_HTML,
            l10n_util::GetStringFUTF16(IDS_PLUGIN_NOT_AUTHORIZED, group_name));
        placeholder->set_allow_loading(true);
        render_view->Send(new ChromeViewHostMsg_BlockedUnauthorizedPlugin(
            render_view->GetRoutingID(),
            group_name,
            identifier));
        break;
      }
      case ChromeViewHostMsg_GetPluginInfo_Status::kClickToPlay: {
        placeholder = PluginPlaceholder::CreateBlockedPlugin(
            render_view, frame, params, plugin, identifier, group_name,
            IDR_CLICK_TO_PLAY_PLUGIN_HTML,
            l10n_util::GetStringFUTF16(IDS_PLUGIN_LOAD, group_name));
        placeholder->set_allow_loading(true);
        RenderThread::Get()->RecordUserMetrics("Plugin_ClickToPlay");
        observer->DidBlockContentType(content_type, identifier);
        break;
      }
      case ChromeViewHostMsg_GetPluginInfo_Status::kBlocked: {
        placeholder = PluginPlaceholder::CreateBlockedPlugin(
            render_view, frame, params, plugin, identifier, group_name,
            IDR_BLOCKED_PLUGIN_HTML,
            l10n_util::GetStringFUTF16(IDS_PLUGIN_BLOCKED, group_name));
        placeholder->set_allow_loading(true);
        RenderThread::Get()->RecordUserMetrics("Plugin_Blocked");
        observer->DidBlockContentType(content_type, identifier);
        break;
      }
    }
  }
  placeholder->SetStatus(status);
  return placeholder->plugin();
}

// For NaCl content handling plugins, the NaCl manifest is stored in an
// additonal 'nacl' param associated with the MIME type.
//  static
GURL ChromeContentRendererClient::GetNaClContentHandlerURL(
    const std::string& actual_mime_type,
    const webkit::WebPluginInfo& plugin) {
  // Look for the manifest URL among the MIME type's additonal parameters.
  const char* kNaClPluginManifestAttribute = "nacl";
  string16 nacl_attr = ASCIIToUTF16(kNaClPluginManifestAttribute);
  for (size_t i = 0; i < plugin.mime_types.size(); ++i) {
    if (plugin.mime_types[i].mime_type == actual_mime_type) {
      const WebPluginMimeType& content_type = plugin.mime_types[i];
      for (size_t i = 0; i < content_type.additional_param_names.size(); ++i) {
        if (content_type.additional_param_names[i] == nacl_attr)
          return GURL(content_type.additional_param_values[i]);
      }
      break;
    }
  }
  return GURL();
}

//  static
bool ChromeContentRendererClient::IsNaClAllowed(
    const GURL& manifest_url,
    const GURL& top_url,
    bool is_nacl_unrestricted,
    bool is_extension_unrestricted,
    bool is_extension_from_webstore,
    WebPluginParams* params) {
  // Temporarily allow these URLs to run NaCl apps. We should remove this
  // code when PNaCl ships.
  bool is_whitelisted_url =
      ((top_url.SchemeIs("http") || top_url.SchemeIs("https")) &&
      (top_url.host() == "plus.google.com" ||
          top_url.host() == "plus.sandbox.google.com") &&
      top_url.path().find("/games") == 0);

  // Allow Chrome Web Store extensions, built-in extensions, extensions
  // under development, invocations from whitelisted URLs, and all invocations
  // if --enable-nacl is set.
  bool is_nacl_allowed = is_extension_from_webstore ||
                         is_whitelisted_url ||
                         is_extension_unrestricted ||
                         is_nacl_unrestricted;
  if (is_nacl_allowed) {
    bool app_can_use_dev_interfaces =
        // NaCl PDF viewer extension
        (is_extension_from_webstore &&
        manifest_url.SchemeIs("chrome-extension") &&
        manifest_url.host() == "acadkphlmlegjaadjagenfimbpphcgnh");
    // Make sure that PPAPI 'dev' interfaces aren't available for production
    // apps unless they're whitelisted.
    WebString dev_attribute = WebString::fromUTF8("@dev");
    if ((!is_whitelisted_url && !is_extension_from_webstore) ||
        app_can_use_dev_interfaces) {
      // Add the special '@dev' attribute.
      std::vector<string16> param_names;
      std::vector<string16> param_values;
      param_names.push_back(dev_attribute);
      param_values.push_back(WebString());
      AppendParams(
          param_names,
          param_values,
          &params->attributeNames,
          &params->attributeValues);
    } else {
      // If the params somehow contain '@dev', remove it.
      size_t attribute_count = params->attributeNames.size();
      for (size_t i = 0; i < attribute_count; ++i) {
        if (params->attributeNames[i].equals(dev_attribute))
          params->attributeNames[i] = WebString();
      }
    }
  }
  return is_nacl_allowed;
}

bool ChromeContentRendererClient::HasErrorPage(int http_status_code,
                                               std::string* error_domain) {
  // Use an internal error page, if we have one for the status code.
  if (!LocalizedError::HasStrings(LocalizedError::kHttpErrorDomain,
                                  http_status_code)) {
    return false;
  }

  *error_domain = LocalizedError::kHttpErrorDomain;
  return true;
}

void ChromeContentRendererClient::GetNavigationErrorStrings(
    const WebKit::WebURLRequest& failed_request,
    const WebKit::WebURLError& error,
    std::string* error_html,
    string16* error_description) {
  const GURL failed_url = error.unreachableURL;
  const Extension* extension = NULL;
  const bool is_repost =
      error.reason == net::ERR_CACHE_MISS &&
      error.domain == WebString::fromUTF8(net::kErrorDomain) &&
      EqualsASCII(failed_request.httpMethod(), "POST");

  if (failed_url.is_valid() &&
      !failed_url.SchemeIs(extensions::kExtensionScheme)) {
    extension = extension_dispatcher_->extensions()->GetExtensionOrAppByURL(
        ExtensionURLInfo(failed_url));
  }

  if (error_html) {
    // Use a local error page.
    int resource_id;
    DictionaryValue error_strings;
    if (extension && !extension->from_bookmark()) {
      LocalizedError::GetAppErrorStrings(error, failed_url, extension,
                                         &error_strings);

      // TODO(erikkay): Should we use a different template for different
      // error messages?
      resource_id = IDR_ERROR_APP_HTML;
    } else {
      if (is_repost) {
        LocalizedError::GetFormRepostStrings(failed_url, &error_strings);
      } else {
        LocalizedError::GetStrings(error, &error_strings,
                                   RenderThread::Get()->GetLocale());
      }
      resource_id = IDR_NET_ERROR_HTML;
    }

    const base::StringPiece template_html(
        ResourceBundle::GetSharedInstance().GetRawDataResource(
            resource_id));
    if (template_html.empty()) {
      NOTREACHED() << "unable to load template. ID: " << resource_id;
    } else {
      // "t" is the id of the templates root node.
      *error_html = webui::GetTemplatesHtml(template_html, &error_strings, "t");
    }
  }

  if (error_description) {
    if (!extension && !is_repost)
      *error_description = LocalizedError::GetErrorDetails(error);
  }
}

bool ChromeContentRendererClient::RunIdleHandlerWhenWidgetsHidden() {
  return !extension_dispatcher_->is_extension_process();
}

bool ChromeContentRendererClient::AllowPopup() {
  extensions::ChromeV8Context* current_context =
      extension_dispatcher_->v8_context_set().GetCurrent();
  return current_context && current_context->extension() &&
      (current_context->context_type() ==
       extensions::Feature::BLESSED_EXTENSION_CONTEXT ||
       current_context->context_type() ==
       extensions::Feature::CONTENT_SCRIPT_CONTEXT);
}

bool ChromeContentRendererClient::ShouldFork(WebFrame* frame,
                                             const GURL& url,
                                             bool is_initial_navigation,
                                             bool* send_referrer) {
  DCHECK(!frame->parent());

  // If |url| matches one of the prerendered URLs, stop this navigation and try
  // to swap in the prerendered page on the browser process. If the prerendered
  // page no longer exists by the time the OpenURL IPC is handled, a normal
  // navigation is attempted.
  if (prerender_dispatcher_.get() && prerender_dispatcher_->IsPrerenderURL(url))
    return true;

  const ExtensionSet* extensions = extension_dispatcher_->extensions();

  // Determine if the new URL is an extension (excluding bookmark apps).
  const Extension* new_url_extension = extensions::GetNonBookmarkAppExtension(
      *extensions, ExtensionURLInfo(url));
  bool is_extension_url = !!new_url_extension;

  // If the navigation would cross an app extent boundary, we also need
  // to defer to the browser to ensure process isolation.
  // TODO(erikkay) This is happening inside of a check to is_content_initiated
  // which means that things like the back button won't trigger it.  Is that
  // OK?
  if (CrossesExtensionExtents(frame, url, *extensions, is_extension_url,
          is_initial_navigation)) {
    // Include the referrer in this case since we're going from a hosted web
    // page. (the packaged case is handled previously by the extension
    // navigation test)
    *send_referrer = true;

    const Extension* extension =
        extension_dispatcher_->extensions()->GetExtensionOrAppByURL(
            ExtensionURLInfo(url));
    if (extension && extension->is_app()) {
      UMA_HISTOGRAM_ENUMERATION(
          extension_misc::kAppLaunchHistogram,
          extension_misc::APP_LAUNCH_CONTENT_NAVIGATION,
          extension_misc::APP_LAUNCH_BUCKET_BOUNDARY);
    }
    return true;
  }

  // If this is a reload, check whether it has the wrong process type.  We
  // should send it to the browser if it's an extension URL (e.g., hosted app)
  // in a normal process, or if it's a process for an extension that has been
  // uninstalled.
  if (frame->top()->document().url() == url) {
    if (is_extension_url != extension_dispatcher_->is_extension_process())
      return true;
  }

  return false;
}

bool ChromeContentRendererClient::WillSendRequest(
    WebKit::WebFrame* frame,
    content::PageTransition transition_type,
    const GURL& url,
    const GURL& first_party_for_cookies,
    GURL* new_url) {
  // Check whether the request should be allowed. If not allowed, we reset the
  // URL to something invalid to prevent the request and cause an error.
  if (url.SchemeIs(extensions::kExtensionScheme) &&
      !extensions::ResourceRequestPolicy::CanRequestResource(
          url,
          frame,
          transition_type,
          extension_dispatcher_->extensions())) {
    *new_url = GURL(chrome::kExtensionInvalidRequestURL);
    return true;
  }

  if (url.SchemeIs(chrome::kExtensionResourceScheme) &&
      !extensions::ResourceRequestPolicy::CanRequestExtensionResourceScheme(
          url,
          frame)) {
    *new_url = GURL(chrome::kExtensionResourceInvalidRequestURL);
    return true;
  }

  return false;
}

bool ChromeContentRendererClient::ShouldPumpEventsDuringCookieMessage() {
  // We no longer pump messages, even under Chrome Frame. We rely on cookie
  // read requests handled by CF not putting up UI or causing other actions
  // that would require us to pump messages. This fixes http://crbug.com/110090.
  return false;
}

void ChromeContentRendererClient::DidCreateScriptContext(
    WebFrame* frame, v8::Handle<v8::Context> context, int extension_group,
    int world_id) {
  extension_dispatcher_->DidCreateScriptContext(
      frame, context, extension_group, world_id);
}

void ChromeContentRendererClient::WillReleaseScriptContext(
    WebFrame* frame, v8::Handle<v8::Context> context, int world_id) {
  extension_dispatcher_->WillReleaseScriptContext(frame, context, world_id);
}

unsigned long long ChromeContentRendererClient::VisitedLinkHash(
    const char* canonical_url, size_t length) {
  return visited_link_slave_->ComputeURLFingerprint(canonical_url, length);
}

bool ChromeContentRendererClient::IsLinkVisited(unsigned long long link_hash) {
  return visited_link_slave_->IsVisited(link_hash);
}

void ChromeContentRendererClient::PrefetchHostName(const char* hostname,
                                                   size_t length) {
  net_predictor_->Resolve(hostname, length);
}

bool ChromeContentRendererClient::ShouldOverridePageVisibilityState(
    const content::RenderView* render_view,
    WebKit::WebPageVisibilityState* override_state) const {
  if (!prerender::PrerenderHelper::IsPrerendering(render_view))
    return false;

  *override_state = WebKit::WebPageVisibilityStatePrerender;
  return true;
}

bool ChromeContentRendererClient::HandleGetCookieRequest(
    content::RenderView* sender,
    const GURL& url,
    const GURL& first_party_for_cookies,
    std::string* cookies) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kChromeFrame)) {
    IPC::SyncMessage* msg = new ChromeViewHostMsg_GetCookies(
        MSG_ROUTING_NONE, url, first_party_for_cookies, cookies);
    sender->Send(msg);
    return true;
  }
  return false;
}

bool ChromeContentRendererClient::HandleSetCookieRequest(
    content::RenderView* sender,
    const GURL& url,
    const GURL& first_party_for_cookies,
    const std::string& value) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kChromeFrame)) {
    sender->Send(new ChromeViewHostMsg_SetCookie(
        MSG_ROUTING_NONE, url, first_party_for_cookies, value));
    return true;
  }
  return false;
}

void ChromeContentRendererClient::SetExtensionDispatcher(
    extensions::Dispatcher* extension_dispatcher) {
  extension_dispatcher_.reset(extension_dispatcher);
}

bool ChromeContentRendererClient::CrossesExtensionExtents(
    WebFrame* frame,
    const GURL& new_url,
    const ExtensionSet& extensions,
    bool is_extension_url,
    bool is_initial_navigation) {
  GURL old_url(frame->top()->document().url());

  // If old_url is still empty and this is an initial navigation, then this is
  // a window.open operation.  We should look at the opener URL.
  if (is_initial_navigation && old_url.is_empty() && frame->opener()) {
    // If we're about to open a normal web page from a same-origin opener stuck
    // in an extension process, we want to keep it in process to allow the
    // opener to script it.
    WebDocument opener_document = frame->opener()->document();
    GURL opener_url = opener_document.url();
    WebSecurityOrigin opener_origin = opener_document.securityOrigin();
    bool opener_is_extension_url = !!extensions.GetExtensionOrAppByURL(
        ExtensionURLInfo(opener_origin, opener_url));
    WebSecurityOrigin opener = frame->opener()->document().securityOrigin();
    if (!is_extension_url &&
        !opener_is_extension_url &&
        extension_dispatcher_->is_extension_process() &&
        opener.canRequest(WebURL(new_url)))
      return false;

    // In all other cases, we want to compare against the top frame's URL (as
    // opposed to the opener frame's), since that's what determines the type of
    // process.  This allows iframes outside an app to open a popup in the app.
    old_url = frame->top()->opener()->top()->document().url();
  }

  // Only consider keeping non-app URLs in an app process if this window
  // has an opener (in which case it might be an OAuth popup that tries to
  // script an iframe within the app).
  bool should_consider_workaround = !!frame->opener();

  return extensions::CrossesExtensionProcessBoundary(
      extensions, ExtensionURLInfo(old_url), ExtensionURLInfo(new_url),
      should_consider_workaround);
}

void ChromeContentRendererClient::SetSpellcheck(SpellCheck* spellcheck) {
  RenderThread* thread = RenderThread::Get();
  if (spellcheck_.get() && thread)
    thread->RemoveObserver(spellcheck_.get());
  spellcheck_.reset(spellcheck);
  SpellCheckReplacer replacer(spellcheck_.get());
  content::RenderView::ForEach(&replacer);
  if (thread)
    thread->AddObserver(spellcheck_.get());
}

void ChromeContentRendererClient::OnPurgeMemory() {
  DVLOG(1) << "Resetting spellcheck in renderer client";
  SetSpellcheck(new SpellCheck());
}

bool ChromeContentRendererClient::IsAdblockInstalled() {
  return extension_dispatcher_->extensions()->Contains(
      "gighmmpiobklfepjocnamgkkbiglidom");
}

bool ChromeContentRendererClient::IsAdblockPlusInstalled() {
  return extension_dispatcher_->extensions()->Contains(
      "cfhdojbkjhnklbpkdaibdccddilifddb");
}

bool ChromeContentRendererClient::IsAdblockWithWebRequestInstalled() {
  return extension_dispatcher_->IsAdblockWithWebRequestInstalled();
}

bool ChromeContentRendererClient::IsAdblockPlusWithWebRequestInstalled() {
  return extension_dispatcher_->IsAdblockPlusWithWebRequestInstalled();
}

bool ChromeContentRendererClient::IsOtherExtensionWithWebRequestInstalled() {
  return extension_dispatcher_->IsOtherExtensionWithWebRequestInstalled();
}

void ChromeContentRendererClient::RegisterPPAPIInterfaceFactories(
    webkit::ppapi::PpapiInterfaceFactoryManager* factory_manager) {
#if defined(ENABLE_PLUGINS)
  factory_manager->RegisterFactory(ChromePPAPIInterfaceFactory);
#endif
}

}  // namespace chrome
