// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_content_renderer_client.h"

#include <string>

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/external_ipc_fuzzer.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/autofill/autofill_agent.h"
#include "chrome/renderer/autofill/password_autofill_manager.h"
#include "chrome/renderer/automation/automation_renderer_helper.h"
#include "chrome/renderer/automation/dom_automation_v8_extension.h"
#include "chrome/renderer/benchmarking_extension.h"
#include "chrome/renderer/blocked_plugin.h"
#include "chrome/renderer/chrome_ppapi_interfaces.h"
#include "chrome/renderer/chrome_render_process_observer.h"
#include "chrome/renderer/chrome_render_view_observer.h"
#include "chrome/renderer/content_settings_observer.h"
#include "chrome/renderer/extensions/extension_bindings_context.h"
#include "chrome/renderer/extensions/extension_dispatcher.h"
#include "chrome/renderer/extensions/extension_helper.h"
#include "chrome/renderer/extensions/extension_process_bindings.h"
#include "chrome/renderer/extensions/extension_resource_request_policy.h"
#include "chrome/renderer/extensions/renderer_extension_bindings.h"
#include "chrome/renderer/external_extension.h"
#include "chrome/renderer/loadtimes_extension_bindings.h"
#include "chrome/renderer/localized_error.h"
#include "chrome/renderer/net/renderer_net_predictor.h"
#include "chrome/renderer/page_click_tracker.h"
#include "chrome/renderer/page_load_histograms.h"
#include "chrome/renderer/plugin_uma.h"
#include "chrome/renderer/prerender/prerender_helper.h"
#include "chrome/renderer/print_web_view_helper.h"
#include "chrome/renderer/renderer_histogram_snapshots.h"
#include "chrome/renderer/safe_browsing/malware_dom_details.h"
#include "chrome/renderer/safe_browsing/phishing_classifier_delegate.h"
#include "chrome/renderer/search_extension.h"
#include "chrome/renderer/searchbox.h"
#include "chrome/renderer/searchbox_extension.h"
#include "chrome/renderer/spellchecker/spellcheck.h"
#include "chrome/renderer/spellchecker/spellcheck_provider.h"
#include "chrome/renderer/translate_helper.h"
#include "chrome/renderer/visitedlink_slave.h"
#include "content/renderer/render_thread.h"
#include "content/renderer/render_view.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/renderer_resources.h"
#include "ipc/ipc_sync_message.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCache.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDataSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginParams.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityPolicy.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/ppapi/plugin_module.h"

using autofill::AutofillAgent;
using autofill::PasswordAutofillManager;
using WebKit::WebCache;
using WebKit::WebDataSource;
using WebKit::WebFrame;
using WebKit::WebPlugin;
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

// Constants for UMA statistic collection.
static const char kPluginTypeMismatch[] = "Plugin.PluginTypeMismatch";
static const char kApplicationOctetStream[] = "application/octet-stream";
enum {
  PLUGIN_TYPE_MISMATCH_NONE = 0,
  PLUGIN_TYPE_MISMATCH_ORIG_EMPTY,
  PLUGIN_TYPE_MISMATCH_ORIG_OCTETSTREAM,
  PLUGIN_TYPE_MISMATCH_ORIG_OTHER,
  PLUGIN_TYPE_MISMATCH_NUM_EVENTS,
};

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

}  // namespace

namespace chrome {

ChromeContentRendererClient::ChromeContentRendererClient()
    : spellcheck_provider_(NULL) {
  chrome::InitializePPAPI();
}

ChromeContentRendererClient::~ChromeContentRendererClient() {
  chrome::UninitializePPAPI();
}

void ChromeContentRendererClient::RenderThreadStarted() {
  chrome_observer_.reset(new ChromeRenderProcessObserver(this));
  extension_dispatcher_.reset(new ExtensionDispatcher());
  histogram_snapshots_.reset(new RendererHistogramSnapshots());
  net_predictor_.reset(new RendererNetPredictor());
  spellcheck_.reset(new SpellCheck());
  visited_link_slave_.reset(new VisitedLinkSlave());
#if defined(ENABLE_SAFE_BROWSING)
  phishing_classifier_.reset(safe_browsing::PhishingClassifierFilter::Create());
#endif

  RenderThread* thread = RenderThread::current();

  thread->AddObserver(chrome_observer_.get());
  thread->AddObserver(extension_dispatcher_.get());
  thread->AddObserver(histogram_snapshots_.get());
#if defined(ENABLE_SAFE_BROWSING)
  thread->AddObserver(phishing_classifier_.get());
#endif
  thread->AddObserver(spellcheck_.get());
  thread->AddObserver(visited_link_slave_.get());

  thread->RegisterExtension(extensions_v8::ExternalExtension::Get());
  thread->RegisterExtension(extensions_v8::LoadTimesExtension::Get());
  thread->RegisterExtension(extensions_v8::SearchBoxExtension::Get());
  v8::Extension* search_extension = extensions_v8::SearchExtension::Get();
  // search_extension is null if not enabled.
  if (search_extension)
    thread->RegisterExtension(search_extension);

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDomAutomationController)) {
    thread->RegisterExtension(DomAutomationV8Extension::Get());
  }

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBenchmarking))
    thread->RegisterExtension(extensions_v8::BenchmarkingExtension::Get());

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableIPCFuzzing)) {
    thread->channel()->set_outgoing_message_filter(LoadExternalIPCFuzzer());
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

  // chrome: pages should not be accessible by bookmarklets or javascript:
  // URLs typed in the omnibox.
  WebSecurityPolicy::registerURLSchemeAsNotAllowingJavascriptURLs(
      chrome_ui_scheme);

  // chrome:, and chrome-extension: resources shouldn't trigger insecure
  // content warnings.
  WebSecurityPolicy::registerURLSchemeAsSecure(chrome_ui_scheme);

  WebString extension_scheme(ASCIIToUTF16(chrome::kExtensionScheme));
  WebSecurityPolicy::registerURLSchemeAsSecure(extension_scheme);
}

void ChromeContentRendererClient::RenderViewCreated(RenderView* render_view) {
  ContentSettingsObserver* content_settings =
      new ContentSettingsObserver(render_view);
  new ExtensionHelper(render_view, extension_dispatcher_.get());
  new PageLoadHistograms(render_view, histogram_snapshots_.get());
  new PrintWebViewHelper(render_view);
  new SearchBox(render_view);
  spellcheck_provider_ = new SpellCheckProvider(render_view, spellcheck_.get());
#if defined(ENABLE_SAFE_BROWSING)
  safe_browsing::MalwareDOMDetails::Create(render_view);
#endif

  PasswordAutofillManager* password_autofill_manager =
      new PasswordAutofillManager(render_view);
  AutofillAgent* autofill_agent = new AutofillAgent(render_view,
                                                    password_autofill_manager);
  PageClickTracker* page_click_tracker = new PageClickTracker(render_view);
  // Note that the order of insertion of the listeners is important.
  // The password_autocomplete_manager takes the first shot at processing the
  // notification and can stop the propagation.
  page_click_tracker->AddListener(password_autofill_manager);
  page_click_tracker->AddListener(autofill_agent);

  TranslateHelper* translate = new TranslateHelper(render_view, autofill_agent);
  new ChromeRenderViewObserver(
      render_view, content_settings, chrome_observer_.get(),
      extension_dispatcher_.get(), translate);

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
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(IDR_SAD_PLUGIN);
}

std::string ChromeContentRendererClient::GetDefaultEncoding() {
  return l10n_util::GetStringUTF8(IDS_DEFAULT_ENCODING);
}

bool ChromeContentRendererClient::OverrideCreatePlugin(
      RenderView* render_view,
      WebFrame* frame,
      const WebPluginParams& params,
      WebKit::WebPlugin** plugin) {
  bool is_default_plugin;
  *plugin = CreatePlugin(render_view, frame, params, &is_default_plugin);
  if (!*plugin || is_default_plugin)
    MissingPluginReporter::GetInstance()->ReportPluginMissing(
        params.mimeType.utf8(), params.url);
  return true;
}

WebPlugin* ChromeContentRendererClient::CreatePlugin(
      RenderView* render_view,
      WebFrame* frame,
      const WebPluginParams& original_params,
      bool* is_default_plugin) {
  *is_default_plugin = false;
  CommandLine* cmd = CommandLine::ForCurrentProcess();
  webkit::WebPluginInfo info;
  GURL url(original_params.url);
  std::string orig_mime_type = original_params.mimeType.utf8();
  std::string actual_mime_type;

  bool found = render_view->GetPluginInfo(
      url, frame->top()->document().url(), orig_mime_type, &info,
      &actual_mime_type);

  if (!found)
    return NULL;

  *is_default_plugin =
    info.path.value() == webkit::npapi::kDefaultPluginLibraryName;

  if (orig_mime_type == actual_mime_type) {
    UMA_HISTOGRAM_ENUMERATION(kPluginTypeMismatch,
                              PLUGIN_TYPE_MISMATCH_NONE,
                              PLUGIN_TYPE_MISMATCH_NUM_EVENTS);
  } else if (orig_mime_type.empty()) {
    UMA_HISTOGRAM_ENUMERATION(kPluginTypeMismatch,
                              PLUGIN_TYPE_MISMATCH_ORIG_EMPTY,
                              PLUGIN_TYPE_MISMATCH_NUM_EVENTS);
  } else if (orig_mime_type == kApplicationOctetStream) {
    UMA_HISTOGRAM_ENUMERATION(kPluginTypeMismatch,
                              PLUGIN_TYPE_MISMATCH_ORIG_OCTETSTREAM,
                              PLUGIN_TYPE_MISMATCH_NUM_EVENTS);
  } else {
    UMA_HISTOGRAM_ENUMERATION(kPluginTypeMismatch,
                              PLUGIN_TYPE_MISMATCH_ORIG_OTHER,
                              PLUGIN_TYPE_MISMATCH_NUM_EVENTS);
    // We do not permit URL-sniff based plug-in MIME type overrides aside from
    // the case where the "type" was initially missing or generic
    // (application/octet-stream).
    // We collected stats to determine this approach isn't a major compat issue,
    // and we defend against content confusion attacks in various cases, such
    // as when the user doesn't have the Flash plug-in enabled.
    return NULL;
  }

  scoped_ptr<webkit::npapi::PluginGroup> group(
      webkit::npapi::PluginList::Singleton()->GetPluginGroup(info));

  ContentSettingsType content_type = CONTENT_SETTINGS_TYPE_PLUGINS;
  ContentSetting plugin_setting = CONTENT_SETTING_DEFAULT;
  std::string resource;
  if (cmd->HasSwitch(switches::kEnableResourceContentSettings))
    resource = group->identifier();
  render_view->Send(new ChromeViewHostMsg_GetPluginContentSetting(
      frame->top()->document().url(), resource, &plugin_setting));
  DCHECK(plugin_setting != CONTENT_SETTING_DEFAULT);

  WebPluginParams params(original_params);
  for (size_t i = 0; i < info.mime_types.size(); ++i) {
    if (info.mime_types[i].mime_type == actual_mime_type) {
      AppendParams(info.mime_types[i].additional_param_names,
                   info.mime_types[i].additional_param_values,
                   &params.attributeNames,
                   &params.attributeValues);
      break;
    }
  }

  ContentSetting outdated_policy = CONTENT_SETTING_ASK;
  ContentSetting authorize_policy = CONTENT_SETTING_ASK;
  if (group->IsVulnerable(info) || group->RequiresAuthorization(info)) {
    // These policies are dynamic and can changed at runtime, so they aren't
    // cached here.
    render_view->Send(new ChromeViewHostMsg_GetPluginPolicies(
        &outdated_policy, &authorize_policy));
  }

  if (group->IsVulnerable(info)) {
    if (outdated_policy == CONTENT_SETTING_ASK ||
        outdated_policy == CONTENT_SETTING_BLOCK) {
      if (outdated_policy == CONTENT_SETTING_ASK) {
        render_view->Send(new ChromeViewHostMsg_BlockedOutdatedPlugin(
            render_view->routing_id(), group->GetGroupName(),
            GURL(group->GetUpdateURL())));
      }
      return CreatePluginPlaceholder(
          render_view, frame, params, *group, IDR_BLOCKED_PLUGIN_HTML,
          IDS_PLUGIN_OUTDATED, false, outdated_policy == CONTENT_SETTING_ASK);
    } else {
      DCHECK(outdated_policy == CONTENT_SETTING_ALLOW);
    }
  }

  ContentSettingsObserver* observer = ContentSettingsObserver::Get(render_view);
  ContentSetting host_setting =
      observer->GetContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS);

  if (group->RequiresAuthorization(info) &&
      authorize_policy == CONTENT_SETTING_ASK &&
      (plugin_setting == CONTENT_SETTING_ALLOW ||
       plugin_setting == CONTENT_SETTING_ASK) &&
      host_setting == CONTENT_SETTING_DEFAULT) {
    render_view->Send(new ChromeViewHostMsg_BlockedOutdatedPlugin(
        render_view->routing_id(), group->GetGroupName(), GURL()));
    return CreatePluginPlaceholder(
        render_view, frame, params, *group, IDR_BLOCKED_PLUGIN_HTML,
        IDS_PLUGIN_NOT_AUTHORIZED, false, true);
  }

  // Treat Native Client invocations like Javascript.
  bool is_nacl_plugin =
      info.name == ASCIIToUTF16(ChromeContentClient::kNaClPluginName);
  if (is_nacl_plugin) {
    content_type = CONTENT_SETTINGS_TYPE_JAVASCRIPT;
    plugin_setting =
        observer->GetContentSetting(content_type);
  }

  if (plugin_setting == CONTENT_SETTING_ALLOW ||
      host_setting == CONTENT_SETTING_ALLOW ||
      info.path.value() == webkit::npapi::kDefaultPluginLibraryName) {
    // Delay loading plugins if prerendering.
    if (prerender::PrerenderHelper::IsPrerendering(render_view)) {
      return CreatePluginPlaceholder(
          render_view, frame, params, *group, IDR_CLICK_TO_PLAY_PLUGIN_HTML,
          IDS_PLUGIN_LOAD, true, true);
    }

    // Enforce the Chrome WebStore restriction on the Native Client plugin.
    if (is_nacl_plugin) {
      bool allow_nacl = cmd->HasSwitch(switches::kEnableNaCl);
      if (!allow_nacl) {
        const char* kNaClPluginMimeType = "application/x-nacl";
        const char* kNaClPluginManifestAttribute = "nacl";

        GURL nexe_url;
        if (actual_mime_type == kNaClPluginMimeType) {
          nexe_url = url;  // Normal embedded NaCl plugin.
        } else {
          // Content type handling NaCl plugin; the "nacl" param on the
          // MIME type holds the nexe URL.
          string16 nacl_attr = ASCIIToUTF16(kNaClPluginManifestAttribute);
          for (size_t i = 0; i < info.mime_types.size(); ++i) {
            if (info.mime_types[i].mime_type == actual_mime_type) {
              const webkit::WebPluginMimeType& content_type =
                  info.mime_types[i];
              for (size_t i = 0;
                  i < content_type.additional_param_names.size(); ++i) {
                if (content_type.additional_param_names[i] == nacl_attr) {
                  nexe_url = GURL(content_type.additional_param_values[i]);
                  break;
                }
              }
              break;
            }
          }
        }

        // Create the NaCl plugin only if the .nexe is part of an extension
        // that was installed from the Chrome Web Store, or part of a component
        // extension, or part of an unpacked extension.
        const Extension* extension =
            extension_dispatcher_->extensions()->GetByURL(nexe_url);
        allow_nacl = extension &&
            (extension->from_webstore() ||
            extension->location() == Extension::COMPONENT ||
            extension->location() == Extension::LOAD);
      }

      if (!allow_nacl) {
        // TODO(bbudge) Webkit will crash if this is a full-frame plug-in and
        // we return NULL. Prepare a patch to fix that, and return NULL here.
        return CreatePluginPlaceholder(
            render_view, frame, params, *group, IDR_BLOCKED_PLUGIN_HTML,
            IDS_PLUGIN_BLOCKED, false, false);
      }
    }

    bool pepper_plugin_was_registered = false;
    scoped_refptr<webkit::ppapi::PluginModule> pepper_module(
        render_view->pepper_delegate()->CreatePepperPluginModule(
            info, &pepper_plugin_was_registered));
    if (pepper_plugin_was_registered) {
      if (pepper_module) {
        return render_view->CreatePepperPlugin(
            frame, params, info.path, pepper_module.get());
      }
      return NULL;
    }

    return render_view->CreateNPAPIPlugin(
        frame, params, info.path, actual_mime_type);
  }

  observer->DidBlockContentType(content_type, resource);
  if (plugin_setting == CONTENT_SETTING_ASK) {
    RenderThread::RecordUserMetrics("Plugin_ClickToPlay");
    return CreatePluginPlaceholder(
        render_view, frame, params, *group, IDR_CLICK_TO_PLAY_PLUGIN_HTML,
        IDS_PLUGIN_LOAD, false, true);
  } else {
    RenderThread::RecordUserMetrics("Plugin_Blocked");
    return CreatePluginPlaceholder(
        render_view, frame, params, *group, IDR_BLOCKED_PLUGIN_HTML,
        IDS_PLUGIN_BLOCKED, false, true);
  }
}

WebPlugin* ChromeContentRendererClient::CreatePluginPlaceholder(
    RenderView* render_view,
    WebFrame* frame,
    const WebPluginParams& params,
    const webkit::npapi::PluginGroup& group,
    int resource_id,
    int message_id,
    bool is_blocked_for_prerendering,
    bool allow_loading) {
  // |blocked_plugin| will delete itself when the WebViewPlugin
  // is destroyed.
  BlockedPlugin* blocked_plugin =
      new BlockedPlugin(render_view,
                        frame,
                        group,
                        params,
                        render_view->webkit_preferences(),
                        resource_id,
                        l10n_util::GetStringFUTF16(message_id,
                                                   group.GetGroupName()),
                        is_blocked_for_prerendering,
                        allow_loading);
  return blocked_plugin->plugin();
}

void ChromeContentRendererClient::ShowErrorPage(RenderView* render_view,
                                                WebKit::WebFrame* frame,
                                                int http_status_code) {
  // Use an internal error page, if we have one for the status code.
  if (LocalizedError::HasStrings(LocalizedError::kHttpErrorDomain,
                                 http_status_code)) {
    WebURLError error;
    error.unreachableURL = frame->document().url();
    error.domain = WebString::fromUTF8(LocalizedError::kHttpErrorDomain);
    error.reason = http_status_code;

    render_view->LoadNavigationErrorPage(
        frame, frame->dataSource()->request(), error, std::string(), true);
  }
}

std::string ChromeContentRendererClient::GetNavigationErrorHtml(
    const WebURLRequest& failed_request,
    const WebURLError& error) {
  GURL failed_url = error.unreachableURL;
  std::string html;
  const Extension* extension = NULL;

  // Use a local error page.
  int resource_id;
  DictionaryValue error_strings;
  if (failed_url.is_valid() && !failed_url.SchemeIs(chrome::kExtensionScheme))
    extension = extension_dispatcher_->extensions()->GetByURL(failed_url);
  if (extension) {
    LocalizedError::GetAppErrorStrings(error, failed_url, extension,
                                       &error_strings);

    // TODO(erikkay): Should we use a different template for different
    // error messages?
    resource_id = IDR_ERROR_APP_HTML;
  } else {
    if (error.domain == WebString::fromUTF8(net::kErrorDomain) &&
        error.reason == net::ERR_CACHE_MISS &&
        EqualsASCII(failed_request.httpMethod(), "POST")) {
      LocalizedError::GetFormRepostStrings(failed_url, &error_strings);
    } else {
      LocalizedError::GetStrings(error, &error_strings);
    }
    resource_id = IDR_NET_ERROR_HTML;
  }

  const base::StringPiece template_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(resource_id));
  if (template_html.empty()) {
    NOTREACHED() << "unable to load template. ID: " << resource_id;
  } else {
    // "t" is the id of the templates root node.
    html = jstemplate_builder::GetTemplatesHtml(
        template_html, &error_strings, "t");
  }

  return html;
}

bool ChromeContentRendererClient::RunIdleHandlerWhenWidgetsHidden() {
  return !extension_dispatcher_->is_extension_process();
}

bool ChromeContentRendererClient::AllowPopup(const GURL& creator) {
  // Extensions and apps always allowed to create unrequested popups. The second
  // check is necessary to include content scripts.
  return extension_dispatcher_->extensions()->GetByURL(creator) ||
      ExtensionBindingsContext::GetCurrent();
}

bool ChromeContentRendererClient::ShouldFork(WebFrame* frame,
                                             const GURL& url,
                                             bool is_content_initiated,
                                             bool is_initial_navigation,
                                             bool* send_referrer) {
  // If the navigation would cross an app extent boundary, we also need
  // to defer to the browser to ensure process isolation.
  // TODO(erikkay) This is happening inside of a check to is_content_initiated
  // which means that things like the back button won't trigger it.  Is that
  // OK?
  if (!CrossesExtensionExtents(frame, url, is_initial_navigation))
    return false;

  // Include the referrer in this case since we're going from a hosted web
  // page. (the packaged case is handled previously by the extension
  // navigation test)
  *send_referrer = true;

  if (is_content_initiated) {
    const Extension* extension =
        extension_dispatcher_->extensions()->GetByURL(url);
    if (extension && extension->is_app()) {
      UMA_HISTOGRAM_ENUMERATION(
          extension_misc::kAppLaunchHistogram,
          extension_misc::APP_LAUNCH_CONTENT_NAVIGATION,
          extension_misc::APP_LAUNCH_BUCKET_BOUNDARY);
    }
  }

  return true;
}

bool ChromeContentRendererClient::WillSendRequest(WebKit::WebFrame* frame,
                                                  const GURL& url,
                                                  GURL* new_url) {
  // If the request is for an extension resource, check whether it should be
  // allowed. If not allowed, we reset the URL to something invalid to prevent
  // the request and cause an error.
  if (url.SchemeIs(chrome::kExtensionScheme) &&
      !ExtensionResourceRequestPolicy::CanRequestResource(
          url,
          GURL(frame->document().url()),
          extension_dispatcher_->extensions())) {
    *new_url = GURL("chrome-extension://invalid/");
    return true;
  }

  return false;
}

bool ChromeContentRendererClient::ShouldPumpEventsDuringCookieMessage() {
  // We only need to pump events for chrome frame processes as the
  // cookie policy is controlled by the host browser (IE). If the
  // policy is set to prompt then the host would put up UI which
  // would require plugins if any to also pump to ensure that we
  // don't have a deadlock.
  return CommandLine::ForCurrentProcess()->HasSwitch(switches::kChromeFrame);
}

void ChromeContentRendererClient::DidCreateScriptContext(
    WebFrame* frame, v8::Handle<v8::Context> context, int world_id) {
  ExtensionBindingsContext::HandleV8ContextCreated(
      frame,
      context,
      extension_dispatcher_.get(),
      world_id);
}

void ChromeContentRendererClient::WillReleaseScriptContext(
    WebFrame* frame, v8::Handle<v8::Context> context, int world_id) {
  ExtensionBindingsContext::HandleV8ContextReleased(
      frame,
      context,
      world_id);
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
    const RenderView* render_view,
    WebKit::WebPageVisibilityState* override_state) const {
  if (!prerender::PrerenderHelper::IsPrerendering(render_view))
    return false;

  *override_state = WebKit::WebPageVisibilityStatePrerender;
  return true;
}

bool ChromeContentRendererClient::HandleGetCookieRequest(
    RenderView* sender,
    const GURL& url,
    const GURL& first_party_for_cookies,
    std::string* cookies) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kChromeFrame)) {
    IPC::SyncMessage* msg = new ChromeViewHostMsg_GetCookies(
        MSG_ROUTING_NONE, url, first_party_for_cookies, cookies);
    msg->EnableMessagePumping();
    sender->Send(msg);
    return true;
  }
  return false;
}

bool ChromeContentRendererClient::HandleSetCookieRequest(
    RenderView* sender,
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

bool ChromeContentRendererClient::IsProtocolSupportedForMedia(
    const GURL& url) {
  return url.SchemeIs(chrome::kExtensionScheme);
}


void ChromeContentRendererClient::SetExtensionDispatcher(
    ExtensionDispatcher* extension_dispatcher) {
  extension_dispatcher_.reset(extension_dispatcher);
}

bool ChromeContentRendererClient::CrossesExtensionExtents(
    WebFrame* frame,
    const GURL& new_url,
    bool is_initial_navigation) {
  const ExtensionSet* extensions = extension_dispatcher_->extensions();
  bool is_extension_url = !!extensions->GetByURL(new_url);
  GURL old_url(frame->top()->document().url());

  // If old_url is still empty and this is an initial navigation, then this is
  // a window.open operation.  We should look at the opener URL.
  if (is_initial_navigation && old_url.is_empty() && frame->opener()) {
    // If we're about to open a normal web page from a same-origin opener stuck
    // in an extension process, we want to keep it in process to allow the
    // opener to script it.
    GURL opener_url = frame->opener()->document().url();
    bool opener_is_extension_url = !!extensions->GetByURL(opener_url);
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

  // If this is a reload, check whether it has the wrong process type.  We
  // should send it to the browser if it's an extension URL (e.g., hosted app)
  // in a normal process, or if it's a process for an extension that has been
  // uninstalled.
  if (old_url == new_url) {
    if (is_extension_url != extension_dispatcher_->is_extension_process())
      return true;
  }

  return !extensions->InSameExtent(old_url, new_url);
}

void ChromeContentRendererClient::OnPurgeMemory() {
  DVLOG(1) << "Resetting spellcheck in renderer client";
  RenderThread* thread = RenderThread::current();
  if (spellcheck_.get())
    thread->RemoveObserver(spellcheck_.get());
  SpellCheck* new_spellcheck = new SpellCheck();
  if (spellcheck_provider_)
    spellcheck_provider_->SetSpellCheck(new_spellcheck);
  spellcheck_.reset(new_spellcheck);
  thread->AddObserver(new_spellcheck);
}

}  // namespace chrome
