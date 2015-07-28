// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_WEB_CONTENTS_OBSERVER_H_

#include "base/callback.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace offline_pages {

// Observes activities occurred in the page that is to be archived.
class OfflinePageWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<OfflinePageWebContentsObserver> {
 public:
  ~OfflinePageWebContentsObserver() override;

  bool is_document_loaded_in_main_frame() const {
    return is_document_loaded_in_main_frame_;
  }

  void set_main_frame_document_loaded_callback(const base::Closure& callback) {
    main_frame_document_loaded_callback_ = callback;
  }

  // content::WebContentsObserver:
  void DocumentLoadedInFrame(
      content::RenderFrameHost* render_frame_host) override;
  void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) override;
  void RenderProcessGone(base::TerminationStatus status) override;

 private:
  explicit OfflinePageWebContentsObserver(content::WebContents* web_contents);
  friend class content::WebContentsUserData<OfflinePageWebContentsObserver>;

  // Removes the observer and clears the WebContents member.
  void CleanUp();

  // Tracks whether the document of main frame has been loaded.
  bool is_document_loaded_in_main_frame_;

  base::Closure main_frame_document_loaded_callback_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageWebContentsObserver);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_OFFLINE_PAGE_WEB_CONTENTS_OBSERVER_H_
