// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOUCH_FRAME_KEYBOARD_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_TOUCH_FRAME_KEYBOARD_CONTAINER_VIEW_H_
#pragma once

#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/common/extensions/extension_messages.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "views/view.h"

namespace IPC {
class Message;
}

class Browser;
class DOMView;
class GURL;
class Profile;

// A class that contains and decorates the virtual keyboard.
//
// This class is also responsible for managing focus of all views related to
// the keyboard to prevent them from interfering with the ClientView.
class KeyboardContainerView : public views::View,
                              public TabContentsObserver,
                              public ExtensionFunctionDispatcher::Delegate {
 public:
  // Internal class name.
  static const char kViewClassName[];

  KeyboardContainerView(Profile* profile, Browser* browser, const GURL& url);
  virtual ~KeyboardContainerView();

  // Overridden from views::View
  virtual std::string GetClassName() const OVERRIDE;
  virtual void Layout();

  // ExtensionFunctionDispatcher::Delegate implementation
  virtual Browser* GetBrowser();
  virtual gfx::NativeView GetNativeViewOfHost();
  virtual TabContents* GetAssociatedTabContents() const;

  // Shows |keyboard_url|. The URL should look something like
  // http://id_of_the_vk_extension/index.html#layout_name_to_show
  void LoadURL(const GURL& keyboard_url);

 protected:
  // Overridden from views::View
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child);

  // Overridden from TabContentsObserver
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  void OnRequest(const ExtensionHostMsg_Request_Params& params);

  DOMView* dom_view_;
  ExtensionFunctionDispatcher extension_function_dispatcher_;
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardContainerView);
};

#endif  // CHROME_BROWSER_UI_TOUCH_FRAME_KEYBOARD_CONTAINER_VIEW_H_
