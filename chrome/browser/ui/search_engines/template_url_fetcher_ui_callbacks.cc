// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search_engines/template_url_fetcher_ui_callbacks.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/ui/search_engines/search_engine_tab_helper.h"
#include "chrome/browser/ui/search_engines/search_engine_tab_helper_delegate.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

TemplateURLFetcherUICallbacks::TemplateURLFetcherUICallbacks(
    SearchEngineTabHelper* tab_helper,
    WebContents* web_contents)
    : source_(tab_helper),
      web_contents_(web_contents) {
  registrar_.Add(this,
                 content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                 content::Source<WebContents>(web_contents_));
}

TemplateURLFetcherUICallbacks::~TemplateURLFetcherUICallbacks() {
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
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == content::NOTIFICATION_WEB_CONTENTS_DESTROYED);
  DCHECK(source == content::Source<WebContents>(web_contents_));
  source_ = NULL;
  web_contents_ = NULL;
}
