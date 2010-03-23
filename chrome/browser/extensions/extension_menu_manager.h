// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_MENU_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_MENU_MANAGER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/linked_ptr.h"
#include "base/stl_util-inl.h"
#include "base/string16.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

struct ContextMenuParams;

class ExtensionMessageService;
class Profile;
class TabContents;

// Represents a menu item added by an extension.
class ExtensionMenuItem {
 public:
  // A list of owned ExtensionMenuItem's.
  typedef std::vector<linked_ptr<ExtensionMenuItem> > List;

  // For context menus, these are the contexts where an item can appear and
  // potentially be enabled.
  enum Context {
    ALL = 1,
    PAGE = 2,
    SELECTION = 4,
    LINK = 8,
    EDITABLE = 16,
    IMAGE = 32,
    VIDEO = 64,
    AUDIO = 128,
  };

  // An item can be only one of these types.
  enum Type {
    NORMAL,
    CHECKBOX,
    RADIO,
    SEPARATOR
  };

  // A list of Contexts for an item (where it should be shown/enabled).
  class ContextList {
   public:
    ContextList() : value_(0) {}
    explicit ContextList(Context context) : value_(context) {}
    ContextList(const ContextList& other) : value_(other.value_) {}

    void operator=(const ContextList& other) {
      value_ = other.value_;
    }

    bool Contains(Context context) const {
      return (value_ & context) > 0;
    }

    void Add(Context context) {
      value_ |= context;
    }

   private:
    uint32 value_;  // A bitmask of Context values.
  };

  ExtensionMenuItem(const std::string& extension_id, std::string title,
                    bool checked, Type type, const ContextList& contexts,
                    const ContextList& enabled_contexts);
  virtual ~ExtensionMenuItem();

  // Simple accessor methods.
  const std::string& extension_id() const { return extension_id_; }
  const std::string& title() const { return title_; }
  int id() const { return id_; }
  int parent_id() const { return parent_id_; }
  int child_count() const { return children_.size(); }
  ContextList contexts() const { return contexts_; }
  ContextList enabled_contexts() const { return enabled_contexts_; }
  Type type() const { return type_; }
  bool checked() const { return checked_; }

  // Returns the child at the given index, or NULL.
  ExtensionMenuItem* ChildAt(int index) const;

  // Returns the title with any instances of %s replaced by |selection|.
  string16 TitleWithReplacement(const string16& selection) const;

 protected:
  friend class ExtensionMenuManager;

  // This is protected because the ExtensionMenuManager is in charge of
  // assigning unique ids.
  virtual void set_id(int id) {
    id_ = id;
  }

  // Provides direct access to the children of this item.
  List* children() { return &children_; }

  // Set the checked state to |checked|. Returns true if successful.
  bool SetChecked(bool checked);

  // Takes ownership of |item| and sets its parent_id_.
  void AddChild(ExtensionMenuItem* item);

  // Removes child menu item with the given id, returning true if the item was
  // found and removed, or false otherwise.
  bool RemoveChild(int child_id);

 private:
  // The extension that added this item.
  std::string extension_id_;

  // What gets shown in the menu for this item.
  std::string title_;

  // A unique id for this item. The value 0 means "not yet assigned".
  int id_;

  Type type_;

  // This should only be true for items of type CHECKBOX or RADIO.
  bool checked_;

  // In what contexts should the item be shown?
  ContextList contexts_;

  // In what contexts should the item be enabled (i.e. not greyed out). This
  // should be a subset of contexts_.
  ContextList enabled_contexts_;

  // If this item is a child of another item, the unique id of its parent. If
  // this is a top-level item with no parent, this will be 0.
  int parent_id_;

  // Any children this item may have.
  List children_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMenuItem);
};

// This class keeps track of menu items added by extensions.
class ExtensionMenuManager : public NotificationObserver {
 public:
  ExtensionMenuManager();
  virtual ~ExtensionMenuManager();

  // Returns the ids of extensions which have menu items registered.
  std::set<std::string> ExtensionIds();

  // Returns a list of all the *top-level* menu items (added via AddContextItem)
  // for the given extension id, *not* including child items (added via
  // AddChildItem); although those can be reached via the top-level items'
  // ChildAt function. A view can then decide how to display these, including
  // whether to put them into a submenu if there are more than 1.
  std::vector<const ExtensionMenuItem*> MenuItems(
      const std::string& extension_id);

  // Takes ownership of |item|. Returns the id assigned to the item. Has the
  // side-effect of incrementing the next_item_id_ member.
  int AddContextItem(ExtensionMenuItem* item);

  // Add an item as a child of another item which has been previously added, and
  // takes ownership of |item|. Returns the id assigned to the item, or 0 on
  // error.  Has the side-effect of incrementing the next_item_id_ member.
  int AddChildItem(int parent_id, ExtensionMenuItem* child);

  // Removes a context menu item with the given id (whether it is a top-level
  // item or a child of some other item), returning true if the item was found
  // and removed or false otherwise.
  bool RemoveContextMenuItem(int id);

  // Returns the item with the given |id| or NULL.
  ExtensionMenuItem* GetItemById(int id);

  // Called when a menu item is clicked on by the user.
  void ExecuteCommand(Profile* profile, TabContents* tab_contents,
                      const ContextMenuParams& params,
                      int menuItemId);

  // Implements the NotificationObserver interface.
  virtual void Observe(NotificationType type, const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  // This is a helper function which takes care of de-selecting any other radio
  // items in the same group (i.e. that are adjacent in the list).
  void RadioItemSelected(ExtensionMenuItem* item);

  // If an item with |id| is found, |item| will be set to point to it and
  // |index| will be set to its index within the containing list.
  void GetItemAndIndex(int id, ExtensionMenuItem** item, size_t* index);

  // We keep items organized by mapping an extension id to a list of items.
  typedef std::map<std::string, ExtensionMenuItem::List> MenuItemMap;
  MenuItemMap context_items_;

  // This lets us make lookup by id fast. It maps id to ExtensionMenuItem* for
  // all items the menu manager knows about, including all children of top-level
  // items.
  std::map<int, ExtensionMenuItem*> items_by_id_;

  // The id we will assign to the next item that gets added.
  int next_item_id_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMenuManager);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_MENU_MANAGER_H_
