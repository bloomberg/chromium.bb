// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OPTIONS_CONTENT_SETTINGS_WINDOW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OPTIONS_CONTENT_SETTINGS_WINDOW_VIEW_H_
#pragma once

#include "chrome/browser/prefs/pref_member.h"
#include "chrome/common/content_settings_types.h"
#include "views/controls/listbox/listbox.h"
#include "views/view.h"
#include "views/window/dialog_delegate.h"

class Profile;
class MessageLoop;
class OptionsPageView;

namespace views {
class Label;
}  // namespace views

///////////////////////////////////////////////////////////////////////////////
// ContentSettingsWindowView
//
//  The contents of the Options dialog window.
//
class ContentSettingsWindowView : public views::View,
                                  public views::DialogDelegate,
                                  public views::Listbox::Listener {
 public:
  explicit ContentSettingsWindowView(Profile* profile);
  virtual ~ContentSettingsWindowView();

  // Shows the Tab corresponding to the specified Content Settings page.
  void ShowContentSettingsTab(ContentSettingsType page);

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

  // views::Listbox::Listener implementation:
  virtual void ListboxSelectionChanged(views::Listbox* sender);

 private:
  // Initializes the view.
  void Init();

  // Makes |pages_[page]| the currently visible page.
  void ShowSettingsPage(int page);

  // Returns the currently selected OptionsPageView.
  const OptionsPageView* GetCurrentContentSettingsTabView() const;

  // The Profile associated with these options.
  Profile* profile_;

  // The label above the left box.
  views::Label* label_;

  // The listbox used to select a page.
  views::Listbox* listbox_;

  // The last page the user was on when they opened the Options window.
  IntegerPrefMember last_selected_page_;

  // Stores the index of the currently visible page.
  int current_page_;

  // Stores the possible content pages displayed on the right.
  // |pages_[current_page_]| is the currently displayed page, and it's the only
  // parented View in |pages_|.
  std::vector<View*> pages_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingsWindowView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OPTIONS_CONTENT_SETTINGS_WINDOW_VIEW_H_

