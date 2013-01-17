// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_loader.h"

#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/safe_browsing/safe_browsing_tab_observer.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper_delegate.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_view.h"
#include "ipc/ipc_message.h"

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
    : public CoreTabHelperDelegate,
      public content::WebContentsDelegate {
 public:
  explicit WebContentsDelegateImpl(InstantLoader* loader);

 private:
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
  virtual content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) OVERRIDE;

  void MaybeCommitFromPointerRelease();

  InstantLoader* const loader_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsDelegateImpl);
};

InstantLoader::WebContentsDelegateImpl::WebContentsDelegateImpl(
    InstantLoader* loader)
    : loader_(loader) {
}

void InstantLoader::WebContentsDelegateImpl::SwapTabContents(
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  // If this is being called, something is swapping in to loader's |contents_|
  // before we've added it to the tab strip.
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
  MaybeCommitFromPointerRelease();
}

void InstantLoader::WebContentsDelegateImpl::WebContentsFocused(
    content::WebContents* /* contents */) {
  // The preview is getting focus. Equivalent to it being clicked.
  bool tmp = loader_->is_pointer_down_from_activate_;
  loader_->is_pointer_down_from_activate_ = true;
  loader_->controller_->InstantLoaderContentsFocused();
  loader_->is_pointer_down_from_activate_ = tmp;
}

bool InstantLoader::WebContentsDelegateImpl::CanDownload(
    content::RenderViewHost* /* render_view_host */,
    int /* request_id */,
    const std::string& /* request_method */) {
  // Downloads are disabled.
  return false;
}

void InstantLoader::WebContentsDelegateImpl::HandleMouseDown() {
  loader_->is_pointer_down_from_activate_ = true;
}

void InstantLoader::WebContentsDelegateImpl::HandleMouseUp() {
  MaybeCommitFromPointerRelease();
}

void InstantLoader::WebContentsDelegateImpl::HandlePointerActivate() {
  loader_->is_pointer_down_from_activate_ = true;
}

void InstantLoader::WebContentsDelegateImpl::HandleGestureEnd() {
  MaybeCommitFromPointerRelease();
}

void InstantLoader::WebContentsDelegateImpl::DragEnded() {
  // If the user drags, we won't get a mouse up (at least on Linux). Commit the
  // Instant result when the drag ends, so that during the drag the page won't
  // move around.
  MaybeCommitFromPointerRelease();
}

bool InstantLoader::WebContentsDelegateImpl::OnGoToEntryOffset(int offset) {
  return false;
}

content::WebContents* InstantLoader::WebContentsDelegateImpl::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  content::WebContents* preview = loader_->contents_.get();
  if (loader_->controller_->CommitIfPossible(INSTANT_COMMIT_NAVIGATED))
    return preview->GetDelegate()->OpenURLFromTab(source, params);
  return NULL;
}

void InstantLoader::WebContentsDelegateImpl::MaybeCommitFromPointerRelease() {
  if (loader_->is_pointer_down_from_activate_) {
    loader_->is_pointer_down_from_activate_ = false;
    loader_->controller_->CommitIfPossible(INSTANT_COMMIT_FOCUS_LOST);
  }
}

// InstantLoader ---------------------------------------------------------------

// static
InstantLoader* InstantLoader::FromWebContents(
    const content::WebContents* web_contents) {
  InstantLoaderUserData* data = static_cast<InstantLoaderUserData*>(
      web_contents->GetUserData(&kUserDataKey));
  return data ? data->loader() : NULL;
}

InstantLoader::InstantLoader(InstantController* controller,
                             const std::string& instant_url)
    : client_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      controller_(controller),
      delegate_(new WebContentsDelegateImpl(
                        ALLOW_THIS_IN_INITIALIZER_LIST(this))),
      instant_url_(instant_url),
      supports_instant_(false),
      is_pointer_down_from_activate_(false) {
}

InstantLoader::~InstantLoader() {
}

void InstantLoader::InitContents(const content::WebContents* active_tab) {
  content::WebContents::CreateParams create_params(
      active_tab->GetBrowserContext());
  if (active_tab)
    create_params.initial_size = active_tab->GetView()->GetContainerSize();
  contents_.reset(content::WebContents::CreateWithSessionStorage(
      create_params,
      active_tab->GetController().GetSessionStorageNamespaceMap()));
  SetupPreviewContents();

  // This HTTP header and value are set on loads that originate from Instant.
  const char kInstantHeader[] = "X-Purpose: Instant";
  DVLOG(1) << "LoadURL: " << instant_url_;
  contents_->GetController().LoadURL(GURL(instant_url_), content::Referrer(),
      content::PAGE_TRANSITION_GENERATED, kInstantHeader);
  contents_->WasHidden();
}

content::WebContents* InstantLoader::ReleaseContents() {
  CleanupPreviewContents();
  return contents_.release();
}

void InstantLoader::DidNavigate(
    const history::HistoryAddPageArgs& add_page_args) {
  last_navigation_ = add_page_args;
}

bool InstantLoader::IsUsingLocalPreview() const {
  return instant_url_ == InstantController::kLocalOmniboxPopupURL;
}

void InstantLoader::Update(const string16& text,
                           size_t selection_start,
                           size_t selection_end,
                           bool verbatim) {
  last_navigation_ = history::HistoryAddPageArgs();
  client_.Update(text, selection_start, selection_end, verbatim);
}

void InstantLoader::Submit(const string16& text) {
  client_.Submit(text);
}

void InstantLoader::Cancel(const string16& text) {
  client_.Cancel(text);
}

void InstantLoader::SetPopupBounds(const gfx::Rect& bounds) {
  client_.SetPopupBounds(bounds);
}

void InstantLoader::SetMarginSize(int start, int end) {
  client_.SetMarginSize(start, end);
}

void InstantLoader::SendAutocompleteResults(
    const std::vector<InstantAutocompleteResult>& results) {
  client_.SendAutocompleteResults(results);
}

void InstantLoader::UpOrDownKeyPressed(int count) {
  client_.UpOrDownKeyPressed(count);
}

void InstantLoader::SearchModeChanged(const chrome::search::Mode& mode) {
  client_.SearchModeChanged(mode);
}

void InstantLoader::SendThemeBackgroundInfo(
    const ThemeBackgroundInfo& theme_info) {
  client_.SendThemeBackgroundInfo(theme_info);
}

void InstantLoader::SendThemeAreaHeight(int height) {
  client_.SendThemeAreaHeight(height);
}

void InstantLoader::SetDisplayInstantResults(bool display_instant_results) {
  client_.SetDisplayInstantResults(display_instant_results);
}

void InstantLoader::KeyCaptureChanged(bool is_key_capture_enabled) {
  client_.KeyCaptureChanged(is_key_capture_enabled);
}

void InstantLoader::SetSuggestions(
    const std::vector<InstantSuggestion>& suggestions) {
  InstantSupportDetermined(true);
  controller_->SetSuggestions(contents(), suggestions);
}

void InstantLoader::InstantSupportDetermined(bool supports_instant) {
  // If we had already determined that the page supports Instant, nothing to do.
  if (supports_instant_)
    return;

  supports_instant_ = supports_instant;
  controller_->InstantSupportDetermined(contents(), supports_instant);
}

void InstantLoader::ShowInstantPreview(InstantShownReason reason,
                                       int height,
                                       InstantSizeUnits units) {
  InstantSupportDetermined(true);
  controller_->ShowInstantPreview(reason, height, units);
}

void InstantLoader::StartCapturingKeyStrokes() {
  InstantSupportDetermined(true);
  controller_->StartCapturingKeyStrokes();
}

void InstantLoader::StopCapturingKeyStrokes() {
  InstantSupportDetermined(true);
  controller_->StopCapturingKeyStrokes();
}

void InstantLoader::RenderViewGone() {
  controller_->InstantLoaderRenderViewGone();
}

void InstantLoader::AboutToNavigateMainFrame(const GURL& url) {
  controller_->InstantLoaderAboutToNavigateMainFrame(url);
}

void InstantLoader::NavigateToURL(const GURL& url,
                                  content::PageTransition transition) {
  InstantSupportDetermined(true);
  controller_->NavigateToURL(url, transition);
}

void InstantLoader::Observe(int type,
                            const content::NotificationSource& source,
                            const content::NotificationDetails& details) {
#if defined(OS_MACOSX)
  if (type == content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED) {
    if (content::RenderWidgetHostView* rwhv =
            contents_->GetRenderWidgetHostView())
      rwhv->SetTakesFocusOnlyOnMouseDown(true);
    return;
  }
  NOTREACHED();
#endif
}

void InstantLoader::SetupPreviewContents() {
  client_.SetContents(contents());
  contents_->SetUserData(&kUserDataKey, new InstantLoaderUserData(this));
  contents_->SetDelegate(delegate_.get());

  // Set up various tab helpers. The rest will get attached when (if) the
  // contents is added to the tab strip.

  // Tab helpers to control popups.
  BlockedContentTabHelper::CreateForWebContents(contents());
  BlockedContentTabHelper::FromWebContents(contents())->
      SetAllContentsBlocked(true);
  TabSpecificContentSettings::CreateForWebContents(contents());
  TabSpecificContentSettings::FromWebContents(contents())->
      SetPopupsBlocked(true);

  // A tab helper to catch prerender content swapping shenanigans.
  CoreTabHelper::CreateForWebContents(contents());
  CoreTabHelper::FromWebContents(contents())->set_delegate(delegate_.get());

  // Tab helpers used when committing a preview.
  chrome::search::SearchTabHelper::CreateForWebContents(contents());
  HistoryTabHelper::CreateForWebContents(contents());

  // Observers.
  extensions::WebNavigationTabObserver::CreateForWebContents(contents());

  // Favicons, required by the Task Manager.
  FaviconTabHelper::CreateForWebContents(contents());

  // And some flat-out paranoia.
  safe_browsing::SafeBrowsingTabObserver::CreateForWebContents(contents());

#if defined(OS_MACOSX)
  // If |contents_| doesn't yet have a RWHV, SetTakesFocusOnlyOnMouseDown() will
  // be called later, when NOTIFICATION_RENDER_VIEW_HOST_CHANGED is received.
  if (content::RenderWidgetHostView* rwhv =
          contents_->GetRenderWidgetHostView())
    rwhv->SetTakesFocusOnlyOnMouseDown(true);
  registrar_.Add(this, content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED,
                 content::Source<content::NavigationController>(
                     &contents_->GetController()));
#endif
}

void InstantLoader::CleanupPreviewContents() {
  client_.SetContents(NULL);
  contents_->RemoveUserData(&kUserDataKey);
  contents_->SetDelegate(NULL);

  // Undo tab helper work done in SetupPreviewContents().

  BlockedContentTabHelper::FromWebContents(contents())->
      SetAllContentsBlocked(false);
  TabSpecificContentSettings::FromWebContents(contents())->
      SetPopupsBlocked(false);

  CoreTabHelper::FromWebContents(contents())->set_delegate(NULL);

#if defined(OS_MACOSX)
  if (content::RenderWidgetHostView* rwhv =
          contents_->GetRenderWidgetHostView())
    rwhv->SetTakesFocusOnlyOnMouseDown(false);
  registrar_.Remove(this, content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED,
                    content::Source<content::NavigationController>(
                        &contents_->GetController()));
#endif
}

void InstantLoader::ReplacePreviewContents(content::WebContents* old_contents,
                                           content::WebContents* new_contents) {
  DCHECK_EQ(old_contents, contents());
  CleanupPreviewContents();
  // We release here without deleting so that the caller still has the
  // responsibility for deleting the WebContents.
  ignore_result(contents_.release());
  contents_.reset(new_contents);
  SetupPreviewContents();
  controller_->SwappedWebContents();
}
