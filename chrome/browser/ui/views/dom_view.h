// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DOMView is a ChromeView that displays the content of a web DOM.
// It should be used with data: URLs.

#ifndef CHROME_BROWSER_UI_VIEWS_DOM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOM_VIEW_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "googleurl/src/gurl.h"
#include "ui/views/events/event.h"
#include "views/controls/native/native_view_host.h"

class Profile;
class SiteInstance;
class TabContents;

class DOMView : public views::NativeViewHost {
 public:
  // Internal class name.
  static const char kViewClassName[];

  DOMView();
  virtual ~DOMView();

  // Overridden from View.
  virtual std::string GetClassName() const OVERRIDE;

  // Initialize the view, creating the contents. This should be
  // called once the view has been added to a container.
  //
  // If |instance| is not null, then the view will be loaded in the same
  // process as the given instance.
  bool Init(Profile* profile, SiteInstance* instance);

  // Loads the given URL into the page. You must have previously called Init().
  void LoadURL(const GURL& url);

  // The TabContents displaying the DOM contents; may be null.
  TabContentsWrapper* dom_contents() const { return dom_contents_.get(); }

 protected:
  // Overridden from View.
  virtual bool SkipDefaultKeyEventProcessing(const views::KeyEvent& e) OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void ViewHierarchyChanged(bool is_add, views::View* parent,
                                    views::View* child) OVERRIDE;

  // AttachTabContents calls Attach to hook up the NativeViewHost. This is
  // here because depending on whether this is a touch build or not the
  // implementation varies slightly, while Detach is the same in both cases.
  void AttachTabContents();

  // Returns new allocated TabContents instance, caller is responsible deleting.
  // Override in derived classes to replace TabContents with derivative.
  virtual TabContents* CreateTabContents(Profile* profile,
                                         SiteInstance* instance);

  scoped_ptr<TabContentsWrapper> dom_contents_;

 private:
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(DOMView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOM_VIEW_H_
