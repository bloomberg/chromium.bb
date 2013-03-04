// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_overlay.h"

#include "base/auto_reset.h"
#include "base/supports_user_data.h"
#include "chrome/browser/ui/search/search.h"
#include "content/public/browser/web_contents.h"

namespace {

int kUserDataKey;

class InstantOverlayUserData : public base::SupportsUserData::Data {
 public:
  explicit InstantOverlayUserData(InstantOverlay* overlay)
      : overlay_(overlay) {}

  virtual InstantOverlay* overlay() const { return overlay_; }

 private:
  virtual ~InstantOverlayUserData() {}

  InstantOverlay* const overlay_;

  DISALLOW_COPY_AND_ASSIGN(InstantOverlayUserData);
};

}  // namespace

// static
InstantOverlay* InstantOverlay::FromWebContents(
    const content::WebContents* web_contents) {
  InstantOverlayUserData* data = static_cast<InstantOverlayUserData*>(
      web_contents->GetUserData(&kUserDataKey));
  return data ? data->overlay() : NULL;
}

InstantOverlay::InstantOverlay(InstantController* controller,
                               const std::string& instant_url)
    : InstantPage(controller),
      loader_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      instant_url_(instant_url),
      is_stale_(false),
      is_pointer_down_from_activate_(false) {
}

InstantOverlay::~InstantOverlay() {
}

void InstantOverlay::InitContents(Profile* profile,
                                  const content::WebContents* active_tab) {
  loader_.Init(GURL(instant_url_), profile, active_tab,
               base::Bind(&InstantOverlay::HandleStalePage,
                           base::Unretained(this)));
  SetContents(loader_.contents());
  contents()->SetUserData(&kUserDataKey, new InstantOverlayUserData(this));
  loader_.Load();
}

scoped_ptr<content::WebContents> InstantOverlay::ReleaseContents() {
  contents()->RemoveUserData(&kUserDataKey);
  SetContents(NULL);
  return loader_.ReleaseContents();
}

void InstantOverlay::DidNavigate(
    const history::HistoryAddPageArgs& add_page_args) {
  last_navigation_ = add_page_args;
}

bool InstantOverlay::IsUsingLocalOverlay() const {
  return instant_url_ == chrome::search::kLocalOmniboxPopupURL;
}

void InstantOverlay::Update(const string16& text,
                            size_t selection_start,
                            size_t selection_end,
                            bool verbatim) {
  last_navigation_ = history::HistoryAddPageArgs();
  InstantPage::Update(text, selection_start, selection_end, verbatim);
}

bool InstantOverlay::ShouldProcessRenderViewCreated() {
  return true;
}

bool InstantOverlay::ShouldProcessRenderViewGone() {
  return true;
}

bool InstantOverlay::ShouldProcessAboutToNavigateMainFrame() {
  return true;
}

bool InstantOverlay::ShouldProcessSetSuggestions() {
  return true;
}

bool InstantOverlay::ShouldProcessShowInstantOverlay() {
  return true;
}

bool InstantOverlay::ShouldProcessNavigateToURL() {
  return true;
}

void InstantOverlay::OnSwappedContents() {
  contents()->RemoveUserData(&kUserDataKey);
  SetContents(loader_.contents());
  contents()->SetUserData(&kUserDataKey, new InstantOverlayUserData(this));
  instant_controller()->SwappedOverlayContents();
}

void InstantOverlay::OnFocus() {
  // The overlay is getting focus. Equivalent to it being clicked.
  base::AutoReset<bool> reset(&is_pointer_down_from_activate_, true);
  instant_controller()->FocusedOverlayContents();
}

void InstantOverlay::OnMouseDown() {
  is_pointer_down_from_activate_ = true;
}

void InstantOverlay::OnMouseUp() {
  if (is_pointer_down_from_activate_) {
    is_pointer_down_from_activate_ = false;
    instant_controller()->CommitIfPossible(INSTANT_COMMIT_FOCUS_LOST);
  }
}

content::WebContents* InstantOverlay::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  if (!supports_instant()) {
    // If the page doesn't yet support Instant, it hasn't fully loaded.
    // This is a redirect that we should allow. http://crbug.com/177948
    content::NavigationController::LoadURLParams load_params(params.url);
    load_params.transition_type = params.transition;
    load_params.referrer = params.referrer;
    load_params.extra_headers = params.extra_headers;
    load_params.is_renderer_initiated = params.is_renderer_initiated;
    load_params.transferred_global_request_id =
        params.transferred_global_request_id;
    load_params.is_cross_site_redirect = params.is_cross_site_redirect;

    contents()->GetController().LoadURLWithParams(load_params);
    return contents();
  }

  // We will allow the navigate to continue if we are able to commit the
  // overlay.
  //
  // First, cache the overlay contents since committing it will cause the
  // contents to be released (and be set to NULL).
  content::WebContents* overlay = contents();
  if (instant_controller()->CommitIfPossible(INSTANT_COMMIT_NAVIGATED)) {
    // If the commit was successful, the overlay's delegate should be the tab
    // strip, which will be able to handle the navigation.
    DCHECK_NE(&loader_, overlay->GetDelegate());
    return overlay->GetDelegate()->OpenURLFromTab(source, params);
  }
  return NULL;
}

void InstantOverlay::HandleStalePage() {
  is_stale_ = true;
  instant_controller()->ReloadOverlayIfStale();
}
