// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/first_run_bubble_presenter.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/search_engines/template_url_service.h"

// static
void FirstRunBubblePresenter::PresentWhenReady(Browser* browser) {
  TemplateURLService* url_service =
      TemplateURLServiceFactory::GetForProfile(browser->profile());
  if (url_service->loaded())
    return chrome::ShowFirstRunBubble(browser);

  // Deletes itself.
  new FirstRunBubblePresenter(url_service, browser);
}

FirstRunBubblePresenter::FirstRunBubblePresenter(
    TemplateURLService* url_service,
    Browser* browser)
    : template_url_service_(url_service),
      session_id_(browser->session_id()),
      scoped_observer_(this) {
  scoped_observer_.Add(template_url_service_);
  template_url_service_->Load();
}

FirstRunBubblePresenter::~FirstRunBubblePresenter() {}

void FirstRunBubblePresenter::OnTemplateURLServiceChanged() {
  // Don't actually self-destruct or run the callback until the
  // TemplateURLService is really loaded.
  if (!template_url_service_->loaded())
    return;
  Browser* browser = chrome::FindBrowserWithID(session_id_.id());
  // Since showing the bubble raises the parent window, only show it if the
  // browser is still active (and not closed).
  if (browser && browser->window()->IsActive())
    chrome::ShowFirstRunBubble(browser);
  delete this;
}

void FirstRunBubblePresenter::OnTemplateURLServiceShuttingDown() {
  delete this;
}
