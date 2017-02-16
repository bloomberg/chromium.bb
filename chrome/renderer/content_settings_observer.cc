// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/content_settings_observer.h"

#include "chrome/common/render_messages.h"
#include "chrome/common/ssl_insecure_content.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "extensions/features/features.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "third_party/WebKit/public/platform/URLConversion.h"
#include "third_party/WebKit/public/platform/WebContentSettingCallbacks.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrameClient.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "url/origin.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
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

GURL GetOriginOrURL(const WebFrame* frame) {
  url::Origin top_origin = url::Origin(frame->top()->getSecurityOrigin());
  // The |top_origin| is unique ("null") e.g., for file:// URLs. Use the
  // document URL as the primary URL in those cases.
  // TODO(alexmos): This is broken for --site-per-process, since top() can be a
  // WebRemoteFrame which does not have a document(), and the WebRemoteFrame's
  // URL is not replicated.  See https://crbug.com/628759.
  if (top_origin.unique() && frame->top()->isWebLocalFrame())
    return frame->top()->document().url();
  return top_origin.GetURL();
}

// Allow passing both WebURL and GURL here, so that we can early return without
// allocating a new backing string if only the default rule matches.
template <typename URL>
ContentSetting GetContentSettingFromRules(
    const ContentSettingsForOneType& rules,
    const WebFrame* frame,
    const URL& secondary_url) {
  ContentSettingsForOneType::const_iterator it;
  // If there is only one rule, it's the default rule and we don't need to match
  // the patterns.
  if (rules.size() == 1) {
    DCHECK(rules[0].primary_pattern == ContentSettingsPattern::Wildcard());
    DCHECK(rules[0].secondary_pattern == ContentSettingsPattern::Wildcard());
    return rules[0].setting;
  }
  const GURL& primary_url = GetOriginOrURL(frame);
  const GURL& secondary_gurl = secondary_url;
  for (it = rules.begin(); it != rules.end(); ++it) {
    if (it->primary_pattern.Matches(primary_url) &&
        it->secondary_pattern.Matches(secondary_gurl)) {
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
#if BUILDFLAG(ENABLE_EXTENSIONS)
      extension_dispatcher_(extension_dispatcher),
#endif
      allow_running_insecure_content_(false),
      content_setting_rules_(NULL),
      is_interstitial_page_(false),
      current_request_id_(0),
      should_whitelist_(should_whitelist) {
  ClearBlockedContentSettings();
  render_frame->GetWebFrame()->setContentSettingsClient(this);

  render_frame->GetInterfaceRegistry()->AddInterface(
      base::Bind(&ContentSettingsObserver::OnInsecureContentRendererRequest,
                 base::Unretained(this)));

  content::RenderFrame* main_frame =
      render_frame->GetRenderView()->GetMainRenderFrame();
  // TODO(nasko): The main frame is not guaranteed to be in the same process
  // with this frame with --site-per-process. This code needs to be updated
  // to handle this case. See https://crbug.com/496670.
  if (main_frame && main_frame != render_frame) {
    // Copy all the settings from the main render frame to avoid race conditions
    // when initializing this data. See https://crbug.com/333308.
    ContentSettingsObserver* parent = ContentSettingsObserver::Get(main_frame);
    allow_running_insecure_content_ = parent->allow_running_insecure_content_;
    temporarily_allowed_plugins_ = parent->temporarily_allowed_plugins_;
    is_interstitial_page_ = parent->is_interstitial_page_;
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
  DCHECK(frame->document().getSecurityOrigin().toString() == "null" ||
         !url.SchemeIs(url::kDataScheme));
}

void ContentSettingsObserver::OnDestruct() {
  delete this;
}

void ContentSettingsObserver::SetAllowRunningInsecureContent() {
  allow_running_insecure_content_ = true;

  // Reload if we are the main frame.
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  if (!frame->parent())
    frame->reload(blink::WebFrameLoadType::ReloadMainResource);
}

void ContentSettingsObserver::OnInsecureContentRendererRequest(
    chrome::mojom::InsecureContentRendererRequest request) {
  insecure_content_renderer_bindings_.AddBinding(this, std::move(request));
}

bool ContentSettingsObserver::allowDatabase(const WebString& name,
                                            const WebString& display_name,
                                            unsigned estimated_size) {
  WebFrame* frame = render_frame()->GetWebFrame();
  if (frame->getSecurityOrigin().isUnique() ||
      frame->top()->getSecurityOrigin().isUnique())
    return false;

  bool result = false;
  Send(new ChromeViewHostMsg_AllowDatabase(
      routing_id(), url::Origin(frame->getSecurityOrigin()).GetURL(),
      url::Origin(frame->top()->getSecurityOrigin()).GetURL(), name.utf16(),
      display_name.utf16(), &result));
  return result;
}

void ContentSettingsObserver::requestFileSystemAccessAsync(
    const WebContentSettingCallbacks& callbacks) {
  WebFrame* frame = render_frame()->GetWebFrame();
  if (frame->getSecurityOrigin().isUnique() ||
      frame->top()->getSecurityOrigin().isUnique()) {
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
      url::Origin(frame->getSecurityOrigin()).GetURL(),
      url::Origin(frame->top()->getSecurityOrigin()).GetURL()));
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
      allow = GetContentSettingFromRules(content_setting_rules_->image_rules,
                                         render_frame()->GetWebFrame(),
                                         image_url) != CONTENT_SETTING_BLOCK;
    }
  }
  if (!allow)
    DidBlockContentType(CONTENT_SETTINGS_TYPE_IMAGES);
  return allow;
}

bool ContentSettingsObserver::allowIndexedDB(const WebString& name,
                                             const WebSecurityOrigin& origin) {
  WebFrame* frame = render_frame()->GetWebFrame();
  if (frame->getSecurityOrigin().isUnique() ||
      frame->top()->getSecurityOrigin().isUnique())
    return false;

  bool result = false;
  Send(new ChromeViewHostMsg_AllowIndexedDB(
      routing_id(), url::Origin(frame->getSecurityOrigin()).GetURL(),
      url::Origin(frame->top()->getSecurityOrigin()).GetURL(), name.utf16(),
      &result));
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
  const auto it = cached_script_permissions_.find(frame);
  if (it != cached_script_permissions_.end())
    return it->second;

  // Evaluate the content setting rules before
  // |IsWhitelistedForContentSettings|; if there is only the default rule
  // allowing all scripts, it's quicker this way.
  bool allow = true;
  if (content_setting_rules_) {
    ContentSetting setting = GetContentSettingFromRules(
        content_setting_rules_->script_rules, frame,
        url::Origin(frame->document().getSecurityOrigin()).GetURL());
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
                                   render_frame()->GetWebFrame(), script_url);
    allow = setting != CONTENT_SETTING_BLOCK;
  }
  return allow || IsWhitelistedForContentSettings();
}

bool ContentSettingsObserver::allowStorage(bool local) {
  WebFrame* frame = render_frame()->GetWebFrame();
  if (frame->getSecurityOrigin().isUnique() ||
      frame->top()->getSecurityOrigin().isUnique())
    return false;

  StoragePermissionsKey key(
      url::Origin(frame->document().getSecurityOrigin()).GetURL(), local);
  const auto permissions = cached_storage_permissions_.find(key);
  if (permissions != cached_storage_permissions_.end())
    return permissions->second;

  bool result = false;
  Send(new ChromeViewHostMsg_AllowDOMStorage(
      routing_id(), url::Origin(frame->getSecurityOrigin()).GetURL(),
      url::Origin(frame->top()->getSecurityOrigin()).GetURL(), local, &result));
  cached_storage_permissions_[key] = result;
  return result;
}

bool ContentSettingsObserver::allowReadFromClipboard(bool default_value) {
  bool allowed = default_value;
#if BUILDFLAG(ENABLE_EXTENSIONS)
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
#if BUILDFLAG(ENABLE_EXTENSIONS)
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

bool ContentSettingsObserver::allowRunningInsecureContent(
    bool allowed_per_settings,
    const blink::WebSecurityOrigin& origin,
    const blink::WebURL& resource_url) {
  // Note: this implementation is a mirror of
  // Browser::ShouldAllowRunningInsecureContent.
  FilteredReportInsecureContentRan(GURL(resource_url));

  if (!allow_running_insecure_content_ && !allowed_per_settings) {
    DidBlockContentType(CONTENT_SETTINGS_TYPE_MIXEDSCRIPT);
    return false;
  }

  return true;
}

bool ContentSettingsObserver::allowAutoplay(bool default_value) {
  if (!content_setting_rules_)
    return default_value;

  WebFrame* frame = render_frame()->GetWebFrame();
  return GetContentSettingFromRules(
             content_setting_rules_->autoplay_rules, frame,
             url::Origin(frame->document().getSecurityOrigin()).GetURL()) ==
         CONTENT_SETTING_ALLOW;
}

void ContentSettingsObserver::passiveInsecureContentFound(
    const blink::WebURL& resource_url) {
  // Note: this implementation is a mirror of
  // Browser::PassiveInsecureContentFound.
  ReportInsecureContent(SslInsecureContentType::DISPLAY);
  FilteredReportInsecureContentDisplayed(GURL(resource_url));
}

void ContentSettingsObserver::didNotAllowPlugins() {
  DidBlockContentType(CONTENT_SETTINGS_TYPE_PLUGINS);
}

void ContentSettingsObserver::didNotAllowScript() {
  DidBlockContentType(CONTENT_SETTINGS_TYPE_JAVASCRIPT);
}

void ContentSettingsObserver::OnLoadBlockedPlugins(
    const std::string& identifier) {
  temporarily_allowed_plugins_.insert(identifier);
}

void ContentSettingsObserver::OnSetAsInterstitial() {
  is_interstitial_page_ = true;
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
#if BUILDFLAG(ENABLE_EXTENSIONS)
  WebFrame* frame = render_frame()->GetWebFrame();
  WebSecurityOrigin origin = frame->document().getSecurityOrigin();
  const extensions::Extension* extension = GetExtension(origin);
  return extension && extension->is_platform_app();
#else
  return false;
#endif
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
const extensions::Extension* ContentSettingsObserver::GetExtension(
    const WebSecurityOrigin& origin) const {
  if (origin.protocol().ascii() != extensions::kExtensionScheme)
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

  const WebDocument& document = render_frame()->GetWebFrame()->document();
  return IsWhitelistedForContentSettings(document.getSecurityOrigin(),
                                         document.url());
}

bool ContentSettingsObserver::IsWhitelistedForContentSettings(
    const WebSecurityOrigin& origin,
    const WebURL& document_url) {
  if (document_url.string() == content::kUnreachableWebDataURL)
    return true;

  if (origin.isUnique())
    return false;  // Uninitialized document?

  blink::WebString protocol = origin.protocol();

  if (protocol == content::kChromeUIScheme)
    return true;  // Browser UI elements should still work.

  if (protocol == content::kChromeDevToolsScheme)
    return true;  // DevTools UI elements should still work.

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (protocol == extensions::kExtensionScheme)
    return true;
#endif

  // If the scheme is file:, an empty file name indicates a directory listing,
  // which requires JavaScript to function properly.
  if (protocol == url::kFileScheme &&
      document_url.protocolIs(url::kFileScheme)) {
    return GURL(document_url).ExtractFileName().empty();
  }
  return false;
}
