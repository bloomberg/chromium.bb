// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INTENTS_WEB_INTENT_INLINE_DISPOSITION_DELEGATE_H_
#define CHROME_BROWSER_UI_INTENTS_WEB_INTENT_INLINE_DISPOSITION_DELEGATE_H_

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"

class Browser;
class WebIntentPicker;

// This class is the policy delegate for the rendered page in the intents
// inline disposition bubble. It also acts as a router for extension messages,
// so we can invoke extension APIs in inline disposition contexts.
class WebIntentInlineDispositionDelegate
    : public content::WebContentsDelegate,
      public content::WebContentsObserver,
      public ExtensionFunctionDispatcher::Delegate {
 public:
  // |picker| is notified when the web contents loading state changes. Must not
  // be NULL.
  // |contents| is the WebContents for the inline disposition.
  // |browser| is the browser inline disposition was invoked from.
  WebIntentInlineDispositionDelegate(WebIntentPicker* picker,
                                     content::WebContents* contents,
                                     Browser* browser);
  virtual ~WebIntentInlineDispositionDelegate();

  // WebContentsDelegate implementation.
  virtual bool IsPopupOrPanel(
      const content::WebContents* source) const OVERRIDE;
  virtual bool ShouldAddNavigationToHistory(
    const history::HistoryAddPageArgs& add_page_args,
    content::NavigationType navigation_type) OVERRIDE;
  virtual content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) OVERRIDE;
  virtual void AddNewContents(content::WebContents* source,
                              content::WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture,
                              bool* was_blocked) OVERRIDE;
  virtual void LoadingStateChanged(content::WebContents* source) OVERRIDE;
  virtual void ResizeDueToAutoResize(content::WebContents* source,
                                     const gfx::Size& pref_size) OVERRIDE;

  // content::WebContentsObserver
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void RenderViewCreated(
      content::RenderViewHost* render_view_host) OVERRIDE;

  // ExtensionFunctionDispatcher::Delegate
  virtual extensions::WindowController* GetExtensionWindowController()
    const OVERRIDE;
  virtual content::WebContents* GetAssociatedWebContents() const OVERRIDE;

  // Message handlers.
  void OnRequest(const ExtensionHostMsg_Request_Params& params);
 private:
  // Picker to notify when loading state changes. Weak pointer.
  WebIntentPicker* picker_;

  // The WebContents container. Weak pointer.
  content::WebContents* web_contents_;

  Browser* browser_;  // Weak pointer.

  // Dispatch handler for extension APIs.
  ExtensionFunctionDispatcher extension_function_dispatcher_;
};

#endif  // CHROME_BROWSER_UI_INTENTS_WEB_INTENT_INLINE_DISPOSITION_DELEGATE_H_
