// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/content_settings_observer.h"

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/navigation_state.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/public/platform/WebPermissionCallbacks.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebFrameClient.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebView.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/common/extensions/chrome_extension_messages.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/renderer/dispatcher.h"
#endif

using blink::WebDataSource;
using blink::WebDocument;
using blink::WebFrame;
using blink::WebPermissionCallbacks;
using blink::WebSecurityOrigin;
using blink::WebString;
using blink::WebURL;
using blink::WebView;
using content::DocumentState;
using content::NavigationState;

namespace {

enum {
  INSECURE_CONTENT_DISPLAY = 0,
  INSECURE_CONTENT_DISPLAY_HOST_GOOGLE,
  INSECURE_CONTENT_DISPLAY_HOST_WWW_GOOGLE,
  INSECURE_CONTENT_DISPLAY_HTML,
  INSECURE_CONTENT_RUN,
  INSECURE_CONTENT_RUN_HOST_GOOGLE,
  INSECURE_CONTENT_RUN_HOST_WWW_GOOGLE,
  INSECURE_CONTENT_RUN_TARGET_YOUTUBE,
  INSECURE_CONTENT_RUN_JS,
  INSECURE_CONTENT_RUN_CSS,
  INSECURE_CONTENT_RUN_SWF,
  INSECURE_CONTENT_DISPLAY_HOST_YOUTUBE,
  INSECURE_CONTENT_RUN_HOST_YOUTUBE,
  INSECURE_CONTENT_RUN_HOST_GOOGLEUSERCONTENT,
  INSECURE_CONTENT_DISPLAY_HOST_MAIL_GOOGLE,
  INSECURE_CONTENT_RUN_HOST_MAIL_GOOGLE,
  INSECURE_CONTENT_DISPLAY_HOST_PLUS_GOOGLE,
  INSECURE_CONTENT_RUN_HOST_PLUS_GOOGLE,
  INSECURE_CONTENT_DISPLAY_HOST_DOCS_GOOGLE,
  INSECURE_CONTENT_RUN_HOST_DOCS_GOOGLE,
  INSECURE_CONTENT_DISPLAY_HOST_SITES_GOOGLE,
  INSECURE_CONTENT_RUN_HOST_SITES_GOOGLE,
  INSECURE_CONTENT_DISPLAY_HOST_PICASAWEB_GOOGLE,
  INSECURE_CONTENT_RUN_HOST_PICASAWEB_GOOGLE,
  INSECURE_CONTENT_DISPLAY_HOST_GOOGLE_READER,
  INSECURE_CONTENT_RUN_HOST_GOOGLE_READER,
  INSECURE_CONTENT_DISPLAY_HOST_CODE_GOOGLE,
  INSECURE_CONTENT_RUN_HOST_CODE_GOOGLE,
  INSECURE_CONTENT_DISPLAY_HOST_GROUPS_GOOGLE,
  INSECURE_CONTENT_RUN_HOST_GROUPS_GOOGLE,
  INSECURE_CONTENT_DISPLAY_HOST_MAPS_GOOGLE,
  INSECURE_CONTENT_RUN_HOST_MAPS_GOOGLE,
  INSECURE_CONTENT_DISPLAY_HOST_GOOGLE_SUPPORT,
  INSECURE_CONTENT_RUN_HOST_GOOGLE_SUPPORT,
  INSECURE_CONTENT_DISPLAY_HOST_GOOGLE_INTL,
  INSECURE_CONTENT_RUN_HOST_GOOGLE_INTL,
  INSECURE_CONTENT_NUM_EVENTS
};

// Constants for UMA statistic collection.
static const char kWWWDotGoogleDotCom[] = "www.google.com";
static const char kMailDotGoogleDotCom[] = "mail.google.com";
static const char kPlusDotGoogleDotCom[] = "plus.google.com";
static const char kDocsDotGoogleDotCom[] = "docs.google.com";
static const char kSitesDotGoogleDotCom[] = "sites.google.com";
static const char kPicasawebDotGoogleDotCom[] = "picasaweb.google.com";
static const char kCodeDotGoogleDotCom[] = "code.google.com";
static const char kGroupsDotGoogleDotCom[] = "groups.google.com";
static const char kMapsDotGoogleDotCom[] = "maps.google.com";
static const char kWWWDotYoutubeDotCom[] = "www.youtube.com";
static const char kDotGoogleUserContentDotCom[] = ".googleusercontent.com";
static const char kGoogleReaderPathPrefix[] = "/reader/";
static const char kGoogleSupportPathPrefix[] = "/support/";
static const char kGoogleIntlPathPrefix[] = "/intl/";
static const char kDotJS[] = ".js";
static const char kDotCSS[] = ".css";
static const char kDotSWF[] = ".swf";
static const char kDotHTML[] = ".html";

// Constants for mixed-content blocking.
static const char kGoogleDotCom[] = "google.com";

static bool IsHostInDomain(const std::string& host, const std::string& domain) {
  return (EndsWith(host, domain, false) &&
          (host.length() == domain.length() ||
           (host.length() > domain.length() &&
            host[host.length() - domain.length() - 1] == '.')));
}

GURL GetOriginOrURL(const WebFrame* frame) {
  WebString top_origin = frame->top()->document().securityOrigin().toString();
  // The the |top_origin| is unique ("null") e.g., for file:// URLs. Use the
  // document URL as the primary URL in those cases.
  if (top_origin == "null")
    return frame->top()->document().url();
  return GURL(top_origin);
}

ContentSetting GetContentSettingFromRules(
    const ContentSettingsForOneType& rules,
    const WebFrame* frame,
    const GURL& secondary_url) {
  ContentSettingsForOneType::const_iterator it;
  // If there is only one rule, it's the default rule and we don't need to match
  // the patterns.
  if (rules.size() == 1) {
    DCHECK(rules[0].primary_pattern == ContentSettingsPattern::Wildcard());
    DCHECK(rules[0].secondary_pattern == ContentSettingsPattern::Wildcard());
    return rules[0].setting;
  }
  const GURL& primary_url = GetOriginOrURL(frame);
  for (it = rules.begin(); it != rules.end(); ++it) {
    if (it->primary_pattern.Matches(primary_url) &&
        it->secondary_pattern.Matches(secondary_url)) {
      return it->setting;
    }
  }
  NOTREACHED();
  return CONTENT_SETTING_DEFAULT;
}

}  // namespace

ContentSettingsObserver::ContentSettingsObserver(
    content::RenderFrame* render_frame,
    extensions::Dispatcher* extension_dispatcher)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<ContentSettingsObserver>(
          render_frame),
#if defined(ENABLE_EXTENSIONS)
      extension_dispatcher_(extension_dispatcher),
#endif
      allow_displaying_insecure_content_(false),
      allow_running_insecure_content_(false),
      content_setting_rules_(NULL),
      is_interstitial_page_(false),
      npapi_plugins_blocked_(false),
      current_request_id_(0) {
  ClearBlockedContentSettings();
  render_frame->GetWebFrame()->setPermissionClient(this);

  if (render_frame->GetRenderView()->GetMainRenderFrame() != render_frame) {
    // Copy all the settings from the main render frame to avoid race conditions
    // when initializing this data. See http://crbug.com/333308.
    ContentSettingsObserver* parent = ContentSettingsObserver::Get(
        render_frame->GetRenderView()->GetMainRenderFrame());
    allow_displaying_insecure_content_ =
        parent->allow_displaying_insecure_content_;
    allow_running_insecure_content_ = parent->allow_running_insecure_content_;
    temporarily_allowed_plugins_ = parent->temporarily_allowed_plugins_;
    is_interstitial_page_ = parent->is_interstitial_page_;
    npapi_plugins_blocked_ = parent->npapi_plugins_blocked_;
  }
}

ContentSettingsObserver::~ContentSettingsObserver() {
}

void ContentSettingsObserver::SetContentSettingRules(
    const RendererContentSettingRules* content_setting_rules) {
  content_setting_rules_ = content_setting_rules;
}

bool ContentSettingsObserver::IsPluginTemporarilyAllowed(
    const std::string& identifier) {
  // If the empty string is in here, it means all plug-ins are allowed.
  // TODO(bauerb): Remove this once we only pass in explicit identifiers.
  return (temporarily_allowed_plugins_.find(identifier) !=
          temporarily_allowed_plugins_.end()) ||
         (temporarily_allowed_plugins_.find(std::string()) !=
          temporarily_allowed_plugins_.end());
}

void ContentSettingsObserver::DidBlockContentType(
    ContentSettingsType settings_type) {
  if (!content_blocked_[settings_type]) {
    content_blocked_[settings_type] = true;
    Send(new ChromeViewHostMsg_ContentBlocked(routing_id(), settings_type));
  }
}

bool ContentSettingsObserver::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ContentSettingsObserver, message)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetAsInterstitial, OnSetAsInterstitial)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_NPAPINotSupported, OnNPAPINotSupported)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetAllowDisplayingInsecureContent,
                        OnSetAllowDisplayingInsecureContent)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetAllowRunningInsecureContent,
                        OnSetAllowRunningInsecureContent)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_ReloadFrame, OnReloadFrame);
    IPC_MESSAGE_HANDLER(ChromeViewMsg_RequestFileSystemAccessAsyncResponse,
                        OnRequestFileSystemAccessAsyncResponse)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  if (handled)
    return true;

  // Don't swallow LoadBlockedPlugins messages, as they're sent to every
  // blocked plugin.
  IPC_BEGIN_MESSAGE_MAP(ContentSettingsObserver, message)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_LoadBlockedPlugins, OnLoadBlockedPlugins)
  IPC_END_MESSAGE_MAP()

  return false;
}

void ContentSettingsObserver::DidCommitProvisionalLoad(bool is_new_navigation) {
  WebFrame* frame = render_frame()->GetWebFrame();
  if (frame->parent())
    return;  // Not a top-level navigation.

  DocumentState* document_state = DocumentState::FromDataSource(
      frame->dataSource());
  NavigationState* navigation_state = document_state->navigation_state();
  if (!navigation_state->was_within_same_page()) {
    // Clear "block" flags for the new page. This needs to happen before any of
    // |allowScript()|, |allowScriptFromSource()|, |allowImage()|, or
    // |allowPlugins()| is called for the new page so that these functions can
    // correctly detect that a piece of content flipped from "not blocked" to
    // "blocked".
    ClearBlockedContentSettings();
    temporarily_allowed_plugins_.clear();
  }

  GURL url = frame->document().url();
  // If we start failing this DCHECK, please makes sure we don't regress
  // this bug: http://code.google.com/p/chromium/issues/detail?id=79304
  DCHECK(frame->document().securityOrigin().toString() == "null" ||
         !url.SchemeIs(url::kDataScheme));
}

bool ContentSettingsObserver::allowDatabase(const WebString& name,
                                            const WebString& display_name,
                                            unsigned long estimated_size) {
  WebFrame* frame = render_frame()->GetWebFrame();
  if (frame->document().securityOrigin().isUnique() ||
      frame->top()->document().securityOrigin().isUnique())
    return false;

  bool result = false;
  Send(new ChromeViewHostMsg_AllowDatabase(
      routing_id(), GURL(frame->document().securityOrigin().toString()),
      GURL(frame->top()->document().securityOrigin().toString()),
      name, display_name, &result));
  return result;
}

void ContentSettingsObserver::requestFileSystemAccessAsync(
    const WebPermissionCallbacks& callbacks) {
  WebFrame* frame = render_frame()->GetWebFrame();
  if (frame->document().securityOrigin().isUnique() ||
      frame->top()->document().securityOrigin().isUnique()) {
    WebPermissionCallbacks permissionCallbacks(callbacks);
    permissionCallbacks.doDeny();
    return;
  }
  ++current_request_id_;
  std::pair<PermissionRequestMap::iterator, bool> insert_result =
      permission_requests_.insert(
          std::make_pair(current_request_id_, callbacks));

  // Verify there are no duplicate insertions.
  DCHECK(insert_result.second);

  Send(new ChromeViewHostMsg_RequestFileSystemAccessAsync(
      routing_id(),
      current_request_id_,
      GURL(frame->document().securityOrigin().toString()),
      GURL(frame->top()->document().securityOrigin().toString())));
}

bool ContentSettingsObserver::allowImage(bool enabled_per_settings,
                                         const WebURL& image_url) {
  bool allow = enabled_per_settings;
  if (enabled_per_settings) {
    if (is_interstitial_page_)
      return true;

    if (IsWhitelistedForContentSettings(render_frame()))
      return true;

    if (content_setting_rules_) {
      GURL secondary_url(image_url);
      allow =
          GetContentSettingFromRules(content_setting_rules_->image_rules,
                                     render_frame()->GetWebFrame(),
                                     secondary_url) != CONTENT_SETTING_BLOCK;
    }
  }
  if (!allow)
    DidBlockContentType(CONTENT_SETTINGS_TYPE_IMAGES);
  return allow;
}

bool ContentSettingsObserver::allowIndexedDB(const WebString& name,
                                             const WebSecurityOrigin& origin) {
  WebFrame* frame = render_frame()->GetWebFrame();
  if (frame->document().securityOrigin().isUnique() ||
      frame->top()->document().securityOrigin().isUnique())
    return false;

  bool result = false;
  Send(new ChromeViewHostMsg_AllowIndexedDB(
      routing_id(), GURL(frame->document().securityOrigin().toString()),
      GURL(frame->top()->document().securityOrigin().toString()),
      name, &result));
  return result;
}

bool ContentSettingsObserver::allowPlugins(bool enabled_per_settings) {
  return enabled_per_settings;
}

bool ContentSettingsObserver::allowScript(bool enabled_per_settings) {
  if (!enabled_per_settings)
    return false;
  if (is_interstitial_page_)
    return true;

  WebFrame* frame = render_frame()->GetWebFrame();
  std::map<WebFrame*, bool>::const_iterator it =
      cached_script_permissions_.find(frame);
  if (it != cached_script_permissions_.end())
    return it->second;

  // Evaluate the content setting rules before
  // |IsWhitelistedForContentSettings|; if there is only the default rule
  // allowing all scripts, it's quicker this way.
  bool allow = true;
  if (content_setting_rules_) {
    ContentSetting setting = GetContentSettingFromRules(
        content_setting_rules_->script_rules,
        frame,
        GURL(frame->document().securityOrigin().toString()));
    allow = setting != CONTENT_SETTING_BLOCK;
  }
  allow = allow || IsWhitelistedForContentSettings(render_frame());

  cached_script_permissions_[frame] = allow;
  return allow;
}

bool ContentSettingsObserver::allowScriptFromSource(
    bool enabled_per_settings,
    const blink::WebURL& script_url) {
  if (!enabled_per_settings)
    return false;
  if (is_interstitial_page_)
    return true;

  bool allow = true;
  if (content_setting_rules_) {
    ContentSetting setting =
        GetContentSettingFromRules(content_setting_rules_->script_rules,
                                   render_frame()->GetWebFrame(),
                                   GURL(script_url));
    allow = setting != CONTENT_SETTING_BLOCK;
  }
  return allow || IsWhitelistedForContentSettings(render_frame());
}

bool ContentSettingsObserver::allowStorage(bool local) {
  WebFrame* frame = render_frame()->GetWebFrame();
  if (frame->document().securityOrigin().isUnique() ||
      frame->top()->document().securityOrigin().isUnique())
    return false;
  bool result = false;

  StoragePermissionsKey key(
      GURL(frame->document().securityOrigin().toString()), local);
  std::map<StoragePermissionsKey, bool>::const_iterator permissions =
      cached_storage_permissions_.find(key);
  if (permissions != cached_storage_permissions_.end())
    return permissions->second;

  Send(new ChromeViewHostMsg_AllowDOMStorage(
      routing_id(), GURL(frame->document().securityOrigin().toString()),
      GURL(frame->top()->document().securityOrigin().toString()),
      local, &result));
  cached_storage_permissions_[key] = result;
  return result;
}

bool ContentSettingsObserver::allowReadFromClipboard(bool default_value) {
  bool allowed = false;
#if defined(ENABLE_EXTENSIONS)
  WebFrame* frame = render_frame()->GetWebFrame();
  // TODO(dcheng): Should we consider a toURL() method on WebSecurityOrigin?
  Send(new ChromeViewHostMsg_CanTriggerClipboardRead(
      GURL(frame->document().securityOrigin().toString()), &allowed));
#endif
  return allowed;
}

bool ContentSettingsObserver::allowWriteToClipboard(bool default_value) {
  bool allowed = false;
#if defined(ENABLE_EXTENSIONS)
  WebFrame* frame = render_frame()->GetWebFrame();
  Send(new ChromeViewHostMsg_CanTriggerClipboardWrite(
      GURL(frame->document().securityOrigin().toString()), &allowed));
#endif
  return allowed;
}

bool ContentSettingsObserver::allowMutationEvents(bool default_value) {
  return IsPlatformApp() ? false : default_value;
}

bool ContentSettingsObserver::allowPushState() {
  return !IsPlatformApp();
}

static void SendInsecureContentSignal(int signal) {
  UMA_HISTOGRAM_ENUMERATION("SSL.InsecureContent", signal,
                            INSECURE_CONTENT_NUM_EVENTS);
}

bool ContentSettingsObserver::allowDisplayingInsecureContent(
    bool allowed_per_settings,
    const blink::WebSecurityOrigin& origin,
    const blink::WebURL& resource_url) {
  SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY);

  std::string origin_host(origin.host().utf8());
  WebFrame* frame = render_frame()->GetWebFrame();
  GURL frame_gurl(frame->document().url());
  if (IsHostInDomain(origin_host, kGoogleDotCom)) {
    SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_GOOGLE);
    if (StartsWithASCII(frame_gurl.path(), kGoogleSupportPathPrefix, false)) {
      SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_GOOGLE_SUPPORT);
    } else if (StartsWithASCII(frame_gurl.path(),
                               kGoogleIntlPathPrefix,
                               false)) {
      SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_GOOGLE_INTL);
    }
  }

  if (origin_host == kWWWDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_WWW_GOOGLE);
    if (StartsWithASCII(frame_gurl.path(), kGoogleReaderPathPrefix, false))
      SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_GOOGLE_READER);
  } else if (origin_host == kMailDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_MAIL_GOOGLE);
  } else if (origin_host == kPlusDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_PLUS_GOOGLE);
  } else if (origin_host == kDocsDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_DOCS_GOOGLE);
  } else if (origin_host == kSitesDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_SITES_GOOGLE);
  } else if (origin_host == kPicasawebDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_PICASAWEB_GOOGLE);
  } else if (origin_host == kCodeDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_CODE_GOOGLE);
  } else if (origin_host == kGroupsDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_GROUPS_GOOGLE);
  } else if (origin_host == kMapsDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_MAPS_GOOGLE);
  } else if (origin_host == kWWWDotYoutubeDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HOST_YOUTUBE);
  }

  GURL resource_gurl(resource_url);
  if (EndsWith(resource_gurl.path(), kDotHTML, false))
    SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY_HTML);

  if (allowed_per_settings || allow_displaying_insecure_content_)
    return true;

  Send(new ChromeViewHostMsg_DidBlockDisplayingInsecureContent(routing_id()));

  return false;
}

bool ContentSettingsObserver::allowRunningInsecureContent(
    bool allowed_per_settings,
    const blink::WebSecurityOrigin& origin,
    const blink::WebURL& resource_url) {
  std::string origin_host(origin.host().utf8());
  WebFrame* frame = render_frame()->GetWebFrame();
  GURL frame_gurl(frame->document().url());
  DCHECK_EQ(frame_gurl.host(), origin_host);

  bool is_google = IsHostInDomain(origin_host, kGoogleDotCom);
  if (is_google) {
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_GOOGLE);
    if (StartsWithASCII(frame_gurl.path(), kGoogleSupportPathPrefix, false)) {
      SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_GOOGLE_SUPPORT);
    } else if (StartsWithASCII(frame_gurl.path(),
                               kGoogleIntlPathPrefix,
                               false)) {
      SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_GOOGLE_INTL);
    }
  }

  if (origin_host == kWWWDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_WWW_GOOGLE);
    if (StartsWithASCII(frame_gurl.path(), kGoogleReaderPathPrefix, false))
      SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_GOOGLE_READER);
  } else if (origin_host == kMailDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_MAIL_GOOGLE);
  } else if (origin_host == kPlusDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_PLUS_GOOGLE);
  } else if (origin_host == kDocsDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_DOCS_GOOGLE);
  } else if (origin_host == kSitesDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_SITES_GOOGLE);
  } else if (origin_host == kPicasawebDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_PICASAWEB_GOOGLE);
  } else if (origin_host == kCodeDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_CODE_GOOGLE);
  } else if (origin_host == kGroupsDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_GROUPS_GOOGLE);
  } else if (origin_host == kMapsDotGoogleDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_MAPS_GOOGLE);
  } else if (origin_host == kWWWDotYoutubeDotCom) {
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_YOUTUBE);
  } else if (EndsWith(origin_host, kDotGoogleUserContentDotCom, false)) {
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_HOST_GOOGLEUSERCONTENT);
  }

  GURL resource_gurl(resource_url);
  if (resource_gurl.host() == kWWWDotYoutubeDotCom)
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_TARGET_YOUTUBE);

  if (EndsWith(resource_gurl.path(), kDotJS, false))
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_JS);
  else if (EndsWith(resource_gurl.path(), kDotCSS, false))
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_CSS);
  else if (EndsWith(resource_gurl.path(), kDotSWF, false))
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_SWF);

  if (!allow_running_insecure_content_ && !allowed_per_settings) {
    DidBlockContentType(CONTENT_SETTINGS_TYPE_MIXEDSCRIPT);
    return false;
  }

  return true;
}

void ContentSettingsObserver::didNotAllowPlugins() {
  DidBlockContentType(CONTENT_SETTINGS_TYPE_PLUGINS);
}

void ContentSettingsObserver::didNotAllowScript() {
  DidBlockContentType(CONTENT_SETTINGS_TYPE_JAVASCRIPT);
}

bool ContentSettingsObserver::AreNPAPIPluginsBlocked() const {
  return npapi_plugins_blocked_;
}

void ContentSettingsObserver::OnLoadBlockedPlugins(
    const std::string& identifier) {
  temporarily_allowed_plugins_.insert(identifier);
}

void ContentSettingsObserver::OnSetAsInterstitial() {
  is_interstitial_page_ = true;
}

void ContentSettingsObserver::OnNPAPINotSupported() {
  npapi_plugins_blocked_ = true;
}

void ContentSettingsObserver::OnSetAllowDisplayingInsecureContent(bool allow) {
  allow_displaying_insecure_content_ = allow;
}

void ContentSettingsObserver::OnSetAllowRunningInsecureContent(bool allow) {
  allow_running_insecure_content_ = allow;
  OnSetAllowDisplayingInsecureContent(allow);
}

void ContentSettingsObserver::OnReloadFrame() {
  DCHECK(!render_frame()->GetWebFrame()->parent()) <<
      "Should only be called on the main frame";
  render_frame()->GetWebFrame()->reload();
}

void ContentSettingsObserver::OnRequestFileSystemAccessAsyncResponse(
    int request_id,
    bool allowed) {
  PermissionRequestMap::iterator it = permission_requests_.find(request_id);
  if (it == permission_requests_.end())
    return;

  WebPermissionCallbacks callbacks = it->second;
  permission_requests_.erase(it);

  if (allowed) {
    callbacks.doAllow();
    return;
  }
  callbacks.doDeny();
}

void ContentSettingsObserver::ClearBlockedContentSettings() {
  for (size_t i = 0; i < arraysize(content_blocked_); ++i)
    content_blocked_[i] = false;
  cached_storage_permissions_.clear();
  cached_script_permissions_.clear();
}

bool ContentSettingsObserver::IsPlatformApp() {
#if defined(ENABLE_EXTENSIONS)
  WebFrame* frame = render_frame()->GetWebFrame();
  WebSecurityOrigin origin = frame->document().securityOrigin();
  const extensions::Extension* extension = GetExtension(origin);
  return extension && extension->is_platform_app();
#else
  return false;
#endif
}

#if defined(ENABLE_EXTENSIONS)
const extensions::Extension* ContentSettingsObserver::GetExtension(
    const WebSecurityOrigin& origin) const {
  if (!EqualsASCII(origin.protocol(), extensions::kExtensionScheme))
    return NULL;

  const std::string extension_id = origin.host().utf8().data();
  if (!extension_dispatcher_->IsExtensionActive(extension_id))
    return NULL;

  return extension_dispatcher_->extensions()->GetByID(extension_id);
}
#endif

bool ContentSettingsObserver::IsWhitelistedForContentSettings(
    content::RenderFrame* frame) {
  // Whitelist Instant processes.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kInstantProcess))
    return true;

  // Whitelist ftp directory listings, as they require JavaScript to function
  // properly.
  if (frame->IsFTPDirectoryListing())
    return true;

  WebFrame* web_frame = frame->GetWebFrame();
  return IsWhitelistedForContentSettings(web_frame->document().securityOrigin(),
                                         web_frame->document().url());
}

bool ContentSettingsObserver::IsWhitelistedForContentSettings(
    const WebSecurityOrigin& origin,
    const GURL& document_url) {
  if (document_url == GURL(content::kUnreachableWebDataURL))
    return true;

  if (origin.isUnique())
    return false;  // Uninitialized document?

  if (EqualsASCII(origin.protocol(), content::kChromeUIScheme))
    return true;  // Browser UI elements should still work.

  if (EqualsASCII(origin.protocol(), content::kChromeDevToolsScheme))
    return true;  // DevTools UI elements should still work.

#if defined(ENABLE_EXTENSIONS)
  if (EqualsASCII(origin.protocol(), extensions::kExtensionScheme))
    return true;
#endif

  // TODO(creis, fsamuel): Remove this once the concept of swapped out
  // RenderFrames goes away.
  if (document_url == GURL(content::kSwappedOutURL))
    return true;

  // If the scheme is file:, an empty file name indicates a directory listing,
  // which requires JavaScript to function properly.
  if (EqualsASCII(origin.protocol(), url::kFileScheme)) {
    return document_url.SchemeIs(url::kFileScheme) &&
           document_url.ExtractFileName().empty();
  }

  return false;
}
