// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_MENU_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_MENU_MANAGER_H_
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/extensions/extension_icon_manager.h"
#include "chrome/common/extensions/extension_extent.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

struct ContextMenuParams;

class Extension;
class Profile;
class SkBitmap;
class TabContents;

// Represents a menu item added by an extension.
class ExtensionMenuItem {
 public:
  // A list of ExtensionMenuItem's.
  typedef std::vector<ExtensionMenuItem*> List;

  // An Id uniquely identifies a context menu item registered by an extension.
  struct Id {
    Id();
    Id(Profile* profile, std::string extension_id, int uid);
    ~Id();

    bool operator==(const Id& other) const;
    bool operator!=(const Id& other) const;
    bool operator<(const Id& other) const;

    Profile* profile;
    std::string extension_id;
    int uid;
  };

  // For context menus, these are the contexts where an item can appear.
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

  // A list of Contexts for an item.
  class ContextList {
   public:
    ContextList() : value_(0) {}
    explicit ContextList(Context context) : value_(context) {}
    ContextList(const ContextList& other) : value_(other.value_) {}

    void operator=(const ContextList& other) {
      value_ = other.value_;
    }

    bool operator==(const ContextList& other) const {
      return value_ == other.value_;
    }

    bool operator!=(const ContextList& other) const {
      return !(*this == other);
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

  ExtensionMenuItem(const Id& id, std::string title, bool checked, Type type,
                    const ContextList& contexts);
  virtual ~ExtensionMenuItem();

  // Simple accessor methods.
  const std::string& extension_id() const { return id_.extension_id; }
  const std::string& title() const { return title_; }
  const List& children() { return children_; }
  const Id& id() const { return id_; }
  Id* parent_id() const { return parent_id_.get(); }
  int child_count() const { return children_.size(); }
  ContextList contexts() const { return contexts_; }
  Type type() const { return type_; }
  bool checked() const { return checked_; }
  const ExtensionExtent& document_url_patterns() const {
    return document_url_patterns_;
  }
  const ExtensionExtent& target_url_patterns() const {
    return target_url_patterns_;
  }

  // Simple mutator methods.
  void set_title(std::string new_title) { title_ = new_title; }
  void set_contexts(ContextList contexts) { contexts_ = contexts; }
  void set_type(Type type) { type_ = type; }
  void set_document_url_patterns(const ExtensionExtent& patterns) {
    document_url_patterns_ = patterns;
  }
  void set_target_url_patterns(const ExtensionExtent& patterns) {
    target_url_patterns_ = patterns;
  }

  // Returns the title with any instances of %s replaced by |selection|. The
  // result will be no longer than |max_length|.
  string16 TitleWithReplacement(const string16& selection,
                                size_t max_length) const;

  // Set the checked state to |checked|. Returns true if successful.
  bool SetChecked(bool checked);

 protected:
  friend class ExtensionMenuManager;

  // Takes ownership of |item| and sets its parent_id_.
  void AddChild(ExtensionMenuItem* item);

  // Takes the child item from this parent. The item is returned and the caller
  // then owns the pointer.
  ExtensionMenuItem* ReleaseChild(const Id& child_id, bool recursive);

  // Recursively removes all descendant items (children, grandchildren, etc.),
  // returning the ids of the removed items.
  std::set<Id> RemoveAllDescendants();

 private:
  // The unique id for this item.
  Id id_;

  // What gets shown in the menu for this item.
  std::string title_;

  Type type_;

  // This should only be true for items of type CHECKBOX or RADIO.
  bool checked_;

  // In what contexts should the item be shown?
  ContextList contexts_;

  // If this item is a child of another item, the unique id of its parent. If
  // this is a top-level item with no parent, this will be NULL.
  scoped_ptr<Id> parent_id_;

  // Patterns for restricting what documents this item will appear for. This
  // applies to the frame where the click took place.
  ExtensionExtent document_url_patterns_;

  // Patterns for restricting where items appear based on the src/href
  // attribute of IMAGE/AUDIO/VIDEO/LINK tags.
  ExtensionExtent target_url_patterns_;

  // Any children this item may have.
  List children_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMenuItem);
};

// This class keeps track of menu items added by extensions.
class ExtensionMenuManager : public NotificationObserver {
 public:
  // A bitmask of values from URLPattern::SchemeMasks indicating the schemes
  // of pages where we'll show extension menu items.
  static const int kAllowedSchemes;

  ExtensionMenuManager();
  virtual ~ExtensionMenuManager();

  // Returns the ids of extensions which have menu items registered.
  std::set<std::string> ExtensionIds();

  // Returns a list of all the *top-level* menu items (added via AddContextItem)
  // for the given extension id, *not* including child items (added via
  // AddChildItem); although those can be reached via the top-level items'
  // children. A view can then decide how to display these, including whether to
  // put them into a submenu if there are more than 1.
  const ExtensionMenuItem::List* MenuItems(const std::string& extension_id);

  // Adds a top-level menu item for an extension, requiring the |extension|
  // pointer so it can load the icon for the extension. Takes ownership of
  // |item|. Returns a boolean indicating success or failure.
  bool AddContextItem(const Extension* extension, ExtensionMenuItem* item);

  // Add an item as a child of another item which has been previously added, and
  // takes ownership of |item|. Returns a boolean indicating success or failure.
  bool AddChildItem(const ExtensionMenuItem::Id& parent_id,
                    ExtensionMenuItem* child);

  // Makes existing item with |child_id| a child of the item with |parent_id|.
  // If the child item was already a child of another parent, this will remove
  // it from that parent first. It is an error to try and move an item to be a
  // child of one of its own descendants. It is legal to pass NULL for
  // |parent_id|, which means the item should be moved to the top-level.
  bool ChangeParent(const ExtensionMenuItem::Id& child_id,
                    const ExtensionMenuItem::Id* parent_id);

  // Removes a context menu item with the given id (whether it is a top-level
  // item or a child of some other item), returning true if the item was found
  // and removed or false otherwise.
  bool RemoveContextMenuItem(const ExtensionMenuItem::Id& id);

  // Removes all items for the given extension id.
  void RemoveAllContextItems(std::string extension_id);

  // Returns the item with the given |id| or NULL.
  ExtensionMenuItem* GetItemById(const ExtensionMenuItem::Id& id) const;

  // Called when a menu item is clicked on by the user.
  void ExecuteCommand(Profile* profile, TabContents* tab_contents,
                      const ContextMenuParams& params,
                      const ExtensionMenuItem::Id& menuItemId);

  // This returns a bitmap of width/height kFaviconSize, loaded either from an
  // entry specified in the extension's 'icon' section of the manifest, or a
  // default extension icon.
  const SkBitmap& GetIconForExtension(const std::string& extension_id);

  // Implements the NotificationObserver interface.
  virtual void Observe(NotificationType type, const NotificationSource& source,
                       const NotificationDetails& details);

  // Returns true if |url| has an allowed scheme for extension context menu
  // items. This checks against kAllowedSchemes.
  static bool HasAllowedScheme(const GURL& url);

 private:
  FRIEND_TEST_ALL_PREFIXES(ExtensionMenuManagerTest, DeleteParent);
  FRIEND_TEST_ALL_PREFIXES(ExtensionMenuManagerTest, RemoveOneByOne);

  // This is a helper function which takes care of de-selecting any other radio
  // items in the same group (i.e. that are adjacent in the list).
  void RadioItemSelected(ExtensionMenuItem* item);

  // Returns true if item is a descendant of an item with id |ancestor_id|.
  bool DescendantOf(ExtensionMenuItem* item,
                    const ExtensionMenuItem::Id& ancestor_id);

  // We keep items organized by mapping an extension id to a list of items.
  typedef std::map<std::string, ExtensionMenuItem::List> MenuItemMap;
  MenuItemMap context_items_;

  // This lets us make lookup by id fast. It maps id to ExtensionMenuItem* for
  // all items the menu manager knows about, including all children of top-level
  // items.
  std::map<ExtensionMenuItem::Id, ExtensionMenuItem*> items_by_id_;

  NotificationRegistrar registrar_;

  ExtensionIconManager icon_manager_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMenuManager);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_MENU_MANAGER_H_
