// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_ENGINES_TEMPLATE_URL_FETCHER_UI_CALLBACKS_H_
#define CHROME_BROWSER_UI_SEARCH_ENGINES_TEMPLATE_URL_FETCHER_UI_CALLBACKS_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/search_engines/template_url_fetcher_callbacks.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class SearchEngineTabHelper;
class TabContentsWrapper;

// Callbacks which display UI for the TemplateURLFetcher.
class TemplateURLFetcherUICallbacks : public TemplateURLFetcherCallbacks,
                                      public NotificationObserver {
 public:
  explicit TemplateURLFetcherUICallbacks(SearchEngineTabHelper* tab_helper,
                                         TabContentsWrapper* tab_contents);
  virtual ~TemplateURLFetcherUICallbacks();

  // TemplateURLFetcherCallback implementation.
  virtual void ConfirmSetDefaultSearchProvider(
      TemplateURL* template_url,
      TemplateURLModel* template_url_model);
  virtual void ConfirmAddSearchProvider(
      TemplateURL* template_url,
      Profile* profile);

  // NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  // The SearchEngineTabHelper where this request originated. Can be NULL if the
  // originating tab is closed. If NULL, the engine is not added.
  SearchEngineTabHelper* source_;

  // The TabContentsWrapper where this request originated.
  TabContentsWrapper* tab_contents_;

  // Handles registering for our notifications.
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(TemplateURLFetcherUICallbacks);
};

#endif  // CHROME_BROWSER_UI_SEARCH_ENGINES_TEMPLATE_URL_FETCHER_UI_CALLBACKS_H_
