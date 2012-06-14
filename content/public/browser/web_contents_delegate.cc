// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_contents_delegate.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_intents_dispatcher.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/bindings_policy.h"
#include "ui/gfx/rect.h"
#include "webkit/glue/web_intent_data.h"

namespace content {

WebContentsDelegate::WebContentsDelegate() {
}

WebContents* WebContentsDelegate::OpenURLFromTab(WebContents* source,
                                                 const OpenURLParams& params) {
  return NULL;
}

bool WebContentsDelegate::IsPopupOrPanel(const WebContents* source) const {
  return false;
}

bool WebContentsDelegate::IsApplication() const { return false; }

bool WebContentsDelegate::CanLoadDataURLsInWebUI() const { return false; }

gfx::Rect WebContentsDelegate::GetRootWindowResizerRect() const {
  return gfx::Rect();
}

bool WebContentsDelegate::ShouldSuppressDialogs() {
  return false;
}

bool WebContentsDelegate::AddMessageToConsole(WebContents* source,
                                              int32 level,
                                              const string16& message,
                                              int32 line_no,
                                              const string16& source_id) {
  return false;
}

void WebContentsDelegate::BeforeUnloadFired(WebContents* web_contents,
                                            bool proceed,
                                            bool* proceed_to_fire_unload) {
  *proceed_to_fire_unload = true;
}

bool WebContentsDelegate::ShouldFocusPageAfterCrash() {
  return true;
}

bool WebContentsDelegate::TakeFocus(bool reverse) {
  return false;
}

int WebContentsDelegate::GetExtraRenderViewHeight() const {
  return 0;
}

bool WebContentsDelegate::CanDownload(RenderViewHost* render_view_host,
                                      int request_id,
                                      const std::string& request_method) {
  return true;
}

bool WebContentsDelegate::HandleContextMenu(
    const content::ContextMenuParams& params) {
  return false;
}

bool WebContentsDelegate::ExecuteContextMenuCommand(int command) {
  return false;
}

void WebContentsDelegate::ViewSourceForTab(WebContents* source,
                                           const GURL& page_url) {
  // Fall back implementation based entirely on the view-source scheme.
  // It suffers from http://crbug.com/523 and that is why browser overrides
  // it with proper implementation.
  GURL url = GURL(chrome::kViewSourceScheme + std::string(":") +
                      page_url.spec());
  OpenURLFromTab(source, OpenURLParams(url, Referrer(),
                                       NEW_FOREGROUND_TAB,
                                       PAGE_TRANSITION_LINK, false));
}

void WebContentsDelegate::ViewSourceForFrame(WebContents* source,
                                             const GURL& frame_url,
                                             const std::string& content_state) {
  // Same as ViewSourceForTab, but for given subframe.
  GURL url = GURL(chrome::kViewSourceScheme + std::string(":") +
                      frame_url.spec());
  OpenURLFromTab(source, OpenURLParams(url, Referrer(),
                                       NEW_FOREGROUND_TAB,
                                       PAGE_TRANSITION_LINK, false));
}

bool WebContentsDelegate::PreHandleKeyboardEvent(
    const NativeWebKeyboardEvent& event,
    bool* is_keyboard_shortcut) {
  return false;
}

bool WebContentsDelegate::OnGoToEntryOffset(int offset) {
  return true;
}

bool WebContentsDelegate::ShouldAddNavigationToHistory(
    const history::HistoryAddPageArgs& add_page_args,
    NavigationType navigation_type) {
  return true;
}

gfx::NativeWindow WebContentsDelegate::GetFrameNativeWindow() {
  return NULL;
}

bool WebContentsDelegate::ShouldCreateWebContents(
    WebContents* web_contents,
    int route_id,
    WindowContainerType window_container_type,
    const string16& frame_name,
    const GURL& target_url) {
  return true;
}

JavaScriptDialogCreator* WebContentsDelegate::GetJavaScriptDialogCreator() {
  return NULL;
}

bool WebContentsDelegate::IsFullscreenForTabOrPending(
    const WebContents* web_contents) const {
  return false;
}

content::ColorChooser* WebContentsDelegate::OpenColorChooser(
    WebContents* web_contents,
    int color_chooser_id,
    SkColor color) {
  return NULL;
}

void WebContentsDelegate::WebIntentDispatch(
    WebContents* web_contents,
    WebIntentsDispatcher* intents_dispatcher) {
  // The caller passes this method ownership of the |intents_dispatcher|, but
  // this empty implementation will not use it, so we delete it immediately.
  delete intents_dispatcher;
}

WebContentsDelegate::~WebContentsDelegate() {
  while (!attached_contents_.empty()) {
    WebContents* web_contents = *attached_contents_.begin();
    web_contents->SetDelegate(NULL);
  }
  DCHECK(attached_contents_.empty());
}

void WebContentsDelegate::Attach(WebContents* web_contents) {
  DCHECK(attached_contents_.find(web_contents) == attached_contents_.end());
  attached_contents_.insert(web_contents);
}

void WebContentsDelegate::Detach(WebContents* web_contents) {
  DCHECK(attached_contents_.find(web_contents) != attached_contents_.end());
  attached_contents_.erase(web_contents);
}

}  // namespace content
