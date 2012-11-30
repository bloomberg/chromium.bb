// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_loader.h"

#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/thumbnails/thumbnail_tab_helper.h"
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
#include "content/public/browser/web_contents_delegate.h"

namespace {

int kUserDataKey;

class InstantLoaderUserData : public base::SupportsUserData::Data {
 public:
  explicit InstantLoaderUserData(InstantLoader* loader) : loader_(loader) {}

  InstantLoader* loader() const { return loader_; }

 private:
  ~InstantLoaderUserData() {}

  InstantLoader* const loader_;

  DISALLOW_COPY_AND_ASSIGN(InstantLoaderUserData);
};

}

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

  // Start observing |web_contents| instead of whatever is currently being
  // observed. If |web_contents| is NULL, effectively stops observing.
  void ObserveContents(content::WebContents* web_contents);

 private:
  // Overridden from ConstrainedWindowTabHelperDelegate:
  virtual bool ShouldFocusConstrainedWindow() OVERRIDE;

  // Overridden from CoreTabHelperDelegate:
  virtual void SwapTabContents(content::WebContents* old_contents,
                               content::WebContents* new_contents) OVERRIDE;

  // Overridden from content::WebContentsDelegate:
  virtual bool ShouldSuppressDialogs() OVERRIDE;
  virtual bool ShouldFocusPageAfterCrash() OVERRIDE;
  virtual void LostCapture() OVERRIDE;
  virtual void WebContentsFocused(content::WebContents* contents) OVERRIDE;
  virtual bool CanDownload(content::RenderViewHost* render_view_host,
                           int request_id,
                           const std::string& request_method) OVERRIDE;
  virtual void HandleMouseDown() OVERRIDE;
  virtual void HandleMouseUp() OVERRIDE;
  virtual void HandlePointerActivate() OVERRIDE;
  virtual void HandleGestureEnd() OVERRIDE;
  virtual void DragEnded() OVERRIDE;
  virtual bool OnGoToEntryOffset(int offset) OVERRIDE;

  // Overridden from content::WebContentsObserver:
  virtual void DidFinishLoad(
      int64 frame_id,
      const GURL& validated_url,
      bool is_main_frame,
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Message from renderer indicating the page has suggestions.
  void OnSetSuggestions(int page_id,
                        const std::vector<InstantSuggestion>& suggestions);

  // Message from the renderer determining whether it supports the Instant API.
  void OnInstantSupportDetermined(int page_id, bool result);

  // Message from the renderer requesting the preview be shown.
  void OnShowInstantPreview(int page_id,
                            InstantShownReason reason,
                            int height,
                            InstantSizeUnits units);

  void CommitFromPointerReleaseIfNecessary();
  void MaybeSetAndNotifyInstantSupportDetermined(bool supports_instant);

  InstantLoader* const loader_;

  // True if the mouse or a touch pointer is down from an activate.
  bool is_pointer_down_from_activate_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsDelegateImpl);
};

InstantLoader::WebContentsDelegateImpl::WebContentsDelegateImpl(
    InstantLoader* loader)
    : loader_(loader),
      is_pointer_down_from_activate_(false) {
}

void InstantLoader::WebContentsDelegateImpl::ObserveContents(
    content::WebContents* web_contents) {
  Observe(web_contents);
}

bool InstantLoader::WebContentsDelegateImpl::ShouldFocusConstrainedWindow() {
  // Return false so that constrained windows are not initially focused. If we
  // did otherwise the preview would prematurely get committed when focus goes
  // to the constrained window.
  return false;
}

void InstantLoader::WebContentsDelegateImpl::SwapTabContents(
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  // If this is being called, something is swapping in to our
  // |preview_contents_| before we've added it to the tab strip.
  loader_->ReplacePreviewContents(old_contents, new_contents);
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
  loader_->controller_->InstantLoaderContentsFocused(loader_);
}

bool InstantLoader::WebContentsDelegateImpl::CanDownload(
    content::RenderViewHost* render_view_host,
    int request_id,
    const std::string& request_method) {
  // Downloads are disabled.
  return false;
}

void InstantLoader::WebContentsDelegateImpl::HandleMouseDown() {
  is_pointer_down_from_activate_ = true;
}

void InstantLoader::WebContentsDelegateImpl::HandleMouseUp() {
  CommitFromPointerReleaseIfNecessary();
}

void InstantLoader::WebContentsDelegateImpl::HandlePointerActivate() {
  is_pointer_down_from_activate_ = true;
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

void InstantLoader::WebContentsDelegateImpl::DidFinishLoad(
    int64 frame_id,
    const GURL& validated_url,
    bool is_main_frame,
    content::RenderViewHost* render_view_host) {
  if (is_main_frame && !loader_->supports_instant_)
    Send(new ChromeViewMsg_DetermineIfPageSupportsInstant(routing_id()));
}

bool InstantLoader::WebContentsDelegateImpl::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebContentsDelegateImpl, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_SetSuggestions, OnSetSuggestions)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_InstantSupportDetermined,
                        OnInstantSupportDetermined)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_ShowInstantPreview,
                        OnShowInstantPreview);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void InstantLoader::WebContentsDelegateImpl::OnSetSuggestions(
    int page_id,
    const std::vector<InstantSuggestion>& suggestions) {
  DCHECK(loader_->preview_contents());
  DCHECK(loader_->preview_contents_->web_contents());
  content::NavigationEntry* entry = loader_->preview_contents_->web_contents()->
                                        GetController().GetActiveEntry();
  if (entry && page_id == entry->GetPageID()) {
    MaybeSetAndNotifyInstantSupportDetermined(true);
    loader_->controller_->SetSuggestions(loader_, suggestions);
  }
}

void InstantLoader::WebContentsDelegateImpl::OnInstantSupportDetermined(
    int page_id,
    bool result) {
  DCHECK(loader_->preview_contents());
  DCHECK(loader_->preview_contents_->web_contents());
  content::NavigationEntry* entry = loader_->preview_contents_->web_contents()->
                                        GetController().GetActiveEntry();
  if (entry && page_id == entry->GetPageID())
    MaybeSetAndNotifyInstantSupportDetermined(result);
}

void InstantLoader::WebContentsDelegateImpl::OnShowInstantPreview(
    int page_id,
    InstantShownReason reason,
    int height,
    InstantSizeUnits units) {
  DCHECK(loader_->preview_contents());
  DCHECK(loader_->preview_contents_->web_contents());
  content::NavigationEntry* entry = loader_->preview_contents_->web_contents()->
                                        GetController().GetActiveEntry();
  if (entry && page_id == entry->GetPageID()) {
    MaybeSetAndNotifyInstantSupportDetermined(true);
    loader_->controller_->ShowInstantPreview(loader_, reason, height, units);
  }
}

void InstantLoader::WebContentsDelegateImpl
    ::CommitFromPointerReleaseIfNecessary() {
  if (is_pointer_down_from_activate_) {
    is_pointer_down_from_activate_ = false;
    loader_->controller_->CommitInstantLoader(loader_);
  }
}

void InstantLoader::WebContentsDelegateImpl
    ::MaybeSetAndNotifyInstantSupportDetermined(bool supports_instant) {
  // If we already determined that the loader supports Instant, nothing to do.
  if (!loader_->supports_instant_) {
    loader_->supports_instant_ = supports_instant;
    loader_->controller_->InstantSupportDetermined(loader_, supports_instant);
  }
}

// InstantLoader ---------------------------------------------------------------

// static
InstantLoader* InstantLoader::FromWebContents(
    content::WebContents* web_contents) {
  InstantLoaderUserData* data = static_cast<InstantLoaderUserData*>(
      web_contents->GetUserData(&kUserDataKey));
  return data ? data->loader() : NULL;
}

InstantLoader::InstantLoader(InstantController* controller,
                             const std::string& instant_url,
                             const TabContents* tab_contents)
    : controller_(controller),
      preview_delegate_(new WebContentsDelegateImpl(
          ALLOW_THIS_IN_INITIALIZER_LIST(this))),
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
}

void InstantLoader::Init() {
  SetupPreviewContents();
  // This HTTP header and value are set on loads that originate from instant.
  const char kInstantHeader[] = "X-Purpose: Instant";
  DVLOG(1) << "LoadURL: " << instant_url_;
  preview_contents_->web_contents()->GetController().LoadURL(GURL(instant_url_),
      content::Referrer(), content::PAGE_TRANSITION_GENERATED, kInstantHeader);
  preview_contents_->web_contents()->WasHidden();
}

void InstantLoader::Update(const string16& user_text, bool verbatim) {
  // TODO: Support real cursor position.
  last_navigation_ = history::HistoryAddPageArgs();
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

void InstantLoader::SendThemeBackgroundInfo(
      const ThemeBackgroundInfo& theme_info) {
  content::RenderViewHost* rvh =
      preview_contents_->web_contents()->GetRenderViewHost();
  rvh->Send(new ChromeViewMsg_SearchBoxThemeChanged(rvh->GetRoutingID(),
                                                    theme_info));
}

void InstantLoader::SendThemeAreaHeight(int height) {
  content::RenderViewHost* rvh =
      preview_contents_->web_contents()->GetRenderViewHost();
  rvh->Send(new ChromeViewMsg_SearchBoxThemeAreaHeightChanged(
      rvh->GetRoutingID(), height));
}

void InstantLoader::SetDisplayInstantResults(bool display_instant_results) {
  content::RenderViewHost* rvh =
      preview_contents_->web_contents()->GetRenderViewHost();
  rvh->Send(new ChromeViewMsg_SearchBoxSetDisplayInstantResults(
      rvh->GetRoutingID(), display_instant_results));
}

void InstantLoader::OnUpOrDownKeyPressed(int count) {
  content::RenderViewHost* rvh =
      preview_contents_->web_contents()->GetRenderViewHost();
  rvh->Send(new ChromeViewMsg_SearchBoxUpOrDownKeyPressed(rvh->GetRoutingID(),
                                                          count));
}

void InstantLoader::SearchModeChanged(const chrome::search::Mode& mode) {
  content::RenderViewHost* rvh =
      preview_contents_->web_contents()->GetRenderViewHost();
  rvh->Send(new ChromeViewMsg_SearchBoxModeChanged(rvh->GetRoutingID(), mode));
}

void InstantLoader::DidNavigate(
    const history::HistoryAddPageArgs& add_page_args) {
  last_navigation_ = add_page_args;
}

TabContents* InstantLoader::ReleasePreviewContents(InstantCommitType type,
                                                   const string16& text) {
  content::RenderViewHost* rvh =
      preview_contents_->web_contents()->GetRenderViewHost();
  if (type == INSTANT_COMMIT_FOCUS_LOST)
    rvh->Send(new ChromeViewMsg_SearchBoxCancel(rvh->GetRoutingID(), text));
  else
    rvh->Send(new ChromeViewMsg_SearchBoxSubmit(rvh->GetRoutingID(), text));
  CleanupPreviewContents();
  return preview_contents_.release();
}

void InstantLoader::CleanupPreviewContents() {
  content::WebContents* old_contents = preview_contents_->web_contents();
  old_contents->RemoveUserData(&kUserDataKey);
  old_contents->SetDelegate(NULL);
  preview_delegate_->ObserveContents(NULL);

  BlockedContentTabHelper::FromWebContents(old_contents)->
      SetAllContentsBlocked(false);
  ConstrainedWindowTabHelper::FromWebContents(old_contents)->set_delegate(NULL);
  TabSpecificContentSettings::FromWebContents(old_contents)->
      SetPopupsBlocked(false);
  CoreTabHelper::FromWebContents(old_contents)->set_delegate(NULL);
  if (ThumbnailTabHelper* thumbnail_tab_helper =
          ThumbnailTabHelper::FromWebContents(old_contents))
    thumbnail_tab_helper->set_enabled(true);

#if defined(OS_MACOSX)
  if (content::RenderWidgetHostView* rwhv =
          old_contents->GetRenderWidgetHostView())
    rwhv->SetTakesFocusOnlyOnMouseDown(false);
  registrar_.Remove(this, content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED,
                    content::Source<content::NavigationController>(
                        &old_contents->GetController()));
#endif
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
  new_contents->SetUserData(&kUserDataKey, new InstantLoaderUserData(this));
  WebContentsDelegateImpl* new_delegate = preview_delegate_.get();
  new_contents->SetDelegate(new_delegate);
  new_delegate->ObserveContents(new_contents);

  // Disable popups and such (mainly to avoid losing focus and reverting the
  // preview prematurely).
  BlockedContentTabHelper::FromWebContents(new_contents)->
      SetAllContentsBlocked(true);
  ConstrainedWindowTabHelper::FromWebContents(new_contents)->
      set_delegate(new_delegate);
  TabSpecificContentSettings::FromWebContents(new_contents)->
      SetPopupsBlocked(true);
  CoreTabHelper::FromWebContents(new_contents)->set_delegate(new_delegate);
  if (ThumbnailTabHelper* thumbnail_tab_helper =
          ThumbnailTabHelper::FromWebContents(new_contents))
    thumbnail_tab_helper->set_enabled(false);

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

void InstantLoader::ReplacePreviewContents(content::WebContents* old_contents,
                                           content::WebContents* new_contents) {
  DCHECK_EQ(old_contents, preview_contents_->web_contents());
  CleanupPreviewContents();
  // We release here without deleting so that the caller still has the
  // responsibility for deleting the TabContents.
  ignore_result(preview_contents_.release());
  preview_contents_.reset(TabContents::FromWebContents(new_contents));
  SetupPreviewContents();
  controller_->SwappedTabContents(this);
}
