// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LANGUAGE_MENU_BUTTON_H_
#define CHROME_BROWSER_CHROMEOS_LANGUAGE_MENU_BUTTON_H_

#include "chrome/browser/chromeos/language_library.h"
#include "chrome/browser/chromeos/status_area_button.h"
#include "views/controls/menu/menu_2.h"
#include "views/controls/menu/view_menu_delegate.h"

class Browser;
class SkBitmap;

namespace chromeos {

// The language menu button in the status area.
// This class will handle getting the IME/XKB status and populating the menu.
class LanguageMenuButton : public views::MenuButton,
                           public views::ViewMenuDelegate,
                           public menus::MenuModel,
                           public LanguageLibrary::Observer {
 public:
  explicit LanguageMenuButton(Browser* browser);
  virtual ~LanguageMenuButton();

  // views::Menu2Model implementation.
  virtual bool HasIcons() const;
  virtual int GetItemCount() const;
  virtual menus::MenuModel::ItemType GetTypeAt(int index) const;
  virtual int GetCommandIdAt(int index) const;
  virtual string16 GetLabelAt(int index) const;
  virtual bool IsLabelDynamicAt(int index) const;
  virtual bool GetAcceleratorAt(int index,
                                menus::Accelerator* accelerator) const;
  virtual bool IsItemCheckedAt(int index) const;
  virtual int GetGroupIdAt(int index) const;
  virtual bool GetIconAt(int index, SkBitmap* icon) const;
  virtual bool IsEnabledAt(int index) const;
  virtual menus::MenuModel* GetSubmenuModelAt(int index) const;
  virtual void HighlightChangedTo(int index);
  virtual void ActivatedAt(int index);
  virtual void MenuWillShow();

  // LanguageLibrary::Observer implementation.
  virtual void LanguageChanged(LanguageLibrary* obj);

 private:
  // views::ViewMenuDelegate implementation.
  virtual void RunMenu(views::View* source, const gfx::Point& pt);

  // Update the status area with |name|.
  void UpdateIcon(const std::wstring& name);

  // The current language list.
  scoped_ptr<InputLanguageList> language_list_;

  // The language menu.
  views::Menu2 language_menu_;
  // The browser window that owns us.
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(LanguageMenuButton);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LANGUAGE_MENU_BUTTON_H_
