// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search_engines/template_url_fetcher_ui_callbacks.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/ui/search_engines/search_engine_tab_helper.h"
#include "chrome/browser/ui/search_engines/search_engine_tab_helper_delegate.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_source.h"
#include "content/common/notification_type.h"

TemplateURLFetcherUICallbacks::TemplateURLFetcherUICallbacks(
    SearchEngineTabHelper* tab_helper,
    TabContents* tab_contents)
    : source_(tab_helper),
      tab_contents_(tab_contents) {
  registrar_.Add(this,
                 NotificationType::TAB_CONTENTS_DESTROYED,
                 Source<TabContents>(tab_contents_));
}

TemplateURLFetcherUICallbacks::~TemplateURLFetcherUICallbacks() {
}

void TemplateURLFetcherUICallbacks::ConfirmSetDefaultSearchProvider(
    TemplateURL* template_url,
    TemplateURLModel* template_url_model) {
  scoped_ptr<TemplateURL> owned_template_url(template_url);
  if (!source_ || !source_->delegate() || !tab_contents_)
      return;

  source_->delegate()->ConfirmSetDefaultSearchProvider(
      tab_contents_,
      owned_template_url.release(),
      template_url_model);
}

void TemplateURLFetcherUICallbacks::ConfirmAddSearchProvider(
    TemplateURL* template_url,
    Profile* profile) {
  scoped_ptr<TemplateURL> owned_template_url(template_url);
  if (!source_ || !source_->delegate())
      return;

  source_->delegate()->ConfirmAddSearchProvider(owned_template_url.release(),
                                                profile);
}

void TemplateURLFetcherUICallbacks::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  DCHECK(type == NotificationType::TAB_CONTENTS_DESTROYED);
  DCHECK(source == Source<TabContents>(tab_contents_));
  source_ = NULL;
  tab_contents_ = NULL;
}
