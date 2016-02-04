// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/content_settings_observer.h"

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "components/content_settings/content/common/content_settings_messages.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/public/platform/URLConversion.h"
#include "third_party/WebKit/public/platform/WebContentSettingCallbacks.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrameClient.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "url/url_constants.h"

#if defined(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/renderer_extension_registry.h"
#endif

using blink::WebContentSettingCallbacks;
using blink::WebDataSource;
using blink::WebDocument;
using blink::WebFrame;
using blink::WebSecurityOrigin;
using blink::WebString;
using blink::WebURL;
using blink::WebView;
using content::DocumentState;
using content::NavigationState;

namespace {

// This enum is histogrammed, so do not add, reorder, or remove values.
enum {
  INSECURE_CONTENT_DISPLAY = 0,
  INSECURE_CONTENT_DISPLAY_HOST_GOOGLE,      // deprecated
  INSECURE_CONTENT_DISPLAY_HOST_WWW_GOOGLE,  // deprecated
  INSECURE_CONTENT_DISPLAY_HTML,
  INSECURE_CONTENT_RUN,
  INSECURE_CONTENT_RUN_HOST_GOOGLE,      // deprecated
  INSECURE_CONTENT_RUN_HOST_WWW_GOOGLE,  // deprecated
  INSECURE_CONTENT_RUN_TARGET_YOUTUBE,   // deprecated
  INSECURE_CONTENT_RUN_JS,
  INSECURE_CONTENT_RUN_CSS,
  INSECURE_CONTENT_RUN_SWF,
  INSECURE_CONTENT_DISPLAY_HOST_YOUTUBE,           // deprecated
  INSECURE_CONTENT_RUN_HOST_YOUTUBE,               // deprecated
  INSECURE_CONTENT_RUN_HOST_GOOGLEUSERCONTENT,     // deprecated
  INSECURE_CONTENT_DISPLAY_HOST_MAIL_GOOGLE,       // deprecated
  INSECURE_CONTENT_RUN_HOST_MAIL_GOOGLE,           // deprecated
  INSECURE_CONTENT_DISPLAY_HOST_PLUS_GOOGLE,       // deprecated
  INSECURE_CONTENT_RUN_HOST_PLUS_GOOGLE,           // deprecated
  INSECURE_CONTENT_DISPLAY_HOST_DOCS_GOOGLE,       // deprecated
  INSECURE_CONTENT_RUN_HOST_DOCS_GOOGLE,           // deprecated
  INSECURE_CONTENT_DISPLAY_HOST_SITES_GOOGLE,      // deprecated
  INSECURE_CONTENT_RUN_HOST_SITES_GOOGLE,          // deprecated
  INSECURE_CONTENT_DISPLAY_HOST_PICASAWEB_GOOGLE,  // deprecated
  INSECURE_CONTENT_RUN_HOST_PICASAWEB_GOOGLE,      // deprecated
  INSECURE_CONTENT_DISPLAY_HOST_GOOGLE_READER,     // deprecated
  INSECURE_CONTENT_RUN_HOST_GOOGLE_READER,         // deprecated
  INSECURE_CONTENT_DISPLAY_HOST_CODE_GOOGLE,       // deprecated
  INSECURE_CONTENT_RUN_HOST_CODE_GOOGLE,           // deprecated
  INSECURE_CONTENT_DISPLAY_HOST_GROUPS_GOOGLE,     // deprecated
  INSECURE_CONTENT_RUN_HOST_GROUPS_GOOGLE,         // deprecated
  INSECURE_CONTENT_DISPLAY_HOST_MAPS_GOOGLE,       // deprecated
  INSECURE_CONTENT_RUN_HOST_MAPS_GOOGLE,           // deprecated
  INSECURE_CONTENT_DISPLAY_HOST_GOOGLE_SUPPORT,    // deprecated
  INSECURE_CONTENT_RUN_HOST_GOOGLE_SUPPORT,        // deprecated
  INSECURE_CONTENT_DISPLAY_HOST_GOOGLE_INTL,       // deprecated
  INSECURE_CONTENT_RUN_HOST_GOOGLE_INTL,           // deprecated
  INSECURE_CONTENT_NUM_EVENTS
};

// Constants for UMA statistic collection.
static const char kDotJS[] = ".js";
static const char kDotCSS[] = ".css";
static const char kDotSWF[] = ".swf";
static const char kDotHTML[] = ".html";

GURL GetOriginOrURL(const WebFrame* frame) {
  WebString top_origin = frame->top()->securityOrigin().toString();
  // The |top_origin| is unique ("null") e.g., for file:// URLs. Use the
  // document URL as the primary URL in those cases.
  // TODO(alexmos): This is broken for --site-per-process, since top() can be a
  // WebRemoteFrame which does not have a document(), and the WebRemoteFrame's
  // URL is not replicated.
  if (top_origin == "null")
    return frame->top()->document().url();
  return blink::WebStringToGURL(top_origin);
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
    extensions::Dispatcher* extension_dispatcher,
    bool should_whitelist)
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
      current_request_id_(0),
      should_whitelist_(should_whitelist) {
  ClearBlockedContentSettings();
  render_frame->GetWebFrame()->setContentSettingsClient(this);

  content::RenderFrame* main_frame =
      render_frame->GetRenderView()->GetMainRenderFrame();
  // TODO(nasko): The main frame is not guaranteed to be in the same process
  // with this frame with --site-per-process. This code needs to be updated
  // to handle this case. See https://crbug.com/496670.
  if (main_frame && main_frame != render_frame) {
    // Copy all the settings from the main render frame to avoid race conditions
    // when initializing this data. See https://crbug.com/333308.
    ContentSettingsObserver* parent = ContentSettingsObserver::Get(main_frame);
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
  // If the empty string is in here, it means all plugins are allowed.
  // TODO(bauerb): Remove this once we only pass in explicit identifiers.
  return (temporarily_allowed_plugins_.find(identifier) !=
          temporarily_allowed_plugins_.end()) ||
         (temporarily_allowed_plugins_.find(std::string()) !=
          temporarily_allowed_plugins_.end());
}

void ContentSettingsObserver::DidBlockContentType(
    ContentSettingsType settings_type) {
  DidBlockContentType(settings_type, base::string16());
}

void ContentSettingsObserver::DidBlockContentType(
    ContentSettingsType settings_type,
    const base::string16& details) {
  // Send multiple ContentBlocked messages if details are provided.
  bool& blocked = content_blocked_[settings_type];
  if (!blocked || !details.empty()) {
    blocked = true;
    Send(new ChromeViewHostMsg_ContentBlocked(routing_id(), settings_type,
                                              details));
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

void ContentSettingsObserver::DidCommitProvisionalLoad(
    bool is_new_navigation,
    bool is_same_page_navigation) {
  WebFrame* frame = render_frame()->GetWebFrame();
  if (frame->parent())
    return;  // Not a top-level navigation.

  if (!is_same_page_navigation) {
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
  if (frame->securityOrigin().isUnique() ||
      frame->top()->securityOrigin().isUnique())
    return false;

  bool result = false;
  Send(new ChromeViewHostMsg_AllowDatabase(
      routing_id(),
      blink::WebStringToGURL(frame->securityOrigin().toString()),
      blink::WebStringToGURL(frame->top()->securityOrigin().toString()),
      name, display_name, &result));
  return result;
}

void ContentSettingsObserver::requestFileSystemAccessAsync(
    const WebContentSettingCallbacks& callbacks) {
  WebFrame* frame = render_frame()->GetWebFrame();
  if (frame->securityOrigin().isUnique() ||
      frame->top()->securityOrigin().isUnique()) {
    WebContentSettingCallbacks permissionCallbacks(callbacks);
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
      routing_id(), current_request_id_,
      blink::WebStringToGURL(frame->securityOrigin().toString()),
      blink::WebStringToGURL(frame->top()->securityOrigin().toString())));
}

bool ContentSettingsObserver::allowImage(bool enabled_per_settings,
                                         const WebURL& image_url) {
  bool allow = enabled_per_settings;
  if (enabled_per_settings) {
    if (is_interstitial_page_)
      return true;

    if (IsWhitelistedForContentSettings())
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
  if (frame->securityOrigin().isUnique() ||
      frame->top()->securityOrigin().isUnique())
    return false;

  bool result = false;
  Send(new ChromeViewHostMsg_AllowIndexedDB(
      routing_id(),
      blink::WebStringToGURL(frame->securityOrigin().toString()),
      blink::WebStringToGURL(frame->top()->securityOrigin().toString()),
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
        blink::WebStringToGURL(
            frame->document().securityOrigin().toString()));
    allow = setting != CONTENT_SETTING_BLOCK;
  }
  allow = allow || IsWhitelistedForContentSettings();

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
  return allow || IsWhitelistedForContentSettings();
}

bool ContentSettingsObserver::allowStorage(bool local) {
  WebFrame* frame = render_frame()->GetWebFrame();
  if (frame->securityOrigin().isUnique() ||
      frame->top()->securityOrigin().isUnique())
    return false;
  bool result = false;

  StoragePermissionsKey key(
      blink::WebStringToGURL(frame->document().securityOrigin().toString()),
      local);
  std::map<StoragePermissionsKey, bool>::const_iterator permissions =
      cached_storage_permissions_.find(key);
  if (permissions != cached_storage_permissions_.end())
    return permissions->second;

  Send(new ChromeViewHostMsg_AllowDOMStorage(
      routing_id(),
      blink::WebStringToGURL(frame->securityOrigin().toString()),
      blink::WebStringToGURL(frame->top()->securityOrigin().toString()),
      local, &result));
  cached_storage_permissions_[key] = result;
  return result;
}

bool ContentSettingsObserver::allowReadFromClipboard(bool default_value) {
  bool allowed = default_value;
#if defined(ENABLE_EXTENSIONS)
  extensions::ScriptContext* current_context =
      extension_dispatcher_->script_context_set().GetCurrent();
  if (current_context) {
    allowed |= current_context->HasAPIPermission(
        extensions::APIPermission::kClipboardRead);
  }
#endif
  return allowed;
}

bool ContentSettingsObserver::allowWriteToClipboard(bool default_value) {
  bool allowed = default_value;
#if defined(ENABLE_EXTENSIONS)
  // All blessed extension pages could historically write to the clipboard, so
  // preserve that for compatibility.
  extensions::ScriptContext* current_context =
      extension_dispatcher_->script_context_set().GetCurrent();
  if (current_context) {
    if (current_context->effective_context_type() ==
        extensions::Feature::BLESSED_EXTENSION_CONTEXT) {
      allowed = true;
    } else {
      allowed |= current_context->HasAPIPermission(
          extensions::APIPermission::kClipboardWrite);
    }
  }
#endif
  return allowed;
}

bool ContentSettingsObserver::allowMutationEvents(bool default_value) {
  return IsPlatformApp() ? false : default_value;
}

static void SendInsecureContentSignal(int signal) {
  UMA_HISTOGRAM_ENUMERATION("SSL.InsecureContent", signal,
                            INSECURE_CONTENT_NUM_EVENTS);
}

bool ContentSettingsObserver::allowDisplayingInsecureContent(
    bool allowed_per_settings,
    const blink::WebURL& resource_url) {
  SendInsecureContentSignal(INSECURE_CONTENT_DISPLAY);

  GURL resource_gurl(resource_url);
  if (base::EndsWith(resource_gurl.path(), kDotHTML,
                     base::CompareCase::INSENSITIVE_ASCII))
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
  GURL resource_gurl(resource_url);
  if (base::EndsWith(resource_gurl.path(), kDotJS,
                     base::CompareCase::INSENSITIVE_ASCII))
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_JS);
  else if (base::EndsWith(resource_gurl.path(), kDotCSS,
                          base::CompareCase::INSENSITIVE_ASCII))
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_CSS);
  else if (base::EndsWith(resource_gurl.path(), kDotSWF,
                          base::CompareCase::INSENSITIVE_ASCII))
    SendInsecureContentSignal(INSECURE_CONTENT_RUN_SWF);

  if (!allow_running_insecure_content_ && !allowed_per_settings) {
    DidBlockContentType(CONTENT_SETTINGS_TYPE_MIXEDSCRIPT, origin.host());
    return false;
  }

  return true;
}

void ContentSettingsObserver::didUseKeygen() {
  WebFrame* frame = render_frame()->GetWebFrame();
  Send(new ChromeViewHostMsg_DidUseKeygen(
      routing_id(),
      blink::WebStringToGURL(frame->securityOrigin().toString())));
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

  WebContentSettingCallbacks callbacks = it->second;
  permission_requests_.erase(it);

  if (allowed) {
    callbacks.doAllow();
    return;
  }
  callbacks.doDeny();
}

void ContentSettingsObserver::ClearBlockedContentSettings() {
  content_blocked_.clear();
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
  if (!base::EqualsASCII(base::StringPiece16(origin.protocol()),
                         extensions::kExtensionScheme))
    return NULL;

  const std::string extension_id = origin.host().utf8().data();
  if (!extension_dispatcher_->IsExtensionActive(extension_id))
    return NULL;

  return extensions::RendererExtensionRegistry::Get()->GetByID(extension_id);
}
#endif

bool ContentSettingsObserver::IsWhitelistedForContentSettings() const {
  if (should_whitelist_)
    return true;

  // Whitelist ftp directory listings, as they require JavaScript to function
  // properly.
  if (render_frame()->IsFTPDirectoryListing())
    return true;

  WebFrame* web_frame = render_frame()->GetWebFrame();
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

  base::string16 protocol = origin.protocol();
  if (base::EqualsASCII(protocol, content::kChromeUIScheme))
    return true;  // Browser UI elements should still work.

  if (base::EqualsASCII(protocol, content::kChromeDevToolsScheme))
    return true;  // DevTools UI elements should still work.

#if defined(ENABLE_EXTENSIONS)
  if (base::EqualsASCII(protocol, extensions::kExtensionScheme))
    return true;
#endif

  // TODO(creis, fsamuel): Remove this once the concept of swapped out
  // RenderFrames goes away.
  if (document_url == GURL(content::kSwappedOutURL))
    return true;

  // If the scheme is file:, an empty file name indicates a directory listing,
  // which requires JavaScript to function properly.
  if (base::EqualsASCII(protocol, url::kFileScheme)) {
    return document_url.SchemeIs(url::kFileScheme) &&
           document_url.ExtractFileName().empty();
  }

  return false;
}
