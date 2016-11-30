// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_OPTIONS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_OPTIONS_UI_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_message_handler.h"

class AutocompleteResult;

namespace base {
class DictionaryValue;
class ListValue;
}

#if defined(OS_CHROMEOS)
namespace chromeos {
namespace system {
class PointerDeviceObserver;
}  // namespace system
}  // namespace chromeos
#endif

namespace options {

// The base class handler of Javascript messages of options pages.
class OptionsPageUIHandler : public content::WebUIMessageHandler {
 public:
  OptionsPageUIHandler();
  ~OptionsPageUIHandler() override;

  // Is this handler enabled?
  virtual bool IsEnabled();

  // Collects localized strings for options page.
  virtual void GetLocalizedValues(base::DictionaryValue* localized_strings) = 0;

  virtual void PageLoadStarted() {}

  // Will be called only once in the life time of the handler. Generally used to
  // add observers, initializes preferences, or start asynchronous calls from
  // various services.
  virtual void InitializeHandler() {}

  // Initialize the page. Called once the DOM is available for manipulation.
  // This will be called when a RenderView is re-used (when navigated to with
  // back/forward or session restored in some cases) or when created.
  virtual void InitializePage() {}

  // Uninitializes the page.  Called just before the object is destructed.
  virtual void Uninitialize() {}

  // WebUIMessageHandler implementation.
  void RegisterMessages() override {}

 protected:
  struct OptionsStringResource {
    // The name of the resource in templateData.
    const char* name;
    // The .grd ID for the resource (IDS_*).
    int id;
    // The .grd ID of the string to replace $1 in |id|'s string. If zero or
    // omitted (default initialized), no substitution is attempted.
    int substitution_id;
  };

  // A helper to simplify string registration in WebUI for strings which do not
  // change at runtime and optionally contain a single substitution.
  static void RegisterStrings(base::DictionaryValue* localized_strings,
                              const OptionsStringResource* resources,
                              size_t length);

  // Registers string resources for a page's header and tab title.
  static void RegisterTitle(base::DictionaryValue* localized_strings,
                            const std::string& variable_name,
                            int title_id);

  content::NotificationRegistrar registrar_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OptionsPageUIHandler);
};

// An interface for common operations that a host of OptionsPageUIHandlers
// should provide.
class OptionsPageUIHandlerHost {
 public:
  virtual void InitializeHandlers() = 0;
  virtual void OnFinishedLoading() {}

 protected:
  virtual ~OptionsPageUIHandlerHost() {}
};

// The WebUI for chrome:settings-frame.
class OptionsUI : public content::WebUIController,
                  public content::WebContentsObserver,
                  public OptionsPageUIHandlerHost {
 public:
  typedef base::CallbackList<void()> OnFinishedLoadingCallbackList;

  explicit OptionsUI(content::WebUI* web_ui);
  ~OptionsUI() override;

  // Registers a callback to be called once the settings frame has finished
  // loading on the HTML/JS side.
  std::unique_ptr<OnFinishedLoadingCallbackList::Subscription>
  RegisterOnFinishedLoadingCallback(const base::Closure& callback);

  // Takes the suggestions from |result| and adds them to |suggestions| so that
  // they can be passed to a JavaScript function.
  static void ProcessAutocompleteSuggestions(
      const AutocompleteResult& result,
      base::ListValue* const suggestions);

  // Overridden from content::WebContentsObserver:
  void DidStartProvisionalLoadForFrame(
      content::RenderFrameHost* render_frame_host,
      const GURL& validated_url,
      bool is_error_page) override;
  void DocumentLoadedInFrame(
      content::RenderFrameHost *render_frame_host) override;
  void DocumentOnLoadCompletedInMainFrame() override;

  // Overridden from OptionsPageUIHandlerHost:
  void InitializeHandlers() override;
  void OnFinishedLoading() override;

 private:
  // Adds OptionsPageUiHandler to the handlers list if handler is enabled.
  void AddOptionsPageUIHandler(base::DictionaryValue* localized_strings,
                               OptionsPageUIHandler* handler);

  bool initialized_handlers_;

  std::vector<OptionsPageUIHandler*> handlers_;
  OnFinishedLoadingCallbackList on_finished_loading_callbacks_;

#if defined(OS_CHROMEOS)
  std::unique_ptr<chromeos::system::PointerDeviceObserver>
      pointer_device_observer_;
#endif

  base::Time load_start_time_;

  DISALLOW_COPY_AND_ASSIGN(OptionsUI);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_OPTIONS_UI_H_
