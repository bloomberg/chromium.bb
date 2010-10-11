// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url_fetcher_ui_callbacks.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"

TemplateURLFetcherUICallbacks::TemplateURLFetcherUICallbacks(
    TabContents* source)
    : source_(source) {
  registrar_.Add(this,
                 NotificationType::TAB_CONTENTS_DESTROYED,
                 Source<TabContents>(source_));
}

TemplateURLFetcherUICallbacks::~TemplateURLFetcherUICallbacks() {
}

void TemplateURLFetcherUICallbacks::ConfirmSetDefaultSearchProvider(
    TemplateURL* template_url,
    TemplateURLModel* template_url_model) {
  scoped_ptr<TemplateURL> owned_template_url(template_url);
  if (!source_ || !source_->delegate())
      return;

  source_->delegate()->ConfirmSetDefaultSearchProvider(
      source_,
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
  DCHECK(source == Source<TabContents>(source_));
  source_ = NULL;
}
