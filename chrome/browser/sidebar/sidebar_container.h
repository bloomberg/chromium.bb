// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIDEBAR_SIDEBAR_CONTAINER_H_
#define CHROME_BROWSER_SIDEBAR_SIDEBAR_CONTAINER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"

class SkBitmap;
class TabContents;

///////////////////////////////////////////////////////////////////////////////
// SidebarContainer
//
//  Stores one particular sidebar state: sidebar's content, its content id,
//  tab it is linked to, mini tab icon, title etc.
//
class SidebarContainer : public TabContentsDelegate,
                         private ImageLoadingTracker::Observer {
 public:
  // Interface to implement to listen for sidebar update notification.
  class Delegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}
    virtual void UpdateSidebar(SidebarContainer* host) = 0;
   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  SidebarContainer(TabContents* tab,
                   const std::string& content_id,
                   Delegate* delegate);
  virtual ~SidebarContainer();

  // Called right before destroying this sidebar.
  // Does all the necessary cleanup.
  void SidebarClosing();

  // Sets default sidebar parameters, as specified in extension manifest.
  void LoadDefaults();

  // Returns sidebar's content id.
  const std::string& content_id() const { return content_id_; }

  // Returns TabContents sidebar is linked to.
  TabContents* tab_contents() const { return tab_; }

  // Returns sidebar's TabContents.
  TabContents* sidebar_contents() const { return sidebar_contents_.get(); }

  // Accessor for the badge text.
  const string16& badge_text() const { return badge_text_; }

  // Accessor for the icon.
  const SkBitmap& icon() const { return *icon_; }

  // Accessor for the title.
  const string16& title() const { return title_; }

  // Functions supporting chrome.experimental.sidebar API.

  // Notifies hosting window that this sidebar was expanded.
  void Show();

  // Notifies hosting window that this sidebar was expanded.
  void Expand();

  // Notifies hosting window that this sidebar was collapsed.
  void Collapse();

  // Navigates sidebar contents to the |url|.
  void Navigate(const GURL& url);

  // Changes sidebar's badge text.
  void SetBadgeText(const string16& badge_text);

  // Changes sidebar's icon.
  void SetIcon(const SkBitmap& bitmap);

  // Changes sidebar's title.
  void SetTitle(const string16& title);

 private:
  // Overridden from TabContentsDelegate:
  virtual content::JavaScriptDialogCreator*
      GetJavaScriptDialogCreator() OVERRIDE;

  // Overridden from ImageLoadingTracker::Observer:
  virtual void OnImageLoaded(SkBitmap* image,
                             const ExtensionResource& resource,
                             int index) OVERRIDE;

  // Returns an extension this sidebar belongs to.
  const Extension* GetExtension() const;

  // Contents of the tab this sidebar is linked to.
  TabContents* tab_;

  // Sidebar's content id. There might be more than one sidebar liked to each
  // particular tab and they are identified by their unique content id.
  const std::string content_id_;

  // Sidebar update notification listener.
  Delegate* delegate_;

  // Sidebar contents.
  scoped_ptr<TabContents> sidebar_contents_;

  // Badge text displayed on the sidebar's mini tab.
  string16 badge_text_;

  // Icon displayed on the sidebar's mini tab.
  scoped_ptr<SkBitmap> icon_;

  // Sidebar's title, displayed as a tooltip for sidebar's mini tab.
  string16 title_;

  // On the first expand sidebar will be automatically navigated to the default
  // page (specified in the extension manifest), but only if the extension has
  // not explicitly navigated it yet. This variable is set to false on the first
  // sidebar navigation.
  bool navigate_to_default_page_on_expand_;
  // Since the default icon (specified in the extension manifest) is loaded
  // asynchronously, sidebar icon can already be set by the extension
  // by the time it's loaded. This variable tracks whether the loaded default
  // icon should be used or discarded.
  bool use_default_icon_;

  // Helper to load icons from extension asynchronously.
  scoped_ptr<ImageLoadingTracker> image_loading_tracker_;

  DISALLOW_COPY_AND_ASSIGN(SidebarContainer);
};

#endif  // CHROME_BROWSER_SIDEBAR_SIDEBAR_CONTAINER_H_
