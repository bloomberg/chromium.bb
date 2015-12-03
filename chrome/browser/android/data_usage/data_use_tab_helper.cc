// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/data_usage/data_use_tab_helper.h"

#include "base/logging.h"
#include "chrome/browser/android/data_usage/data_use_ui_tab_model.h"
#include "chrome/browser/android/data_usage/data_use_ui_tab_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "ui/base/page_transition_types.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(DataUseTabHelper);

DataUseTabHelper::~DataUseTabHelper() {}

DataUseTabHelper::DataUseTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

void DataUseTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // TODO(tbansal): Consider the case of a page that provides a navigation bar
  // and loads pages in a sub-frame.
  if (!navigation_handle->IsInMainFrame())
    return;

  // crbug.com/564871: Calling GetPageTransition() may fail if the navigation
  // has not committed.
  if (!navigation_handle->HasCommitted())
    return;

  // Notify the DataUseUITabModel.
  chrome::android::DataUseUITabModel* data_use_ui_tab_model =
      chrome::android::DataUseUITabModelFactory::GetForBrowserContext(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  SessionID::id_type tab_id = SessionTabHelper::IdForTab(web_contents());
  if (data_use_ui_tab_model && tab_id >= 0) {
    data_use_ui_tab_model->ReportBrowserNavigation(
        navigation_handle->GetURL(),
        ui::PageTransitionFromInt(navigation_handle->GetPageTransition()),
        tab_id);
  }
}

void DataUseTabHelper::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Check if it is a main frame.
  if (render_frame_host->GetParent())
    return;

  // Notify the DataUseUITabModel.
  chrome::android::DataUseUITabModel* data_use_ui_tab_model =
      chrome::android::DataUseUITabModelFactory::GetForBrowserContext(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  SessionID::id_type tab_id = SessionTabHelper::IdForTab(web_contents());
  if (data_use_ui_tab_model && tab_id >= 0)
    data_use_ui_tab_model->ReportTabClosure(tab_id);
}
