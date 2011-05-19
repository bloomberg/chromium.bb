// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/content_settings_observer.h"

#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/common/database_messages.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_view.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDataSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrameClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

using WebKit::WebDataSource;
using WebKit::WebFrame;
using WebKit::WebFrameClient;
using WebKit::WebSecurityOrigin;
using WebKit::WebString;
using WebKit::WebURLRequest;
using WebKit::WebView;

namespace {

// True if |frame| contains content that is white-listed for content settings.
static bool IsWhitelistedForContentSettings(WebFrame* frame) {
  WebSecurityOrigin origin = frame->securityOrigin();
  if (origin.isEmpty())
    return false;  // Uninitialized document?

  if (EqualsASCII(origin.protocol(), chrome::kChromeUIScheme))
    return true;  // Browser UI elements should still work.

  // If the scheme is ftp: or file:, an empty file name indicates a directory
  // listing, which requires JavaScript to function properly.
  GURL frame_url = frame->url();
  const char* kDirProtocols[] = { "ftp", "file" };
  for (size_t i = 0; i < arraysize(kDirProtocols); ++i) {
    if (EqualsASCII(origin.protocol(), kDirProtocols[i])) {
      return frame_url.SchemeIs(kDirProtocols[i]) &&
             frame_url.ExtractFileName().empty();
    }
  }

  return false;
}

}  // namespace

ContentSettingsObserver::ContentSettingsObserver(RenderView* render_view)
    : RenderViewObserver(render_view),
      RenderViewObserverTracker<ContentSettingsObserver>(render_view),
      plugins_temporarily_allowed_(false) {
  ClearBlockedContentSettings();
}

ContentSettingsObserver::~ContentSettingsObserver() {
}


void ContentSettingsObserver::SetContentSettings(
    const ContentSettings& settings) {
  current_content_settings_ = settings;
}

ContentSetting ContentSettingsObserver::GetContentSetting(
    ContentSettingsType type) {
  if (type == CONTENT_SETTINGS_TYPE_PLUGINS &&
      plugins_temporarily_allowed_) {
    return CONTENT_SETTING_ALLOW;
  }
  return current_content_settings_.settings[type];
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
    Send(new ViewHostMsg_ContentBlocked(routing_id(), settings_type,
                                        resource_identifier));
  }
}

bool ContentSettingsObserver::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ContentSettingsObserver, message)
    // Don't swallow LoadBlockedPlugins messages, as they're sent to every
    // blocked plugin.
    IPC_MESSAGE_HANDLER_GENERIC(ViewMsg_LoadBlockedPlugins,
                                OnLoadBlockedPlugins(); handled = false)
    IPC_MESSAGE_HANDLER(ViewMsg_SetContentSettingsForLoadingURL,
                        OnSetContentSettingsForLoadingURL)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ContentSettingsObserver::DidCommitProvisionalLoad(
    WebFrame* frame, bool is_new_navigation) {
  if (frame->parent())
    return; // Not a top-level navigation.

  WebDataSource* ds = frame->dataSource();
  const WebURLRequest& request = ds->request();

  // Clear "block" flags for the new page. This needs to happen before any of
  // allowScripts(), allowImages(), allowPlugins() is called for the new page
  // so that these functions can correctly detect that a piece of content
  // flipped from "not blocked" to "blocked".
  ClearBlockedContentSettings();

  // Set content settings. Default them from the parent window if one exists.
  // This makes sure about:blank windows work as expected.
  HostContentSettings::iterator host_content_settings =
      host_content_settings_.find(GURL(request.url()));
  if (host_content_settings != host_content_settings_.end()) {
    SetContentSettings(host_content_settings->second);

    // These content settings were merely recorded transiently for this load.
    // We can erase them now.  If at some point we reload this page, the
    // browser will send us new, up-to-date content settings.
    host_content_settings_.erase(host_content_settings);
  } else if (frame->opener()) {
    // The opener's view is not guaranteed to be non-null (it could be
    // detached from its page but not yet destructed).
    if (WebView* opener_view = frame->opener()->view()) {
      RenderView* opener = RenderView::FromWebView(opener_view);
      ContentSettingsObserver* observer = ContentSettingsObserver::Get(opener);
      SetContentSettings(observer->current_content_settings_);
    }
  }
}

bool ContentSettingsObserver::AllowDatabase(WebFrame* frame,
                                            const WebString& name,
                                            const WebString& display_name,
                                            unsigned long estimated_size) {
  WebSecurityOrigin origin = frame->securityOrigin();
  if (origin.isEmpty())
    return false;  // Uninitialized document?

  bool result = false;
  Send(new ViewHostMsg_AllowDatabase(
      routing_id(), GURL(origin.toString()), name, display_name, &result));
  return result;
}

bool ContentSettingsObserver::AllowFileSystem(WebFrame* frame) {
  WebSecurityOrigin origin = frame->securityOrigin();
  if (origin.isEmpty())
    return false;  // Uninitialized document?

  bool result = false;
  Send(new ViewHostMsg_AllowFileSystem(
      routing_id(), GURL(origin.toString()), &result));
  return result;
}

bool ContentSettingsObserver::AllowImages(WebFrame* frame,
                                          bool enabled_per_settings) {
  if (enabled_per_settings &&
      AllowContentType(CONTENT_SETTINGS_TYPE_IMAGES)) {
    return true;
  }

  if (IsWhitelistedForContentSettings(frame))
    return true;

  DidBlockContentType(CONTENT_SETTINGS_TYPE_IMAGES, std::string());
  return false;  // Other protocols fall through here.
}

bool ContentSettingsObserver::AllowIndexedDB(WebFrame* frame,
                                             const WebString& name,
                                             const WebSecurityOrigin& origin) {
  bool result = false;
  Send(new ViewHostMsg_AllowIndexedDB(
      routing_id(), origin.databaseIdentifier(), name, &result));
  return result;
}

bool ContentSettingsObserver::AllowPlugins(WebFrame* frame,
                                           bool enabled_per_settings) {
  return enabled_per_settings;
}

bool ContentSettingsObserver::AllowScript(WebFrame* frame,
                                          bool enabled_per_settings) {
  if (enabled_per_settings &&
      AllowContentType(CONTENT_SETTINGS_TYPE_JAVASCRIPT)) {
    return true;
  }

  if (IsWhitelistedForContentSettings(frame))
    return true;

  return false;  // Other protocols fall through here.
}

bool ContentSettingsObserver::AllowStorage(WebFrame* frame, bool local) {
  bool result = false;
  Send(new ViewHostMsg_AllowDOMStorage(
      routing_id(), frame->url(),
      local ? DOM_STORAGE_LOCAL : DOM_STORAGE_SESSION,
      &result));
  return result;
}

void ContentSettingsObserver::DidNotAllowPlugins(WebFrame* frame) {
  DidBlockContentType(CONTENT_SETTINGS_TYPE_PLUGINS, std::string());
}

void ContentSettingsObserver::DidNotAllowScript(WebFrame* frame) {
  DidBlockContentType(CONTENT_SETTINGS_TYPE_JAVASCRIPT, std::string());
}

void ContentSettingsObserver::OnSetContentSettingsForLoadingURL(
    const GURL& url,
    const ContentSettings& content_settings) {
  host_content_settings_[url] = content_settings;
}

void ContentSettingsObserver::OnLoadBlockedPlugins() {
  plugins_temporarily_allowed_ = true;
}

bool ContentSettingsObserver::AllowContentType(
    ContentSettingsType settings_type) {
  // CONTENT_SETTING_ASK is only valid for cookies.
  return current_content_settings_.settings[settings_type] !=
      CONTENT_SETTING_BLOCK;
}

void ContentSettingsObserver::ClearBlockedContentSettings() {
  for (size_t i = 0; i < arraysize(content_blocked_); ++i)
    content_blocked_[i] = false;
}
