// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_ENGINES_TEMPLATE_URL_FETCHER_UI_CALLBACKS_H_
#define CHROME_BROWSER_UI_SEARCH_ENGINES_TEMPLATE_URL_FETCHER_UI_CALLBACKS_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/search_engines/template_url_fetcher_callbacks.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class SearchEngineTabHelper;

namespace content {
class WebContents;
}

// Callbacks which display UI for the TemplateURLFetcher.
class TemplateURLFetcherUICallbacks : public TemplateURLFetcherCallbacks,
                                      public content::NotificationObserver {
 public:
  TemplateURLFetcherUICallbacks(SearchEngineTabHelper* tab_helper,
                                content::WebContents* web_contents);
  virtual ~TemplateURLFetcherUICallbacks();

  // TemplateURLFetcherCallback implementation.
  virtual void ConfirmAddSearchProvider(TemplateURL* template_url,
                                        Profile* profile) OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  // The SearchEngineTabHelper where this request originated. Can be NULL if the
  // originating tab is closed. If NULL, the engine is not added.
  SearchEngineTabHelper* source_;

  // The WebContents where this request originated.
  content::WebContents* web_contents_;

  // Handles registering for our notifications.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(TemplateURLFetcherUICallbacks);
};

#endif  // CHROME_BROWSER_UI_SEARCH_ENGINES_TEMPLATE_URL_FETCHER_UI_CALLBACKS_H_
