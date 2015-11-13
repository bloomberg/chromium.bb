// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/data_usage/data_use_tab_helper.h"

#include "base/logging.h"
#include "chrome/browser/android/data_usage/data_use_ui_tab_model.h"
#include "chrome/browser/android/data_usage/data_use_ui_tab_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/base/page_transition_types.h"

namespace content {
class RenderFrameHost;
}

DEFINE_WEB_CONTENTS_USER_DATA_KEY(DataUseTabHelper);

DataUseTabHelper::~DataUseTabHelper() {}

DataUseTabHelper::DataUseTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

void DataUseTabHelper::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Notify the DataUseUITabModel.
  chrome::android::DataUseUITabModel* data_use_ui_tab_model =
      chrome::android::DataUseUITabModelFactory::GetForBrowserContext(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  if (data_use_ui_tab_model) {
    data_use_ui_tab_model->ReportBrowserNavigation(
        navigation_handle->GetURL(),
        ui::PageTransitionFromInt(navigation_handle->GetPageTransition()),
        SessionTabHelper::IdForTab(web_contents()));
  }
}

void DataUseTabHelper::FrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Notify the DataUseUITabModel.
  chrome::android::DataUseUITabModel* data_use_ui_tab_model =
      chrome::android::DataUseUITabModelFactory::GetForBrowserContext(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  if (data_use_ui_tab_model) {
    data_use_ui_tab_model->ReportTabClosure(
        SessionTabHelper::IdForTab(web_contents()));
  }
}
