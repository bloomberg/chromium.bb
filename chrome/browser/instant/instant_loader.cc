// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_loader.h"

#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/instant/instant_loader_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/thumbnail_generator.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper.h"
#include "chrome/browser/ui/constrained_window_tab_helper.h"
#include "chrome/browser/ui/constrained_window_tab_helper_delegate.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

// WebContentsDelegateImpl -----------------------------------------------------

class InstantLoader::WebContentsDelegateImpl
    : public ConstrainedWindowTabHelperDelegate,
      public CoreTabHelperDelegate,
      public content::WebContentsDelegate,
      public content::WebContentsObserver {
 public:
  explicit WebContentsDelegateImpl(InstantLoader* loader);

  bool is_pointer_down_from_activate() const {
    return is_pointer_down_from_activate_;
  }

  // ConstrainedWindowTabHelperDelegate:
  virtual bool ShouldFocusConstrainedWindow() OVERRIDE;

  // CoreTabHelperDelegate:
  virtual void SwapTabContents(TabContents* old_tc,
                               TabContents* new_tc) OVERRIDE;

  // content::WebContentsDelegate:
  virtual bool ShouldSuppressDialogs() OVERRIDE;
  virtual bool ShouldFocusPageAfterCrash() OVERRIDE;
  virtual void LostCapture() OVERRIDE;
  virtual void WebContentsFocused(content::WebContents* contents) OVERRIDE;
  virtual bool CanDownload(content::RenderViewHost* render_view_host,
                           int request_id,
                           const std::string& request_method) OVERRIDE;
  virtual void HandleMouseUp() OVERRIDE;
  virtual void HandlePointerActivate() OVERRIDE;
  virtual void HandleGestureBegin() OVERRIDE;
  virtual void HandleGestureEnd() OVERRIDE;
  virtual void DragEnded() OVERRIDE;
  virtual bool OnGoToEntryOffset(int offset) OVERRIDE;
  virtual bool ShouldAddNavigationToHistory(
      const history::HistoryAddPageArgs& add_page_args,
      content::NavigationType navigation_type) OVERRIDE;

  // content::WebContentsObserver:
  virtual void DidFinishLoad(
      int64 frame_id,
      const GURL& validated_url,
      bool is_main_frame,
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  // Message from renderer indicating the page has suggestions.
  void OnSetSuggestions(int page_id,
                        const std::vector<InstantSuggestion>& suggestions);

  // Message from the renderer determining whether it supports the Instant API.
  void OnInstantSupportDetermined(int page_id, bool result);

  void CommitFromPointerReleaseIfNecessary();
  void MaybeSetAndNotifyInstantSupportDetermined(bool supports_instant);

  InstantLoader* const loader_;

  // True if the mouse or a touch pointer is down from an activate.
  bool is_pointer_down_from_activate_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsDelegateImpl);
};

InstantLoader::WebContentsDelegateImpl::WebContentsDelegateImpl(
    InstantLoader* loader)
    : content::WebContentsObserver(loader->preview_contents_->web_contents()),
      loader_(loader),
      is_pointer_down_from_activate_(false) {
}

bool InstantLoader::WebContentsDelegateImpl::ShouldFocusConstrainedWindow() {
  // Return false so that constrained windows are not initially focused. If we
  // did otherwise the preview would prematurely get committed when focus goes
  // to the constrained window.
  return false;
}

void InstantLoader::WebContentsDelegateImpl::SwapTabContents(
    TabContents* old_tc,
    TabContents* new_tc) {
  // If this is being called, something is swapping in to our
  // |preview_contents_| before we've added it to the tab strip.
  loader_->ReplacePreviewContents(old_tc, new_tc);
}

bool InstantLoader::WebContentsDelegateImpl::ShouldSuppressDialogs() {
  // Any message shown during Instant cancels Instant, so we suppress them.
  return true;
}

bool InstantLoader::WebContentsDelegateImpl::ShouldFocusPageAfterCrash() {
  return false;
}

void InstantLoader::WebContentsDelegateImpl::LostCapture() {
  CommitFromPointerReleaseIfNecessary();
}

void InstantLoader::WebContentsDelegateImpl::WebContentsFocused(
    content::WebContents* contents) {
  loader_->loader_delegate_->InstantLoaderContentsFocused(loader_);
}

bool InstantLoader::WebContentsDelegateImpl::CanDownload(
    content::RenderViewHost* render_view_host,
    int request_id,
    const std::string& request_method) {
  // Downloads are disabled.
  return false;
}

void InstantLoader::WebContentsDelegateImpl::HandleMouseUp() {
  CommitFromPointerReleaseIfNecessary();
}

void InstantLoader::WebContentsDelegateImpl::HandlePointerActivate() {
  is_pointer_down_from_activate_ = true;
}

void InstantLoader::WebContentsDelegateImpl::HandleGestureBegin() {
}

void InstantLoader::WebContentsDelegateImpl::HandleGestureEnd() {
  CommitFromPointerReleaseIfNecessary();
}

void InstantLoader::WebContentsDelegateImpl::DragEnded() {
  // If the user drags, we won't get a mouse up (at least on Linux). Commit the
  // Instant result when the drag ends, so that during the drag the page won't
  // move around.
  CommitFromPointerReleaseIfNecessary();
}

bool InstantLoader::WebContentsDelegateImpl::OnGoToEntryOffset(int offset) {
  return false;
}

bool InstantLoader::WebContentsDelegateImpl::ShouldAddNavigationToHistory(
    const history::HistoryAddPageArgs& add_page_args,
    content::NavigationType navigation_type) {
  loader_->last_navigation_ = add_page_args.Clone();
  return false;
}

void InstantLoader::WebContentsDelegateImpl::DidFinishLoad(
    int64 frame_id,
    const GURL& validated_url,
    bool is_main_frame,
    content::RenderViewHost* render_view_host) {
  if (is_main_frame) {
    if (!loader_->supports_instant_)
      Send(new ChromeViewMsg_DetermineIfPageSupportsInstant(routing_id()));
    loader_->loader_delegate_->InstantLoaderPreviewLoaded(loader_);
  }
}

bool InstantLoader::WebContentsDelegateImpl::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebContentsDelegateImpl, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_SetSuggestions, OnSetSuggestions)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_InstantSupportDetermined,
                        OnInstantSupportDetermined)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void InstantLoader::WebContentsDelegateImpl::OnSetSuggestions(
    int page_id,
    const std::vector<InstantSuggestion>& suggestions) {
  DCHECK(loader_->preview_contents() &&
         loader_->preview_contents_->web_contents());
  // TODO(sreeram): Remove this 'if' bandaid once bug 141875 is confirmed fixed.
  if (!loader_->preview_contents() ||
      !loader_->preview_contents_->web_contents())
    return;
  content::NavigationEntry* entry = loader_->preview_contents_->web_contents()->
                                        GetController().GetActiveEntry();
  if (entry && page_id == entry->GetPageID()) {
    MaybeSetAndNotifyInstantSupportDetermined(true);
    loader_->loader_delegate_->SetSuggestions(loader_, suggestions);
  }
}

void InstantLoader::WebContentsDelegateImpl::OnInstantSupportDetermined(
    int page_id,
    bool result) {
  DCHECK(loader_->preview_contents() &&
         loader_->preview_contents_->web_contents());
  // TODO(sreeram): Remove this 'if' bandaid once bug 141875 is confirmed fixed.
  if (!loader_->preview_contents() ||
      !loader_->preview_contents_->web_contents())
    return;
  content::NavigationEntry* entry = loader_->preview_contents_->web_contents()->
                                        GetController().GetActiveEntry();
  if (entry && page_id == entry->GetPageID())
    MaybeSetAndNotifyInstantSupportDetermined(result);
}


void InstantLoader::WebContentsDelegateImpl
    ::CommitFromPointerReleaseIfNecessary() {
  if (is_pointer_down_from_activate_) {
    is_pointer_down_from_activate_ = false;
    loader_->loader_delegate_->CommitInstantLoader(loader_);
  }
}

void InstantLoader::WebContentsDelegateImpl
    ::MaybeSetAndNotifyInstantSupportDetermined(bool supports_instant) {
  // If we already determined that the loader supports Instant, nothing to do.
  if (loader_->supports_instant_)
    return;

  // If the page doesn't support the Instant API, InstantController schedules
  // the loader for destruction. Stop sending the controller any more messages,
  // by severing the connection from the WebContents to us (its delegate).
  if (!supports_instant) {
    loader_->preview_contents_->web_contents()->SetDelegate(NULL);
    Observe(NULL);
  }

  loader_->supports_instant_ = supports_instant;
  loader_->loader_delegate_->InstantSupportDetermined(loader_,
                                                      supports_instant);
}

// InstantLoader ---------------------------------------------------------------

InstantLoader::InstantLoader(InstantLoaderDelegate* delegate,
                             const std::string& instant_url,
                             const TabContents* tab_contents)
    : loader_delegate_(delegate),
      preview_contents_(
          TabContents::Factory::CreateTabContents(
              content::WebContents::CreateWithSessionStorage(
                  tab_contents->profile(), NULL, MSG_ROUTING_NONE,
                  tab_contents->web_contents(),
                  tab_contents->web_contents()->GetController().
                                          GetSessionStorageNamespaceMap()))),
      supports_instant_(false),
      instant_url_(instant_url) {
}

InstantLoader::~InstantLoader() {
  if (preview_contents())
    preview_contents_->web_contents()->SetDelegate(NULL);
}

void InstantLoader::Init() {
  SetupPreviewContents();
  // This HTTP header and value are set on loads that originate from instant.
  const char* const kInstantHeader = "X-Purpose: Instant";
  preview_contents_->web_contents()->GetController().LoadURL(GURL(instant_url_),
      content::Referrer(), content::PAGE_TRANSITION_GENERATED, kInstantHeader);
  preview_contents_->web_contents()->WasHidden();
}

void InstantLoader::Update(const string16& user_text, bool verbatim) {
  // TODO: Support real cursor position.
  last_navigation_ = NULL;
  content::RenderViewHost* rvh =
      preview_contents_->web_contents()->GetRenderViewHost();
  rvh->Send(new ChromeViewMsg_SearchBoxChange(rvh->GetRoutingID(), user_text,
                    verbatim, user_text.size(), user_text.size()));
}

void InstantLoader::SetOmniboxBounds(const gfx::Rect& bounds) {
  content::RenderViewHost* rvh =
      preview_contents_->web_contents()->GetRenderViewHost();
  rvh->Send(new ChromeViewMsg_SearchBoxResize(rvh->GetRoutingID(), bounds));
}

void InstantLoader::SendAutocompleteResults(
    const std::vector<InstantAutocompleteResult>& results) {
  content::RenderViewHost* rvh =
      preview_contents_->web_contents()->GetRenderViewHost();
  rvh->Send(new ChromeViewMsg_SearchBoxAutocompleteResults(rvh->GetRoutingID(),
                                                           results));
}

TabContents* InstantLoader::ReleasePreviewContents(InstantCommitType type,
                                                   const string16& text) {
  content::RenderViewHost* rvh =
      preview_contents_->web_contents()->GetRenderViewHost();
  if (type == INSTANT_COMMIT_PRESSED_ENTER)
    rvh->Send(new ChromeViewMsg_SearchBoxSubmit(rvh->GetRoutingID(), text));
  else
    rvh->Send(new ChromeViewMsg_SearchBoxCancel(rvh->GetRoutingID(), text));
  CleanupPreviewContents();
  return preview_contents_.release();
}

bool InstantLoader::IsPointerDownFromActivate() const {
  return preview_delegate_->is_pointer_down_from_activate();
}

void InstantLoader::Observe(int type,
                            const content::NotificationSource& source,
                            const content::NotificationDetails& details) {
#if defined(OS_MACOSX)
  if (type == content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED) {
    if (content::RenderWidgetHostView* rwhv =
            preview_contents_->web_contents()->GetRenderWidgetHostView())
      rwhv->SetTakesFocusOnlyOnMouseDown(true);
    return;
  }
  NOTREACHED();
#endif
}

void InstantLoader::SetupPreviewContents() {
  content::WebContents* new_contents = preview_contents_->web_contents();
  preview_delegate_.reset(new WebContentsDelegateImpl(this));
  WebContentsDelegateImpl* new_delegate = preview_delegate_.get();
  new_contents->SetDelegate(new_delegate);

  // Disable popups and such (mainly to avoid losing focus and reverting the
  // preview prematurely).
  preview_contents_->blocked_content_tab_helper()->SetAllContentsBlocked(true);
  preview_contents_->constrained_window_tab_helper()->set_delegate(
                                                          new_delegate);
  preview_contents_->content_settings()->SetPopupsBlocked(true);
  preview_contents_->core_tab_helper()->set_delegate(new_delegate);
  if (ThumbnailGenerator* tg = preview_contents_->thumbnail_generator())
    tg->set_enabled(false);

#if defined(OS_MACOSX)
  // If |preview_contents_| does not currently have a RWHV, we will call
  // SetTakesFocusOnlyOnMouseDown() as a result of the RENDER_VIEW_HOST_CHANGED
  // notification.
  if (content::RenderWidgetHostView* rwhv =
          new_contents->GetRenderWidgetHostView())
    rwhv->SetTakesFocusOnlyOnMouseDown(true);
  registrar_.Add(this, content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED,
                 content::Source<content::NavigationController>(
                     &new_contents->GetController()));
#endif
}

void InstantLoader::CleanupPreviewContents() {
  content::WebContents* old_contents = preview_contents_->web_contents();
  old_contents->SetDelegate(NULL);
  preview_delegate_.reset();

  preview_contents_->blocked_content_tab_helper()->SetAllContentsBlocked(false);
  preview_contents_->constrained_window_tab_helper()->set_delegate(NULL);
  preview_contents_->content_settings()->SetPopupsBlocked(false);
  preview_contents_->core_tab_helper()->set_delegate(NULL);
  if (ThumbnailGenerator* tg = preview_contents_->thumbnail_generator())
    tg->set_enabled(true);

#if defined(OS_MACOSX)
  if (content::RenderWidgetHostView* rwhv =
          old_contents->GetRenderWidgetHostView())
    rwhv->SetTakesFocusOnlyOnMouseDown(false);
  registrar_.Remove(this, content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED,
                    content::Source<content::NavigationController>(
                        &old_contents->GetController()));
#endif
}

void InstantLoader::ReplacePreviewContents(TabContents* old_tc,
                                           TabContents* new_tc) {
  DCHECK(old_tc == preview_contents_);
  CleanupPreviewContents();
  // We release here without deleting so that the caller still has the
  // responsibility for deleting the TabContents.
  ignore_result(preview_contents_.release());
  preview_contents_.reset(new_tc);
  SetupPreviewContents();
  loader_delegate_->SwappedTabContents(this);
}
