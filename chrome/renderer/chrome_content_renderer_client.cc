// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_content_renderer_client.h"

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_settings_pattern.h"
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
#include "chrome/renderer/benchmarking_extension.h"
#include "chrome/renderer/chrome_ppapi_interfaces.h"
#include "chrome/renderer/chrome_render_process_observer.h"
#include "chrome/renderer/chrome_render_view_observer.h"
#include "chrome/renderer/content_settings_observer.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "chrome/renderer/extensions/chrome_v8_extension.h"
#include "chrome/renderer/extensions/extension_dispatcher.h"
#include "chrome/renderer/extensions/extension_helper.h"
#include "chrome/renderer/extensions/extension_resource_request_policy.h"
#include "chrome/renderer/extensions/miscellaneous_bindings.h"
#include "chrome/renderer/extensions/schema_generated_bindings.h"
#include "chrome/renderer/external_extension.h"
#include "chrome/renderer/loadtimes_extension_bindings.h"
#include "chrome/renderer/localized_error.h"
#include "chrome/renderer/net/renderer_net_predictor.h"
#include "chrome/renderer/page_click_tracker.h"
#include "chrome/renderer/page_load_histograms.h"
#include "chrome/renderer/plugins/blocked_plugin.h"
#include "chrome/renderer/plugins/missing_plugin.h"
#include "chrome/renderer/plugins/plugin_uma.h"
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
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/renderer_resources.h"
#include "ipc/ipc_sync_channel.h"
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
#include "webkit/plugins/ppapi/ppapi_interface_factory.h"

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
using autofill::AutofillAgent;
using autofill::PasswordAutofillManager;
using content::RenderThread;

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

ChromeContentRendererClient::ChromeContentRendererClient()
    : spellcheck_provider_(NULL) {
}

ChromeContentRendererClient::~ChromeContentRendererClient() {
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

  RenderThread* thread = RenderThread::Get();

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
    thread->RegisterExtension(new ChromeV8Extension(
        "dom_automation.js", IDR_DOM_AUTOMATION_JS, NULL));
  }

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBenchmarking))
    thread->RegisterExtension(extensions_v8::BenchmarkingExtension::Get());

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableIPCFuzzing)) {
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

void ChromeContentRendererClient::RenderViewCreated(
    content::RenderView* render_view) {
  ContentSettingsObserver* content_settings =
      new ContentSettingsObserver(render_view);
  if (chrome_observer_.get()) {
    content_settings->SetContentSettingRules(
        chrome_observer_->content_setting_rules());
  }
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

  TranslateHelper* translate = new TranslateHelper(render_view);
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
    content::RenderView* render_view,
    WebFrame* frame,
    const WebPluginParams& params,
    WebKit::WebPlugin** plugin) {
  *plugin = CreatePlugin(render_view, frame, params);
  return true;
}

WebPlugin* ChromeContentRendererClient::CreatePlugin(
    content::RenderView* render_view,
    WebFrame* frame,
    const WebPluginParams& original_params) {
  CommandLine* cmd = CommandLine::ForCurrentProcess();
  GURL url(original_params.url);
  std::string orig_mime_type = original_params.mimeType.utf8();
  ChromeViewHostMsg_GetPluginInfo_Status status;
  webkit::WebPluginInfo plugin;
  std::string actual_mime_type;
  render_view->Send(new ChromeViewHostMsg_GetPluginInfo(
      render_view->GetRoutingId(), url, frame->top()->document().url(),
      orig_mime_type, &status, &plugin, &actual_mime_type));

  if (status.value == ChromeViewHostMsg_GetPluginInfo_Status::kNotFound) {
    MissingPluginReporter::GetInstance()->ReportPluginMissing(
        orig_mime_type, url);
    return MissingPlugin::Create(render_view, frame, original_params);
  }
  if (plugin.path.value() == webkit::npapi::kDefaultPluginLibraryName) {
    MissingPluginReporter::GetInstance()->ReportPluginMissing(
        orig_mime_type, url);
  }

  scoped_ptr<webkit::npapi::PluginGroup> group(
      webkit::npapi::PluginList::Singleton()->GetPluginGroup(plugin));

  if (status.value == ChromeViewHostMsg_GetPluginInfo_Status::kDisabled) {
    return BlockedPlugin::Create(
        render_view, frame, original_params, plugin, group.get(),
        IDR_DISABLED_PLUGIN_HTML, IDS_PLUGIN_DISABLED, false, false);
  }

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

  if (status.value ==
      ChromeViewHostMsg_GetPluginInfo_Status::kOutdatedBlocked) {
    render_view->Send(new ChromeViewHostMsg_BlockedOutdatedPlugin(
        render_view->GetRoutingId(), group->GetGroupName(),
        GURL(group->GetUpdateURL())));
  }
  if (status.value ==
          ChromeViewHostMsg_GetPluginInfo_Status::kOutdatedBlocked ||
      status.value ==
          ChromeViewHostMsg_GetPluginInfo_Status::kOutdatedDisallowed) {
    return BlockedPlugin::Create(
        render_view, frame, params, plugin, group.get(),
        IDR_BLOCKED_PLUGIN_HTML, IDS_PLUGIN_OUTDATED, false,
        status.value ==
            ChromeViewHostMsg_GetPluginInfo_Status::kOutdatedBlocked);
  }

  ContentSettingsObserver* observer = ContentSettingsObserver::Get(render_view);

  if (status.value ==
      ChromeViewHostMsg_GetPluginInfo_Status::kUnauthorized &&
      !observer->plugins_temporarily_allowed()) {
    render_view->Send(new ChromeViewHostMsg_BlockedOutdatedPlugin(
        render_view->GetRoutingId(), group->GetGroupName(), GURL()));
    return BlockedPlugin::Create(
        render_view, frame, params, plugin, group.get(),
        IDR_BLOCKED_PLUGIN_HTML, IDS_PLUGIN_NOT_AUTHORIZED, false, true);
  }

  bool is_nacl_plugin = (plugin.name == ASCIIToUTF16(
      chrome::ChromeContentClient::kNaClPluginName));
  if (status.value == ChromeViewHostMsg_GetPluginInfo_Status::kAllowed ||
      observer->plugins_temporarily_allowed() ||
      plugin.path.value() == webkit::npapi::kDefaultPluginLibraryName) {
    // Delay loading plugins if prerendering.
    if (prerender::PrerenderHelper::IsPrerendering(render_view)) {
      return BlockedPlugin::Create(
          render_view, frame, params, plugin, group.get(),
          IDR_CLICK_TO_PLAY_PLUGIN_HTML, IDS_PLUGIN_LOAD, true, true);
    }

    // Determine if NaCl is allowed for both the internal plugin and
    // any external plugin that handles our MIME type. This is so NaCl
    // tests will still pass.
    const char* kNaClMimeType = "application/x-nacl";
    bool is_nacl_mime_type = actual_mime_type == kNaClMimeType;
    bool is_nacl_enabled;
    if (is_nacl_plugin) {
      is_nacl_enabled = cmd->HasSwitch(switches::kEnableNaCl);
    } else {
      // If this is an external plugin that handles NaCl mime type,
      // we want to allow Native Client, because it's how
      // NaCl tests for the plugin work.
      is_nacl_enabled = true;
    }
    if (is_nacl_plugin || is_nacl_mime_type) {
      if (!IsNaClAllowed(plugin,
                         url,
                         actual_mime_type,
                         is_nacl_mime_type,
                         is_nacl_enabled,
                         params)) {
        return BlockedPlugin::Create(
            render_view, frame, params, plugin, group.get(),
            IDR_BLOCKED_PLUGIN_HTML, IDS_PLUGIN_BLOCKED, false, false);
      }
    }

    return render_view->CreatePlugin(frame, plugin, params);
  }

  observer->DidBlockContentType(
      is_nacl_plugin ? CONTENT_SETTINGS_TYPE_JAVASCRIPT :
                       CONTENT_SETTINGS_TYPE_PLUGINS,
      group->identifier());
  if (status.value == ChromeViewHostMsg_GetPluginInfo_Status::kClickToPlay) {
    RenderThread::Get()->RecordUserMetrics("Plugin_ClickToPlay");
    return BlockedPlugin::Create(
        render_view, frame, params, plugin, group.get(),
        IDR_CLICK_TO_PLAY_PLUGIN_HTML, IDS_PLUGIN_LOAD, false, true);
  } else {
    DCHECK(status.value == ChromeViewHostMsg_GetPluginInfo_Status::kBlocked);
    RenderThread::Get()->RecordUserMetrics("Plugin_Blocked");
    return BlockedPlugin::Create(
        render_view, frame, params, plugin, group.get(),
        IDR_BLOCKED_PLUGIN_HTML, IDS_PLUGIN_BLOCKED, false, true);
  }
}

bool ChromeContentRendererClient::IsNaClAllowed(
    const webkit::WebPluginInfo& plugin,
    const GURL& url,
    const std::string& actual_mime_type,
    bool is_nacl_mime_type,
    bool enable_nacl,
    WebKit::WebPluginParams& params) {
  GURL manifest_url;
  if (is_nacl_mime_type) {
    manifest_url = url;  // Normal embedded NaCl plugin.
  } else {
    // This is a content type handling NaCl plugin; look for the .nexe URL
    // among the MIME type's additonal parameters.
    const char* kNaClPluginManifestAttribute = "nacl";
    string16 nacl_attr = ASCIIToUTF16(kNaClPluginManifestAttribute);
    for (size_t i = 0; i < plugin.mime_types.size(); ++i) {
      if (plugin.mime_types[i].mime_type == actual_mime_type) {
        const webkit::WebPluginMimeType& content_type =
            plugin.mime_types[i];
        for (size_t i = 0;
            i < content_type.additional_param_names.size(); ++i) {
          if (content_type.additional_param_names[i] == nacl_attr) {
            manifest_url = GURL(content_type.additional_param_values[i]);
            break;
          }
        }
        break;
      }
    }
  }

  // Determine if the manifest URL is part of an extension.
  const Extension* extension =
      extension_dispatcher_->extensions()->GetByURL(manifest_url);
  // Only component, unpacked, and Chrome Web Store extensions are allowed.
  bool allowed_extension = extension &&
      (extension->from_webstore() ||
      extension->location() == Extension::COMPONENT ||
      extension->location() == Extension::LOAD);

  // Block any other use of NaCl plugin, unless --enable-nacl is set.
  if (!allowed_extension && !enable_nacl)
    return false;

  // Allow dev interfaces for non-extension apps.
  bool allow_dev_interfaces = true;
  if (allowed_extension) {
    // Allow dev interfaces for component and unpacked extensions.
    if (extension->location() != Extension::COMPONENT &&
        extension->location() != Extension::LOAD) {
      // Whitelist all other allowed extensions.
      allow_dev_interfaces =
          // PDF Viewer plugin
          (manifest_url.scheme() == "chrome-extension" &&
          manifest_url.host() == "acadkphlmlegjaadjagenfimbpphcgnh");
    }
  }

  WebString dev_attribute = WebString::fromUTF8("@dev");
  if (allow_dev_interfaces) {
    std::vector<string16> param_names;
    std::vector<string16> param_values;
    param_names.push_back(dev_attribute);
    param_values.push_back(WebString());
    AppendParams(
        param_names,
        param_values,
        &params.attributeNames,
        &params.attributeValues);
  } else {
    // If the params somehow contain this special attribute, remove it.
    size_t attribute_count = params.attributeNames.size();
    for (size_t i = 0; i < attribute_count; ++i) {
      if (params.attributeNames[i].equals(dev_attribute))
        params.attributeNames[i] = WebString();
    }
  }

  return true;
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

  if (failed_url.is_valid() && !failed_url.SchemeIs(chrome::kExtensionScheme))
    extension = extension_dispatcher_->extensions()->GetByURL(failed_url);

  if (error_html) {
    // Use a local error page.
    int resource_id;
    DictionaryValue error_strings;
    if (extension) {
      LocalizedError::GetAppErrorStrings(error, failed_url, extension,
                                         &error_strings);

      // TODO(erikkay): Should we use a different template for different
      // error messages?
      resource_id = IDR_ERROR_APP_HTML;
    } else {
      if (is_repost) {
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
      *error_html = jstemplate_builder::GetTemplatesHtml(
          template_html, &error_strings, "t");
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

bool ChromeContentRendererClient::AllowPopup(const GURL& creator) {
  ChromeV8Context* current_context =
      extension_dispatcher_->v8_context_set().GetCurrent();
  return current_context && !current_context->extension_id().empty();
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
  extension_dispatcher_->DidCreateScriptContext(frame, context, world_id);
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
    msg->EnableMessagePumping();
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
    ExtensionDispatcher* extension_dispatcher) {
  extension_dispatcher_.reset(extension_dispatcher);
}

const Extension* ChromeContentRendererClient::GetNonBookmarkAppExtension(
    const ExtensionSet* extensions, const GURL& url) {
  // Exclude bookmark apps, which do not use the app process model.
  const Extension* extension = extensions->GetByURL(url);
  if (extension && extension->from_bookmark())
    extension = NULL;
  return extension;
}

bool ChromeContentRendererClient::CrossesExtensionExtents(
    WebFrame* frame,
    const GURL& new_url,
    bool is_initial_navigation) {
  const ExtensionSet* extensions = extension_dispatcher_->extensions();
  GURL old_url(frame->top()->document().url());

  // Determine if the new URL is an extension (excluding bookmark apps).
  const Extension* new_url_extension = GetNonBookmarkAppExtension(extensions,
                                                                  new_url);

  // If old_url is still empty and this is an initial navigation, then this is
  // a window.open operation.  We should look at the opener URL.
  if (is_initial_navigation && old_url.is_empty() && frame->opener()) {
    // If we're about to open a normal web page from a same-origin opener stuck
    // in an extension process, we want to keep it in process to allow the
    // opener to script it.
    GURL opener_url = frame->opener()->document().url();
    bool opener_is_extension_url = !!extensions->GetByURL(opener_url);
    WebSecurityOrigin opener = frame->opener()->document().securityOrigin();
    if (!new_url_extension &&
        !opener_is_extension_url &&
        extension_dispatcher_->is_extension_process() &&
        opener.canRequest(WebURL(new_url)))
      return false;

    // In all other cases, we want to compare against the top frame's URL (as
    // opposed to the opener frame's), since that's what determines the type of
    // process.  This allows iframes outside an app to open a popup in the app.
    old_url = frame->top()->opener()->top()->document().url();
  }

  // Determine if the old URL is an extension (excluding bookmark apps).
  const Extension* old_url_extension = GetNonBookmarkAppExtension(extensions,
                                                                  old_url);

  // TODO(creis): Temporary workaround for crbug.com/59285: Only return true if
  // we would enter an extension app's extent from a non-app, or if we leave an
  // extension with no web extent.  We avoid swapping processes to exit a hosted
  // app for now, since we do not yet support postMessage calls from outside the
  // app back into it (e.g., as in Facebook OAuth 2.0).
  bool old_url_is_hosted_app = old_url_extension &&
      !old_url_extension->web_extent().is_empty();
  if (old_url_is_hosted_app)
    return false;

  return old_url_extension != new_url_extension;
}

void ChromeContentRendererClient::OnPurgeMemory() {
  DVLOG(1) << "Resetting spellcheck in renderer client";
  RenderThread* thread = RenderThread::Get();
  if (spellcheck_.get())
    thread->RemoveObserver(spellcheck_.get());
  SpellCheck* new_spellcheck = new SpellCheck();
  if (spellcheck_provider_)
    spellcheck_provider_->SetSpellCheck(new_spellcheck);
  spellcheck_.reset(new_spellcheck);
  thread->AddObserver(new_spellcheck);
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
  factory_manager->RegisterFactory(ChromePPAPIInterfaceFactory);
}

}  // namespace chrome
