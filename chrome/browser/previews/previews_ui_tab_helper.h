// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_PREVIEWS_UI_TAB_HELPER_H_
#define CHROME_BROWSER_PREVIEWS_PREVIEWS_UI_TAB_HELPER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_user_data.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

typedef base::OnceCallback<void(bool opt_out)> OnDismissPreviewsUICallback;

// Tracks whether a previews UI has been shown for a page. Handles showing
// the UI when the main frame response indicates a Lite Page.
class PreviewsUITabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PreviewsUITabHelper> {
 public:
  ~PreviewsUITabHelper() override;

  // Trigger the Previews UI to be shown to the user.
  void ShowUIElement(previews::PreviewsType previews_type,
                     base::Time previews_freshness,
                     bool is_data_saver_user,
                     bool is_reload,
                     OnDismissPreviewsUICallback on_dismiss_callback);

  // Reloads the content of the page without previews.
  void ReloadWithoutPreviews();

  // Reloads the content of the page without previews for the given preview
  // type.
  void ReloadWithoutPreviews(previews::PreviewsType previews_type);

  // Indicates whether the UI for a preview has been shown for the page.
  bool displayed_preview_ui() const { return displayed_preview_ui_; }

  // Sets whether the UI for a preview has been shown for the page.
  // |displayed_preview_ui_| is reset to false on
  // DidStartProvisionalLoadForFrame for the main frame.
  void set_displayed_preview_ui(bool displayed) {
    displayed_preview_ui_ = displayed;
  }

#if defined(OS_ANDROID)
  // Indicates whether the Android Omnibox badge should be shown as the Previews
  // UI.
  bool should_display_android_omnibox_badge() const {
    return should_display_android_omnibox_badge_;
  }
#endif

  // Sets whether the timestamp on the UI for a preview has been shown for
  // the page. |displayed_preview_timestamp_| is reset to false on
  // DidStartProvisionalLoadForFrame for the main frame.
  void set_displayed_preview_timestamp(bool displayed_preview_timestamp) {
    displayed_preview_timestamp_ = displayed_preview_timestamp;
  }

  // The Previews information related to the navigation that was most recently
  // finished.
  previews::PreviewsUserData* previews_user_data() const {
    return previews_user_data_.get();
  }

 private:
  friend class content::WebContentsUserData<PreviewsUITabHelper>;
  friend class PreviewsUITabHelperUnitTest;

  explicit PreviewsUITabHelper(content::WebContents* web_contents);

  // Overridden from content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // True if the UI for a preview has been shown for the page.
  bool displayed_preview_ui_ = false;

  // True if the UI with a timestamp was shown for the page.
  bool displayed_preview_timestamp_ = false;

#if defined(OS_ANDROID)
  // True if the Android Omnibox badge should be shown as the Previews UI.
  bool should_display_android_omnibox_badge_ = false;
#endif

  // The callback to run when the original page is loaded.
  OnDismissPreviewsUICallback on_dismiss_callback_;

  // The Previews information related to the navigation that was most recently
  // finished.
  std::unique_ptr<previews::PreviewsUserData> previews_user_data_;

  DISALLOW_COPY_AND_ASSIGN(PreviewsUITabHelper);
};

#endif  // CHROME_BROWSER_PREVIEWS_PREVIEWS_UI_TAB_HELPER_H_
