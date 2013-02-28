// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTERNAL_TAB_EXTERNAL_TAB_CONTAINER_H_
#define CHROME_BROWSER_EXTERNAL_TAB_EXTERNAL_TAB_CONTAINER_H_

#include <windows.h>

#include "base/memory/ref_counted.h"
#include "ui/gfx/native_widget_types.h"

class AutomationProvider;
class AutomationResourceMessageFilter;
class GURL;
class Profile;

namespace content {
class WebContents;
}

namespace gfx {
class Rect;
}

namespace IPC {
class Message;
}

class ExternalTabContainer : public base::RefCounted<ExternalTabContainer> {
 public:
  static ExternalTabContainer* Create(
      AutomationProvider* automation_provider,
      AutomationResourceMessageFilter* filter);

  // A helper method that retrieves the ExternalTabContainer object that
  // hosts the given WebContents.
  static ExternalTabContainer* GetContainerForTab(
      content::WebContents* web_contents);

  // Returns the ExternalTabContainer instance associated with the cookie
  // passed in. It also erases the corresponding reference from the map.
  // Returns NULL if we fail to find the cookie in the map.
  static scoped_refptr<ExternalTabContainer> RemovePendingTab(uintptr_t cookie);

  // Initializes the instance. This must be invoked before any other member
  // functions.
  virtual bool Init(Profile* profile,
                    HWND parent,
                    const gfx::Rect& bounds,
                    DWORD style,
                    bool load_requests_via_automation,
                    bool handle_top_level_requests,
                    content::WebContents* existing_contents,
                    const GURL& initial_url,
                    const GURL& referrer,
                    bool infobars_enabled,
                    bool supports_full_tab_mode) = 0;

  // Unhook the keystroke listener and notify about the closing WebContents.
  // This function gets called from three places, which is fine.
  // 1. OnFinalMessage
  // 2. In the destructor.
  // 3. In AutomationProvider::CreateExternalTab
  virtual void Uninitialize() = 0;

  // Used to reinitialize the automation channel and related information
  // for this container. Typically used when an ExternalTabContainer
  // instance is created by Chrome and attached to an automation client.
  virtual bool Reinitialize(AutomationProvider* automation_provider,
                            AutomationResourceMessageFilter* filter,
                            HWND parent_window) = 0;

  // This is invoked when the external host reflects back to us a keyboard
  // message it did not process.
  virtual void ProcessUnhandledAccelerator(const MSG& msg) = 0;

  // See WebContents::FocusThroughTabTraversal. Called from AutomationProvider.
  virtual void FocusThroughTabTraversal(bool reverse,
                                        bool restore_focus_to_view) = 0;

  virtual void RunUnloadHandlers(IPC::Message* reply_message) = 0;

  virtual content::WebContents* GetWebContents() const = 0;
  virtual HWND GetExternalTabHWND() const = 0;
  virtual HWND GetContentHWND() const = 0;

  virtual void SetTabHandle(int handle) = 0;
  virtual int GetTabHandle() const = 0;

  // Returns true if the context menu command was handled
  virtual bool ExecuteContextMenuCommand(int command) = 0;

 protected:
  virtual ~ExternalTabContainer() {}

 private:
  friend class base::RefCounted<ExternalTabContainer>;
};

#endif  // CHROME_BROWSER_EXTERNAL_TAB_EXTERNAL_TAB_CONTAINER_H_
