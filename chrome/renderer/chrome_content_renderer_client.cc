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
#include "chrome/renderer/autofill/form_manager.h"
#include "chrome/renderer/autofill/password_autofill_manager.h"
#include "chrome/renderer/automation/automation_renderer_helper.h"
#include "chrome/renderer/automation/dom_automation_v8_extension.h"
#include "chrome/renderer/blocked_plugin.h"
#include "chrome/renderer/chrome_render_process_observer.h"
#include "chrome/renderer/chrome_render_view_observer.h"
#include "chrome/renderer/content_settings_observer.h"
#include "chrome/renderer/devtools_agent.h"
#include "chrome/renderer/devtools_agent_filter.h"
#include "chrome/renderer/extensions/bindings_utils.h"
#include "chrome/renderer/extensions/event_bindings.h"
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
#include "chrome/renderer/text_input_client_observer.h"
#include "chrome/renderer/translate_helper.h"
#include "chrome/renderer/visitedlink_slave.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_thread.h"
#include "content/renderer/render_view.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/renderer_resources.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCache.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDataSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginParams.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityPolicy.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/ppapi/plugin_module.h"

using autofill::AutofillAgent;
using autofill::FormManager;
using autofill::PasswordAutofillManager;
using WebKit::WebCache;
using WebKit::WebDataSource;
using WebKit::WebFrame;
using WebKit::WebPlugin;
using WebKit::WebPluginParams;
using WebKit::WebSecurityPolicy;
using WebKit::WebString;
using WebKit::WebURLError;
using WebKit::WebURLRequest;
using WebKit::WebURLResponse;
using WebKit::WebVector;

namespace {

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

ChromeContentRendererClient::ChromeContentRendererClient() {
}

ChromeContentRendererClient::~ChromeContentRendererClient() {
}

void ChromeContentRendererClient::RenderThreadStarted() {
  chrome_observer_.reset(new ChromeRenderProcessObserver());
  extension_dispatcher_.reset(new ExtensionDispatcher());
  histogram_snapshots_.reset(new RendererHistogramSnapshots());
  net_predictor_.reset(new RendererNetPredictor());
  spellcheck_.reset(new SpellCheck());
  visited_link_slave_.reset(new VisitedLinkSlave());
  phishing_classifier_.reset(safe_browsing::PhishingClassifierFilter::Create());

  RenderThread* thread = RenderThread::current();
  thread->AddFilter(new DevToolsAgentFilter());

  thread->AddObserver(chrome_observer_.get());
  thread->AddObserver(extension_dispatcher_.get());
  thread->AddObserver(histogram_snapshots_.get());
  thread->AddObserver(phishing_classifier_.get());
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
          switches::kEnableIPCFuzzing)) {
    thread->channel()->set_outgoing_message_filter(LoadExternalIPCFuzzer());
  }
  // chrome: pages should not be accessible by normal content, and should
  // also be unable to script anything but themselves (to help limit the damage
  // that a corrupt chrome: page could cause).
  WebString chrome_ui_scheme(ASCIIToUTF16(chrome::kChromeUIScheme));
  WebSecurityPolicy::registerURLSchemeAsDisplayIsolated(chrome_ui_scheme);

  // chrome-extension: resources shouldn't trigger insecure content warnings.
  WebString extension_scheme(ASCIIToUTF16(chrome::kExtensionScheme));
  WebSecurityPolicy::registerURLSchemeAsSecure(extension_scheme);
}

void ChromeContentRendererClient::RenderViewCreated(RenderView* render_view) {
  safe_browsing::PhishingClassifierDelegate* phishing_classifier = NULL;
#ifndef OS_CHROMEOS
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableClientSidePhishingDetection)) {
    phishing_classifier =
        safe_browsing::PhishingClassifierDelegate::Create(render_view, NULL);
  }
#endif

  ContentSettingsObserver* content_settings =
      new ContentSettingsObserver(render_view);
  new DevToolsAgent(render_view);
  new ExtensionHelper(render_view, extension_dispatcher_.get());
  new PageLoadHistograms(render_view, histogram_snapshots_.get());
  new PrintWebViewHelper(render_view);
  new SearchBox(render_view);
  new SpellCheckProvider(render_view, spellcheck_.get());
  safe_browsing::MalwareDOMDetails::Create(render_view);

#if defined(OS_MACOSX)
  new TextInputClientObserver(render_view);
#endif  // defined(OS_MACOSX)

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
      render_view, content_settings, extension_dispatcher_.get(),
      translate, phishing_classifier);

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

WebPlugin* ChromeContentRendererClient::CreatePlugin(
      RenderView* render_view,
      WebFrame* frame,
      const WebPluginParams& original_params) {
  bool found = false;
  CommandLine* cmd = CommandLine::ForCurrentProcess();
  webkit::npapi::WebPluginInfo info;
  GURL url(original_params.url);
  std::string actual_mime_type;
  render_view->Send(new ViewHostMsg_GetPluginInfo(
      render_view->routing_id(), url, frame->top()->url(),
      original_params.mimeType.utf8(), &found, &info, &actual_mime_type));

  if (!found)
    return NULL;
  if (!webkit::npapi::IsPluginEnabled(info))
    return NULL;

  const webkit::npapi::PluginGroup* group =
      webkit::npapi::PluginList::Singleton()->GetPluginGroup(info);
  DCHECK(group != NULL);

  ContentSetting plugin_setting = CONTENT_SETTING_DEFAULT;
  std::string resource = group->identifier();
  render_view->Send(new ViewHostMsg_GetPluginContentSetting(
      frame->top()->url(), resource, &plugin_setting));
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
  if (group->IsVulnerable() || group->RequiresAuthorization()) {
    // These policies are dynamic and can changed at runtime, so they aren't
    // cached here.
    render_view->Send(new ViewHostMsg_GetPluginPolicies(
        &outdated_policy, &authorize_policy));
  }

  if (group->IsVulnerable()) {
    if (outdated_policy == CONTENT_SETTING_ASK ||
        outdated_policy == CONTENT_SETTING_BLOCK) {
      if (outdated_policy == CONTENT_SETTING_ASK) {
        render_view->Send(new ViewHostMsg_BlockedOutdatedPlugin(
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

  if (group->RequiresAuthorization() &&
      authorize_policy == CONTENT_SETTING_ASK &&
      (plugin_setting == CONTENT_SETTING_ALLOW ||
       plugin_setting == CONTENT_SETTING_ASK) &&
      host_setting == CONTENT_SETTING_DEFAULT) {
    render_view->Send(new ViewHostMsg_BlockedOutdatedPlugin(
        render_view->routing_id(), group->GetGroupName(), GURL()));
    return CreatePluginPlaceholder(
        render_view, frame, params, *group, IDR_BLOCKED_PLUGIN_HTML,
        IDS_PLUGIN_NOT_AUTHORIZED, false, true);
  }

  if (info.path.value() == webkit::npapi::kDefaultPluginLibraryName ||
      plugin_setting == CONTENT_SETTING_ALLOW ||
      host_setting == CONTENT_SETTING_ALLOW) {
    // Delay loading plugins if prerendering.
    if (prerender::PrerenderHelper::IsPrerendering(render_view)) {
      return CreatePluginPlaceholder(
          render_view, frame, params, *group, IDR_CLICK_TO_PLAY_PLUGIN_HTML,
          IDS_PLUGIN_LOAD, true, true);
    }

    bool pepper_plugin_was_registered = false;
    scoped_refptr<webkit::ppapi::PluginModule> pepper_module(
        render_view->pepper_delegate()->CreatePepperPlugin(
            info.path, &pepper_plugin_was_registered));
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

  observer->DidBlockContentType(CONTENT_SETTINGS_TYPE_PLUGINS,
      cmd->HasSwitch(switches::kEnableResourceContentSettings) ?
          resource : std::string());
  if (plugin_setting == CONTENT_SETTING_ASK) {
    return CreatePluginPlaceholder(
        render_view, frame, params, *group, IDR_CLICK_TO_PLAY_PLUGIN_HTML,
        IDS_PLUGIN_LOAD, false, true);
  } else {
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
    error.unreachableURL = frame->url();
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
      bindings_utils::GetInfoForCurrentContext();
}

bool ChromeContentRendererClient::ShouldFork(WebFrame* frame,
                                             const GURL& url,
                                             bool is_content_initiated,
                                             bool* send_referrer) {
  // If the navigation would cross an app extent boundary, we also need
  // to defer to the browser to ensure process isolation.
  // TODO(erikkay) This is happening inside of a check to is_content_initiated
  // which means that things like the back button won't trigger it.  Is that
  // OK?
  if (!CrossesExtensionExtents(frame, url))
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
          url, GURL(frame->url()), extension_dispatcher_->extensions())) {
    *new_url = GURL("chrome-extension://invalid/");
    return true;
  }

  return false;
}

FilePath ChromeContentRendererClient::GetMediaLibraryPath() {
  FilePath rv;
  PathService::Get(chrome::DIR_MEDIA_LIBS, &rv);
  return rv;
}

bool ChromeContentRendererClient::ShouldPumpEventsDuringCookieMessage() {
  // We only need to pump events for chrome frame processes as the
  // cookie policy is controlled by the host browser (IE). If the
  // policy is set to prompt then the host would put up UI which
  // would require plugins if any to also pump to ensure that we
  // don't have a deadlock.
  return CommandLine::ForCurrentProcess()->HasSwitch(switches::kChromeFrame);
}

void ChromeContentRendererClient::DidCreateScriptContext(WebFrame* frame) {
  EventBindings::HandleContextCreated(
      frame, false, extension_dispatcher_.get());
}

void ChromeContentRendererClient::DidDestroyScriptContext(WebFrame* frame) {
  EventBindings::HandleContextDestroyed(frame);
}

void ChromeContentRendererClient::DidCreateIsolatedScriptContext(
  WebFrame* frame) {
  EventBindings::HandleContextCreated(frame, true, extension_dispatcher_.get());
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

void ChromeContentRendererClient::SetExtensionDispatcher(
    ExtensionDispatcher* extension_dispatcher) {
  extension_dispatcher_.reset(extension_dispatcher);
}

bool ChromeContentRendererClient::CrossesExtensionExtents(WebFrame* frame,
                                                          const GURL& new_url) {
  const ExtensionSet* extensions = extension_dispatcher_->extensions();
  // If the URL is still empty, this is a window.open navigation. Check the
  // opener's URL.
  GURL old_url(frame->url());
  if (old_url.is_empty() && frame->opener())
    old_url = frame->opener()->url();

  return !extensions->InSameExtent(old_url, new_url);
}

}  // namespace chrome
