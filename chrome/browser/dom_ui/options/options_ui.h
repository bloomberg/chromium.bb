// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_OPTIONS_OPTIONS_UI_H_
#define CHROME_BROWSER_DOM_UI_OPTIONS_OPTIONS_UI_H_
#pragma once

#include <string>

#include "base/scoped_ptr.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/dom_ui/web_ui.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_type.h"

class GURL;
class PrefService;
struct UserMetricsAction;

class OptionsUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  // The constructor takes over ownership of |localized_strings|.
  explicit OptionsUIHTMLSource(DictionaryValue* localized_strings);
  virtual ~OptionsUIHTMLSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const;

 private:
   // Localized strings collection.
  scoped_ptr<DictionaryValue> localized_strings_;

  DISALLOW_COPY_AND_ASSIGN(OptionsUIHTMLSource);
};

// The base class handler of Javascript messages of options pages.
class OptionsPageUIHandler : public WebUIMessageHandler,
                             public NotificationObserver {
 public:
  OptionsPageUIHandler();
  virtual ~OptionsPageUIHandler();

  // Is this handler enabled?
  virtual bool IsEnabled();

  // Collects localized strings for options page.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings) = 0;

  // Initialize the page.  Called once the DOM is available for manipulation.
  // This will be called only once.
  virtual void Initialize() {}

  // Uninitializes the page.  Called just before the object is destructed.
  virtual void Uninitialize() {}

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() {}

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {}

  void UserMetricsRecordAction(const UserMetricsAction& action);

 protected:
  struct OptionsStringResource {
    // The name of the resource in templateData.
    const char* name;
    // The .grd ID for the resource (IDS_*).
    int id;
    // True if the trailing colon should be stripped on platforms that
    // don't want trailing colons.
    bool strip_colon;
  };
  // A helper for simplifying the process of registering strings in WebUI.
  static void RegisterStrings(DictionaryValue* localized_strings,
                              const OptionsStringResource* resources,
                              size_t length);

  NotificationRegistrar registrar_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OptionsPageUIHandler);
};

class OptionsUI : public DOMUI {
 public:
  explicit OptionsUI(TabContents* contents);
  virtual ~OptionsUI();

  static RefCountedMemory* GetFaviconResourceBytes();
  virtual void RenderViewCreated(RenderViewHost* render_view_host);
  virtual void DidBecomeActiveForReusedRenderView();

  void InitializeHandlers();

 private:
  // Adds OptionsPageUiHandler to the handlers list if handler is enabled.
  void AddOptionsPageUIHandler(DictionaryValue* localized_strings,
                               OptionsPageUIHandler* handler);

  bool initialized_handlers_;

  DISALLOW_COPY_AND_ASSIGN(OptionsUI);
};

#endif  // CHROME_BROWSER_DOM_UI_OPTIONS_OPTIONS_UI_H_
