// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "extensions/common/extension_icon_set.h"
#include "third_party/skia/include/core/SkColor.h"
// TODO(robertphillips): change this to "class SkBaseDevice;"
#include "third_party/skia/include/core/SkDevice.h"
#include "ui/gfx/animation/linear_animation.h"

class GURL;
class SkBitmap;

namespace gfx {
class Canvas;
class Image;
class ImageSkia;
class Rect;
class Size;
}

// ExtensionAction encapsulates the state of a browser action or page action.
// Instances can have both global and per-tab state. If a property does not have
// a per-tab value, the global value is used instead.
class ExtensionAction {
 public:
  // The action that the UI should take after the ExtensionAction is clicked.
  enum ShowAction {
    ACTION_NONE,
    ACTION_SHOW_POPUP,
    // We don't need a SHOW_CONTEXT_MENU because that's handled separately in
    // the UI.
  };

  // Use this ID to indicate the default state for properties that take a tab_id
  // parameter.
  static const int kDefaultTabId;

  // Max size (both dimensions) for page actions.
  static const int kPageActionIconMaxSize;

  ExtensionAction(const std::string& extension_id,
                  extensions::ActionInfo::Type action_type,
                  const extensions::ActionInfo& manifest_data);
  ~ExtensionAction();

  // Gets a copy of this, ownership passed to caller.
  // It doesn't make sense to copy of an ExtensionAction except in tests.
  scoped_ptr<ExtensionAction> CopyForTest() const;

  // Given the extension action type, returns the size the extension action icon
  // should have. The icon should be square, so only one dimension is
  // returned.
  static int GetIconSizeForType(extensions::ActionInfo::Type type);

  // extension id
  const std::string& extension_id() const { return extension_id_; }

  // What kind of action is this?
  extensions::ActionInfo::Type action_type() const {
    return action_type_;
  }

  // action id -- only used with legacy page actions API
  std::string id() const { return id_; }
  void set_id(const std::string& id) { id_ = id; }

  // Set the url which the popup will load when the user clicks this action's
  // icon.  Setting an empty URL will disable the popup for a given tab.
  void SetPopupUrl(int tab_id, const GURL& url);

  // Use HasPopup() to see if a popup should be displayed.
  bool HasPopup(int tab_id) const;

  // Get the URL to display in a popup.
  GURL GetPopupUrl(int tab_id) const;

  // Set this action's title on a specific tab.
  void SetTitle(int tab_id, const std::string& title) {
    SetValue(&title_, tab_id, title);
  }

  // If tab |tab_id| has a set title, return it.  Otherwise, return
  // the default title.
  std::string GetTitle(int tab_id) const { return GetValue(&title_, tab_id); }

  // Icons are a bit different because the default value can be set to either a
  // bitmap or a path. However, conceptually, there is only one default icon.
  // Setting the default icon using a path clears the bitmap and vice-versa.
  // To retrieve the icon for the extension action, use
  // ExtensionActionIconFactory.

  // Set this action's icon bitmap on a specific tab.
  void SetIcon(int tab_id, const gfx::Image& image);

  // Tries to parse |*icon| from a dictionary {"19": imageData19, "38":
  // imageData38}, returning false if a value is corrupt.
  static bool ParseIconFromCanvasDictionary(const base::DictionaryValue& dict,
                                            gfx::ImageSkia* icon);

  // Gets the icon that has been set using |SetIcon| for the tab.
  gfx::ImageSkia GetExplicitlySetIcon(int tab_id) const;

  // Sets the icon for a tab, in a way that can't be read by the extension's
  // javascript.  Multiple icons can be set at the same time; some icon with the
  // highest priority will be used.
  void DeclarativeSetIcon(int tab_id, int priority, const gfx::Image& icon);
  void UndoDeclarativeSetIcon(int tab_id, int priority, const gfx::Image& icon);

  // Non-tab-specific icon path. This is used to support the default_icon key of
  // page and browser actions.
  void set_default_icon(scoped_ptr<ExtensionIconSet> icon_set) {
     default_icon_ = icon_set.Pass();
  }

  const ExtensionIconSet* default_icon() const {
    return default_icon_.get();
  }

  // Set this action's badge text on a specific tab.
  void SetBadgeText(int tab_id, const std::string& text) {
    SetValue(&badge_text_, tab_id, text);
  }
  // Get the badge text for a tab, or the default if no badge text was set.
  std::string GetBadgeText(int tab_id) const {
    return GetValue(&badge_text_, tab_id);
  }

  // Set this action's badge text color on a specific tab.
  void SetBadgeTextColor(int tab_id, SkColor text_color) {
    SetValue(&badge_text_color_, tab_id, text_color);
  }
  // Get the text color for a tab, or the default color if no text color
  // was set.
  SkColor GetBadgeTextColor(int tab_id) const {
    return GetValue(&badge_text_color_, tab_id);
  }

  // Set this action's badge background color on a specific tab.
  void SetBadgeBackgroundColor(int tab_id, SkColor color) {
    SetValue(&badge_background_color_, tab_id, color);
  }
  // Get the badge background color for a tab, or the default if no color
  // was set.
  SkColor GetBadgeBackgroundColor(int tab_id) const {
    return GetValue(&badge_background_color_, tab_id);
  }

  // Set this action's badge visibility on a specific tab.  Returns true if
  // the visibility has changed.
  bool SetIsVisible(int tab_id, bool value);
  // The declarative appearance overrides a default appearance but is overridden
  // by an appearance set directly on the tab.
  void DeclarativeShow(int tab_id);
  void UndoDeclarativeShow(int tab_id);
  const gfx::ImageSkia GetDeclarativeIcon(int tab_id) const;

  // Get the badge visibility for a tab, or the default badge visibility
  // if none was set.
  // Gets the visibility of |tab_id|.  Returns the first of: a specific
  // visibility set on the tab; a declarative visibility set on the tab; the
  // default visibility set for all tabs; or |false|.  Don't return this
  // result to an extension's background page because the declarative state can
  // leak information about hosts the extension doesn't have permission to
  // access.
  bool GetIsVisible(int tab_id) const {
    if (const bool* tab_is_visible = FindOrNull(&is_visible_, tab_id))
      return *tab_is_visible;

    if (ContainsKey(declarative_show_count_, tab_id))
      return true;

    if (const bool* default_is_visible =
        FindOrNull(&is_visible_, kDefaultTabId))
      return *default_is_visible;

    return false;
  }

  // Remove all tab-specific state.
  void ClearAllValuesForTab(int tab_id);

  // If the specified tab has a badge, paint it into the provided bounds.
  void PaintBadge(gfx::Canvas* canvas, const gfx::Rect& bounds, int tab_id);

  // Returns icon image with badge for specified tab.
  gfx::ImageSkia GetIconWithBadge(const gfx::ImageSkia& icon,
                                  int tab_id,
                                  const gfx::Size& spacing) const;

  // Determine whether or not the ExtensionAction has a value set for the given
  // |tab_id| for each property.
  bool HasPopupUrl(int tab_id) const;
  bool HasTitle(int tab_id) const;
  bool HasBadgeText(int tab_id) const;
  bool HasBadgeBackgroundColor(int tab_id) const;
  bool HasBadgeTextColor(int tab_id) const;
  bool HasIsVisible(int tab_id) const;
  bool HasIcon(int tab_id) const;

 private:
  // Returns width of the current icon for tab_id.
  // TODO(tbarzic): The icon selection is done in ExtensionActionIconFactory.
  // We should probably move this there too.
  int GetIconWidth(int tab_id) const;

  template <class T>
  struct ValueTraits {
    static T CreateEmpty() {
      return T();
    }
  };

  template<class T>
  void SetValue(std::map<int, T>* map, int tab_id, const T& val) {
    (*map)[tab_id] = val;
  }

  template<class Map>
  static const typename Map::mapped_type* FindOrNull(
      const Map* map,
      const typename Map::key_type& key) {
    typename Map::const_iterator iter = map->find(key);
    if (iter == map->end())
      return NULL;
    return &iter->second;
  }

  template<class T>
  T GetValue(const std::map<int, T>* map, int tab_id) const {
    if (const T* tab_value = FindOrNull(map, tab_id)) {
      return *tab_value;
    } else if (const T* default_value = FindOrNull(map, kDefaultTabId)) {
      return *default_value;
    } else {
      return ValueTraits<T>::CreateEmpty();
    }
  }

  // The id for the extension this action belongs to (as defined in the
  // extension manifest).
  const std::string extension_id_;

  const extensions::ActionInfo::Type action_type_;

  // Each of these data items can have both a global state (stored with the key
  // kDefaultTabId), or tab-specific state (stored with the tab_id as the key).
  std::map<int, GURL> popup_url_;
  std::map<int, std::string> title_;
  std::map<int, gfx::ImageSkia> icon_;
  std::map<int, std::string> badge_text_;
  std::map<int, SkColor> badge_background_color_;
  std::map<int, SkColor> badge_text_color_;
  std::map<int, bool> is_visible_;

  // Declarative state exists for two reasons: First, we need to hide it from
  // the extension's background/event page to avoid leaking data from hosts the
  // extension doesn't have permission to access.  Second, the action's state
  // gets both reset and given its declarative values in response to a
  // WebContentsObserver::DidNavigateMainFrame event, and there's no way to set
  // those up to be called in the right order.

  // Maps tab_id to the number of active (applied-but-not-reverted)
  // declarativeContent.ShowPageAction actions.
  std::map<int, int> declarative_show_count_;

  // declarative_icon_[tab_id][declarative_rule_priority] is a vector of icon
  // images that are currently in effect
  std::map<int, std::map<int, std::vector<gfx::Image> > > declarative_icon_;

  // ExtensionIconSet containing paths to bitmaps from which default icon's
  // image representations will be selected.
  scoped_ptr<const ExtensionIconSet> default_icon_;

  // The id for the ExtensionAction, for example: "RssPageAction". This is
  // needed for compat with an older version of the page actions API.
  std::string id_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionAction);
};

template<>
struct ExtensionAction::ValueTraits<int> {
  static int CreateEmpty() {
    return -1;
  }
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_H_
