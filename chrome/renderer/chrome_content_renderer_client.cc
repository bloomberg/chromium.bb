// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_content_renderer_client.h"

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/values.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/autofill/autofill_agent.h"
#include "chrome/renderer/autofill/form_manager.h"
#include "chrome/renderer/autofill/password_autofill_manager.h"
#include "chrome/renderer/blocked_plugin.h"
#include "chrome/renderer/devtools_agent.h"
#include "chrome/renderer/extensions/bindings_utils.h"
#include "chrome/renderer/extensions/event_bindings.h"
#include "chrome/renderer/extensions/extension_dispatcher.h"
#include "chrome/renderer/extensions/extension_helper.h"
#include "chrome/renderer/extensions/extension_process_bindings.h"
#include "chrome/renderer/extensions/extension_resource_request_policy.h"
#include "chrome/renderer/extensions/renderer_extension_bindings.h"
#include "chrome/renderer/localized_error.h"
#include "chrome/renderer/page_click_tracker.h"
#include "chrome/renderer/safe_browsing/malware_dom_details.h"
#include "chrome/renderer/safe_browsing/phishing_classifier_delegate.h"
#include "chrome/renderer/translate_helper.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_view.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/renderer_resources.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginParams.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/cld/encodings/compact_lang_det/win/cld_unicodetext.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/ppapi/plugin_module.h"

using autofill::AutofillAgent;
using autofill::FormManager;
using autofill::PasswordAutofillManager;
using WebKit::WebFrame;
using WebKit::WebPlugin;
using WebKit::WebPluginParams;
using WebKit::WebString;
using WebKit::WebURLError;
using WebKit::WebURLRequest;
using WebKit::WebURLResponse;

namespace {

// Returns true if the frame is navigating to an URL either into or out of an
// extension app's extent.
// TODO(creis): Temporary workaround for crbug.com/65953: Only return true if
// we would enter an extension app's extent from a non-app, or if we leave an
// extension with no web extent.  We avoid swapping processes to exit a hosted
// app with a web extent for now, since we do not yet restore context (such
// as window.opener) if the window navigates back.
static bool CrossesExtensionExtents(WebFrame* frame, const GURL& new_url) {
  const ExtensionSet* extensions = ExtensionDispatcher::Get()->extensions();
  // If the URL is still empty, this is a window.open navigation. Check the
  // opener's URL.
  GURL old_url(frame->url());
  if (old_url.is_empty() && frame->opener())
    old_url = frame->opener()->url();

  bool old_url_is_hosted_app = extensions->GetByURL(old_url) &&
      !extensions->GetByURL(old_url)->web_extent().is_empty();
  return !extensions->InSameExtent(old_url, new_url) &&
         !old_url_is_hosted_app;
}

}  // namespcae

namespace chrome {

void ChromeContentRendererClient::RenderViewCreated(RenderView* render_view) {
  new DevToolsAgent(render_view);

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

  new TranslateHelper(render_view);

#ifndef OS_CHROMEOS
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableClientSidePhishingDetection)) {
    new safe_browsing::PhishingClassifierDelegate(render_view, NULL);
  }
#endif

  // Observer for Malware DOM details messages.
  new safe_browsing::MalwareDOMDetails(render_view);

  new ExtensionHelper(render_view);
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
      const WebPluginParams& params) {
  bool found = false;
  int plugin_setting = CONTENT_SETTING_DEFAULT;
  CommandLine* cmd = CommandLine::ForCurrentProcess();
  webkit::npapi::WebPluginInfo info;
  GURL url(params.url);
  std::string actual_mime_type;
  render_view->Send(new ViewHostMsg_GetPluginInfo(
      render_view->routing_id(), url, frame->top()->url(),
      params.mimeType.utf8(), &found, &info, &plugin_setting,
      &actual_mime_type));

  if (!found)
    return NULL;
  DCHECK(plugin_setting != CONTENT_SETTING_DEFAULT);
  if (!webkit::npapi::IsPluginEnabled(info))
    return NULL;

  const webkit::npapi::PluginGroup* group =
      webkit::npapi::PluginList::Singleton()->GetPluginGroup(info);
  DCHECK(group != NULL);

  if (group->IsVulnerable()) {
    // The outdated plugins policy isn't cached here because
    // it's a dynamic policy that can be modified at runtime.
    ContentSetting outdated_plugins_policy;
    render_view->Send(new ViewHostMsg_GetOutdatedPluginsPolicy(
        render_view->routing_id(), &outdated_plugins_policy));

    if (outdated_plugins_policy == CONTENT_SETTING_ASK ||
        outdated_plugins_policy == CONTENT_SETTING_BLOCK) {
      if (outdated_plugins_policy == CONTENT_SETTING_ASK) {
        render_view->Send(new ViewHostMsg_BlockedOutdatedPlugin(
            render_view->routing_id(), group->GetGroupName(),
            GURL(group->GetUpdateURL())));
      }
      return CreatePluginPlaceholder(
          render_view, frame, params, *group, IDR_BLOCKED_PLUGIN_HTML,
          IDS_PLUGIN_OUTDATED, false,
          outdated_plugins_policy == CONTENT_SETTING_ASK);
    } else {
      DCHECK(outdated_plugins_policy == CONTENT_SETTING_ALLOW);
    }
  }

  ContentSetting host_setting = render_view->current_content_settings_.
      settings[CONTENT_SETTINGS_TYPE_PLUGINS];

  if (group->RequiresAuthorization() &&
      !cmd->HasSwitch(switches::kAlwaysAuthorizePlugins) &&
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
    if (render_view->is_prerendering_) {
      return CreatePluginPlaceholder(
          render_view, frame, params, *group, IDR_CLICK_TO_PLAY_PLUGIN_HTML,
          IDS_PLUGIN_LOAD, true, true);
    }

    scoped_refptr<webkit::ppapi::PluginModule> pepper_module(
        render_view->pepper_delegate_.CreatePepperPlugin(info.path));
    if (pepper_module) {
      return render_view->CreatePepperPlugin(
          frame, params, info.path, pepper_module.get());
    }

    return render_view->CreateNPAPIPlugin(
        frame, params, info.path, actual_mime_type);
  }

  std::string resource;
  if (cmd->HasSwitch(switches::kEnableResourceContentSettings))
    resource = group->identifier();
  render_view->DidBlockContentType(CONTENT_SETTINGS_TYPE_PLUGINS, resource);
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
    extension = ExtensionDispatcher::Get()->extensions()->GetByURL(failed_url);
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

// Returns the ISO 639_1 language code of the specified |text|, or 'unknown'
// if it failed.
std::string ChromeContentRendererClient::DetermineTextLanguage(
    const string16& text) {
  std::string language = chrome::kUnknownLanguageCode;
  int num_languages = 0;
  int text_bytes = 0;
  bool is_reliable = false;
  Language cld_language =
      DetectLanguageOfUnicodeText(NULL, text.c_str(), true, &is_reliable,
                                  &num_languages, NULL, &text_bytes);
  // We don't trust the result if the CLD reports that the detection is not
  // reliable, or if the actual text used to detect the language was less than
  // 100 bytes (short texts can often lead to wrong results).
  if (is_reliable && text_bytes >= 100 && cld_language != NUM_LANGUAGES &&
      cld_language != UNKNOWN_LANGUAGE && cld_language != TG_UNKNOWN_LANGUAGE) {
    // We should not use LanguageCode_ISO_639_1 because it does not cover all
    // the languages CLD can detect. As a result, it'll return the invalid
    // language code for tradtional Chinese among others.
    // |LanguageCodeWithDialect| will go through ISO 639-1, ISO-639-2 and
    // 'other' tables to do the 'right' thing. In addition, it'll return zh-CN
    // for Simplified Chinese.
    language = LanguageCodeWithDialects(cld_language);
  }
  return language;
}

bool ChromeContentRendererClient::RunIdleHandlerWhenWidgetsHidden() {
  return !ExtensionDispatcher::Get()->is_extension_process();
}

bool ChromeContentRendererClient::AllowPopup(const GURL& creator) {
  // Extensions and apps always allowed to create unrequested popups. The second
  // check is necessary to include content scripts.
  return ExtensionDispatcher::Get()->extensions()->GetByURL(creator) ||
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
  // TODO(creis): For hosted apps, we currently only swap processes to enter
  // the app and not exit it, since we currently lose context (e.g.,
  // window.opener) if the window navigates back.  See crbug.com/65953.
  if (!CrossesExtensionExtents(frame, url))
    return false;

  // Include the referrer in this case since we're going from a hosted web
  // page. (the packaged case is handled previously by the extension
  // navigation test)
  *send_referrer = true;

  if (is_content_initiated) {
    const Extension* extension =
        ExtensionDispatcher::Get()->extensions()->GetByURL(url);
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
          url, GURL(frame->url()), ExtensionDispatcher::Get()->extensions())) {
    *new_url = GURL("chrome-extension://invalid/");
    return true;
  }

  return false;
}

void ChromeContentRendererClient::DidCreateScriptContext(WebFrame* frame) {
  EventBindings::HandleContextCreated(frame, false);
}

void ChromeContentRendererClient::DidDestroyScriptContext(WebFrame* frame) {
  EventBindings::HandleContextDestroyed(frame);
}

void ChromeContentRendererClient::DidCreateIsolatedScriptContext(
  WebFrame* frame) {
  EventBindings::HandleContextCreated(frame, true);
}

}  // namespace chrome
