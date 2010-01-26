// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_OPTIONS_CONTENT_SETTINGS_WINDOW_VIEW_H_
#define CHROME_BROWSER_VIEWS_OPTIONS_CONTENT_SETTINGS_WINDOW_VIEW_H_

#include "chrome/common/pref_member.h"
#include "chrome/browser/content_settings_window.h"
#include "views/controls/tabbed_pane/tabbed_pane.h"
#include "views/view.h"
#include "views/window/dialog_delegate.h"

class Profile;
class MessageLoop;
class OptionsPageView;

///////////////////////////////////////////////////////////////////////////////
// ContentSettingsWindowView
//
//  The contents of the Options dialog window.
//
class ContentSettingsWindowView : public views::View,
                                  public views::DialogDelegate,
                                  public views::TabbedPane::Listener {
 public:
  explicit ContentSettingsWindowView(Profile* profile);
  virtual ~ContentSettingsWindowView();

  // Shows the Tab corresponding to the specified Content Settings page.
  void ShowContentSettingsTab(ContentSettingsTab page);

 protected:
  // views::View overrides:
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);

  // views::DialogDelegate implementation:
  virtual int GetDialogButtons() const {
    return MessageBoxFlags::DIALOGBUTTON_CANCEL;
  }
  virtual std::wstring GetWindowTitle() const;
  virtual void WindowClosing();
  virtual bool Cancel();
  virtual views::View* GetContentsView();

  // views::TabbedPane::Listener implementation:
  virtual void TabSelectedAt(int index);

 private:
  // Initializes the view.
  void Init();

  // Returns the currently selected OptionsPageView.
  const OptionsPageView* GetCurrentContentSettingsTabView() const;

  // The Tab view that contains all of the options pages.
  views::TabbedPane* tabs_;

  // The Profile associated with these options.
  Profile* profile_;

  // The last page the user was on when they opened the Options window.
  IntegerPrefMember last_selected_page_;

  DISALLOW_EVIL_CONSTRUCTORS(ContentSettingsWindowView);
};

#endif  // CHROME_BROWSER_VIEWS_OPTIONS_CONTENT_SETTINGS_WINDOW_VIEW_H_

