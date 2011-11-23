// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/content_settings_observer.h"

#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/navigation_state.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDataSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrameClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

using WebKit::WebDataSource;
using WebKit::WebFrame;
using WebKit::WebFrameClient;
using WebKit::WebSecurityOrigin;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebView;
using content::DocumentState;
using content::NavigationState;

namespace {

// True if |frame| contains content that is white-listed for content settings.
static bool IsWhitelistedForContentSettings(WebFrame* frame) {
  WebSecurityOrigin origin = frame->document().securityOrigin();
  if (origin.isUnique())
    return false;  // Uninitialized document?

  if (EqualsASCII(origin.protocol(), chrome::kChromeUIScheme))
    return true;  // Browser UI elements should still work.

  if (EqualsASCII(origin.protocol(), chrome::kChromeDevToolsScheme))
    return true;  // DevTools UI elements should still work.

  // If the scheme is ftp: or file:, an empty file name indicates a directory
  // listing, which requires JavaScript to function properly.
  GURL document_url = frame->document().url();
  const char* kDirProtocols[] = { chrome::kFtpScheme, chrome::kFileScheme };
  for (size_t i = 0; i < arraysize(kDirProtocols); ++i) {
    if (EqualsASCII(origin.protocol(), kDirProtocols[i])) {
      return document_url.SchemeIs(kDirProtocols[i]) &&
             document_url.ExtractFileName().empty();
    }
  }

  return false;
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
    content::RenderView* render_view)
    : content::RenderViewObserver(render_view),
      content::RenderViewObserverTracker<ContentSettingsObserver>(render_view),
      content_setting_rules_(NULL),
      plugins_temporarily_allowed_(false) {
  ClearBlockedContentSettings();
}

ContentSettingsObserver::~ContentSettingsObserver() {
}

void ContentSettingsObserver::SetContentSettingRules(
    const RendererContentSettingRules* content_setting_rules) {
  content_setting_rules_ = content_setting_rules;
}

void ContentSettingsObserver::DidBlockContentType(
    ContentSettingsType settings_type,
    const std::string& resource_identifier) {
  // Always send a message when |resource_identifier| is not empty, to tell the
  // browser which resource was blocked (otherwise the browser will only show
  // the first resource to be blocked, and none that are blocked at a later
  // time).
  if (!content_blocked_[settings_type] || !resource_identifier.empty()) {
    content_blocked_[settings_type] = true;
    Send(new ChromeViewHostMsg_ContentBlocked(routing_id(), settings_type,
                                        resource_identifier));
  }
}

bool ContentSettingsObserver::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ContentSettingsObserver, message)
    // Don't swallow LoadBlockedPlugins messages, as they're sent to every
    // blocked plugin.
    IPC_MESSAGE_HANDLER_GENERIC(ChromeViewMsg_LoadBlockedPlugins,
                                OnLoadBlockedPlugins(); handled = false)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ContentSettingsObserver::DidCommitProvisionalLoad(
    WebFrame* frame, bool is_new_navigation) {
  if (frame->parent())
    return; // Not a top-level navigation.

  DocumentState* document_state = DocumentState::FromDataSource(
      frame->dataSource());
  NavigationState* navigation_state = document_state->navigation_state();
  if (!navigation_state->was_within_same_page()) {
    // Clear "block" flags for the new page. This needs to happen before any of
    // |AllowScript()|, |AllowScriptFromSource()|, |AllowImage()|, or
    // |AllowPlugins()| is called for the new page so that these functions can
    // correctly detect that a piece of content flipped from "not blocked" to
    // "blocked".
    ClearBlockedContentSettings();
    plugins_temporarily_allowed_ = false;
  }

  GURL url = frame->document().url();
  // If we start failing this DCHECK, please makes sure we don't regress
  // this bug: http://code.google.com/p/chromium/issues/detail?id=79304
  DCHECK(frame->document().securityOrigin().toString() == "null" ||
         !url.SchemeIs(chrome::kDataScheme));
}

bool ContentSettingsObserver::AllowDatabase(WebFrame* frame,
                                            const WebString& name,
                                            const WebString& display_name,
                                            unsigned long estimated_size) {
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

bool ContentSettingsObserver::AllowFileSystem(WebFrame* frame) {
  if (frame->document().securityOrigin().isUnique() ||
      frame->top()->document().securityOrigin().isUnique())
    return false;

  bool result = false;
  Send(new ChromeViewHostMsg_AllowFileSystem(
      routing_id(), GURL(frame->document().securityOrigin().toString()),
      GURL(frame->top()->document().securityOrigin().toString()), &result));
  return result;
}

bool ContentSettingsObserver::AllowImage(WebFrame* frame,
                                         bool enabled_per_settings,
                                         const WebURL& image_url) {
  if (IsWhitelistedForContentSettings(frame))
    return true;

  bool allow = enabled_per_settings;
  if (content_setting_rules_ && enabled_per_settings) {
    GURL secondary_url(image_url);
    allow = GetContentSettingFromRules(
        content_setting_rules_->image_rules,
        frame, secondary_url) != CONTENT_SETTING_BLOCK;
  }

  if (!allow)
    DidBlockContentType(CONTENT_SETTINGS_TYPE_IMAGES, std::string());
  return allow;
}

bool ContentSettingsObserver::AllowIndexedDB(WebFrame* frame,
                                             const WebString& name,
                                             const WebSecurityOrigin& origin) {
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

bool ContentSettingsObserver::AllowPlugins(WebFrame* frame,
                                           bool enabled_per_settings) {
  return enabled_per_settings;
}

bool ContentSettingsObserver::AllowScript(WebFrame* frame,
                                          bool enabled_per_settings) {
  if (!enabled_per_settings)
    return false;

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
  allow = allow || IsWhitelistedForContentSettings(frame);

  cached_script_permissions_[frame] = allow;
  return allow;
}

bool ContentSettingsObserver::AllowScriptFromSource(
    WebFrame* frame,
    bool enabled_per_settings,
    const WebKit::WebURL& script_url) {
  if (!enabled_per_settings)
    return false;

  bool allow = true;
  if (content_setting_rules_) {
    ContentSetting setting = GetContentSettingFromRules(
        content_setting_rules_->script_rules,
        frame,
        GURL(script_url));
    allow = setting != CONTENT_SETTING_BLOCK;
  }
  return allow || IsWhitelistedForContentSettings(frame);
}

bool ContentSettingsObserver::AllowStorage(WebFrame* frame, bool local) {
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

void ContentSettingsObserver::DidNotAllowPlugins(WebFrame* frame) {
  DidBlockContentType(CONTENT_SETTINGS_TYPE_PLUGINS, std::string());
}

void ContentSettingsObserver::DidNotAllowScript(WebFrame* frame) {
  DidBlockContentType(CONTENT_SETTINGS_TYPE_JAVASCRIPT, std::string());
}

void ContentSettingsObserver::OnLoadBlockedPlugins() {
  plugins_temporarily_allowed_ = true;
}

void ContentSettingsObserver::ClearBlockedContentSettings() {
  for (size_t i = 0; i < arraysize(content_blocked_); ++i)
    content_blocked_[i] = false;
  cached_storage_permissions_.clear();
  cached_script_permissions_.clear();
}
