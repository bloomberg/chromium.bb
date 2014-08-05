// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WRENCH_MENU_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WRENCH_MENU_H_

#include <map>
#include <utility>

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/models/menu_model.h"
#include "ui/views/controls/menu/menu_delegate.h"

class BookmarkMenuDelegate;
class Browser;
class WrenchMenuObserver;

namespace ui {
class NativeTheme;
}

namespace views {
class MenuButton;
struct MenuConfig;
class MenuItemView;
class MenuRunner;
class View;
}  // namespace views

// WrenchMenu adapts the WrenchMenuModel to view's menu related classes.
class WrenchMenu : public views::MenuDelegate,
                   public BaseBookmarkModelObserver,
                   public content::NotificationObserver {
 public:
  enum RunFlags {
    // Indicates that the menu was opened for a drag-and-drop operation.
    FOR_DROP = 1 << 0,
  };

  WrenchMenu(Browser* browser, int run_flags);
  virtual ~WrenchMenu();

  void Init(ui::MenuModel* model);

  // Shows the menu relative to the specified view.
  void RunMenu(views::MenuButton* host);

  // Closes the menu if it is open, otherwise does nothing.
  void CloseMenu();

  // Whether the menu is currently visible to the user.
  bool IsShowing();

  bool for_drop() const { return (run_flags_ & FOR_DROP) != 0; }

  void AddObserver(WrenchMenuObserver* observer);
  void RemoveObserver(WrenchMenuObserver* observer);

  // MenuDelegate overrides:
  virtual const gfx::FontList* GetLabelFontList(int command_id) const OVERRIDE;
  virtual bool GetShouldUseDisabledEmphasizedForegroundColor(
      int command_id) const OVERRIDE;
  virtual base::string16 GetTooltipText(int command_id,
                                        const gfx::Point& p) const OVERRIDE;
  virtual bool IsTriggerableEvent(views::MenuItemView* menu,
                                  const ui::Event& e) OVERRIDE;
  virtual bool GetDropFormats(
      views::MenuItemView* menu,
      int* formats,
      std::set<ui::OSExchangeData::CustomFormat>* custom_formats) OVERRIDE;
  virtual bool AreDropTypesRequired(views::MenuItemView* menu) OVERRIDE;
  virtual bool CanDrop(views::MenuItemView* menu,
                       const ui::OSExchangeData& data) OVERRIDE;
  virtual int GetDropOperation(views::MenuItemView* item,
                               const ui::DropTargetEvent& event,
                               DropPosition* position) OVERRIDE;
  virtual int OnPerformDrop(views::MenuItemView* menu,
                            DropPosition position,
                            const ui::DropTargetEvent& event) OVERRIDE;
  virtual bool ShowContextMenu(views::MenuItemView* source,
                               int command_id,
                               const gfx::Point& p,
                               ui::MenuSourceType source_type) OVERRIDE;
  virtual bool CanDrag(views::MenuItemView* menu) OVERRIDE;
  virtual void WriteDragData(views::MenuItemView* sender,
                             ui::OSExchangeData* data) OVERRIDE;
  virtual int GetDragOperations(views::MenuItemView* sender) OVERRIDE;
  virtual int GetMaxWidthForMenu(views::MenuItemView* menu) OVERRIDE;
  virtual bool IsItemChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandEnabled(int command_id) const OVERRIDE;
  virtual void ExecuteCommand(int command_id, int mouse_event_flags) OVERRIDE;
  virtual bool GetAccelerator(int command_id,
                              ui::Accelerator* accelerator) const OVERRIDE;
  virtual void WillShowMenu(views::MenuItemView* menu) OVERRIDE;
  virtual void WillHideMenu(views::MenuItemView* menu) OVERRIDE;
  virtual bool ShouldCloseOnDragComplete() OVERRIDE;

  // BaseBookmarkModelObserver overrides:
  virtual void BookmarkModelChanged() OVERRIDE;

  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  class CutCopyPasteView;
  class RecentTabsMenuModelDelegate;
  class ZoomView;

  typedef std::pair<ui::MenuModel*,int> Entry;
  typedef std::map<int,Entry> CommandIDToEntry;

  // Populates |parent| with all the child menus in |model|. Recursively invokes
  // |PopulateMenu| for any submenu.
  void PopulateMenu(views::MenuItemView* parent,
                    ui::MenuModel* model);

  // Adds a new menu item to |parent| at |menu_index| to represent the item in
  // |model| at |model_index|:
  // - |menu_index|: position in |parent| to add the new item.
  // - |model_index|: position in |model| to retrieve information about the
  //   new menu item.
  // The returned item's MenuItemView::GetCommand() is the same as that of
  // |model|->GetCommandIdAt(|model_index|).
  views::MenuItemView* AddMenuItem(views::MenuItemView* parent,
                                   int menu_index,
                                   ui::MenuModel* model,
                                   int model_index,
                                   ui::MenuModel::ItemType menu_type);

  // Invoked from the cut/copy/paste menus. Cancels the current active menu and
  // activates the menu item in |model| at |index|.
  void CancelAndEvaluate(ui::MenuModel* model, int index);

  // Creates the bookmark menu if necessary. Does nothing if already created or
  // the bookmark model isn't loaded.
  void CreateBookmarkMenu();

  // Returns the index of the MenuModel/index pair representing the |command_id|
  // in |command_id_to_entry_|.
  int ModelIndexFromCommandId(int command_id) const;

  // The views menu. Owned by |menu_runner_|.
  views::MenuItemView* root_;

  scoped_ptr<views::MenuRunner> menu_runner_;

  // Maps from the command ID in model to the model/index pair the item came
  // from.
  CommandIDToEntry command_id_to_entry_;

  // Browser the menu is being shown for.
  Browser* browser_;

  // |CancelAndEvaluate| sets |selected_menu_model_| and |selected_index_|.
  // If |selected_menu_model_| is non-null after the menu completes
  // ActivatedAt is invoked. This is done so that ActivatedAt isn't invoked
  // while the message loop is nested.
  ui::MenuModel* selected_menu_model_;
  int selected_index_;

  // Used for managing the bookmark menu items.
  scoped_ptr<BookmarkMenuDelegate> bookmark_menu_delegate_;

  // Menu corresponding to IDC_BOOKMARKS_MENU.
  views::MenuItemView* bookmark_menu_;

  // Menu corresponding to IDC_FEEDBACK.
  views::MenuItemView* feedback_menu_item_;

  // Used for managing "Recent tabs" menu items.
  scoped_ptr<RecentTabsMenuModelDelegate> recent_tabs_menu_model_delegate_;

  content::NotificationRegistrar registrar_;

  // The bit mask of RunFlags.
  const int run_flags_;

  ObserverList<WrenchMenuObserver> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(WrenchMenu);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WRENCH_MENU_H_
