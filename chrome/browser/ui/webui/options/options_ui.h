// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_OPTIONS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_OPTIONS_UI_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "content/browser/webui/web_ui.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_type.h"

class GURL;
class PrefService;
struct UserMetricsAction;

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
  };
  // A helper for simplifying the process of registering strings in WebUI.
  static void RegisterStrings(DictionaryValue* localized_strings,
                              const OptionsStringResource* resources,
                              size_t length);

  // Registers string resources for a page's header and tab title.
  static void RegisterTitle(DictionaryValue* localized_strings,
                            const std::string& variable_name,
                            int title_id);

  NotificationRegistrar registrar_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OptionsPageUIHandler);
};

// An interface for common operations that a host of OptionsPageUIHandlers
// should provide.
class OptionsPageUIHandlerHost {
 public:
  virtual void InitializeHandlers() = 0;
};

// The WebUI for chrome:settings.
class OptionsUI : public WebUI,
                  public OptionsPageUIHandlerHost {
 public:
  explicit OptionsUI(TabContents* contents);
  virtual ~OptionsUI();

  static RefCountedMemory* GetFaviconResourceBytes();

  // Overridden from WebUI:
  virtual void RenderViewCreated(RenderViewHost* render_view_host) OVERRIDE;
  virtual void DidBecomeActiveForReusedRenderView() OVERRIDE;

  // Overridden from OptionsPageUIHandlerHost:
  virtual void InitializeHandlers() OVERRIDE;

 private:
  // Adds OptionsPageUiHandler to the handlers list if handler is enabled.
  void AddOptionsPageUIHandler(DictionaryValue* localized_strings,
                               OptionsPageUIHandler* handler);

  bool initialized_handlers_;

  DISALLOW_COPY_AND_ASSIGN(OptionsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_OPTIONS_UI_H_
