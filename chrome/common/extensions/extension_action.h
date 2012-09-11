// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_ACTION_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_ACTION_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/animation/linear_animation.h"

class GURL;
class SkBitmap;
class SkDevice;

namespace gfx {
class Canvas;
class Image;
class ImageSkia;
class Rect;
class Size;
}

// ExtensionAction encapsulates the state of a browser action, page action, or
// script badge.
// Instances can have both global and per-tab state. If a property does not have
// a per-tab value, the global value is used instead.
class ExtensionAction {
 public:
  // Use this ID to indicate the default state for properties that take a tab_id
  // parameter.
  static const int kDefaultTabId;

  // The types of extension actions.
  enum Type {
    TYPE_BROWSER,
    TYPE_PAGE,
    TYPE_SCRIPT_BADGE,
  };

  enum Appearance {
    // The action icon is hidden.
    INVISIBLE,
    // The action is trying to get the user's attention but isn't yet
    // running on the page.  Currently only used for script badges.
    WANTS_ATTENTION,
    // The action icon is visible with its normal appearance.
    ACTIVE,
  };

  // A fade-in animation.
  class IconAnimation : public ui::LinearAnimation {
   public:
    // Observes changes to icon animation state.
    class Observer {
     public:
      virtual void OnIconChanged() = 0;

     protected:
      virtual ~Observer() {}
    };

    // A holder for an IconAnimation with a scoped observer.
    class ScopedObserver {
     public:
      ScopedObserver(const base::WeakPtr<IconAnimation>& icon_animation,
                     Observer* observer);
      ~ScopedObserver();

      // Gets the icon animation, or NULL if the reference has expired.
      const IconAnimation* icon_animation() const {
        return icon_animation_.get();
      }

     private:
      base::WeakPtr<IconAnimation> icon_animation_;
      Observer* observer_;

      DISALLOW_COPY_AND_ASSIGN(ScopedObserver);
    };

    virtual ~IconAnimation();

    // Returns the icon derived from the current animation state applied to
    // |icon|. Ownership remains with this.
    const SkBitmap& Apply(const SkBitmap& icon) const;

    void AddObserver(Observer* observer);
    void RemoveObserver(Observer* observer);

   private:
    // Construct using ExtensionAction::RunIconAnimation().
    friend class ExtensionAction;
    IconAnimation();

    base::WeakPtr<IconAnimation> AsWeakPtr();

    // ui::LinearAnimation implementation.
    virtual void AnimateToState(double state) OVERRIDE;

    // Device we use to paint icons to.
    mutable scoped_ptr<SkDevice> device_;

    ObserverList<Observer> observers_;

    base::WeakPtrFactory<IconAnimation> weak_ptr_factory_;

    DISALLOW_COPY_AND_ASSIGN(IconAnimation);
  };

  ExtensionAction(const std::string& extension_id, Type action_type);
  ~ExtensionAction();

  // Gets a copy of this, ownership passed to caller.
  // It doesn't make sense to copy of an ExtensionAction except in tests.
  scoped_ptr<ExtensionAction> CopyForTest() const;

  // extension id
  const std::string& extension_id() const { return extension_id_; }

  // What kind of action is this?
  Type action_type() const { return action_type_; }

  // action id -- only used with legacy page actions API
  std::string id() const { return id_; }
  void set_id(const std::string& id) { id_ = id; }

  // static icon paths from manifest -- only used with legacy page actions API.
  std::vector<std::string>* icon_paths() { return &icon_paths_; }
  const std::vector<std::string>* icon_paths() const { return &icon_paths_; }

  bool has_changed() const { return has_changed_; }
  void set_has_changed(bool value) { has_changed_ = value; }

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

  // Since ExtensionAction, living in common/, can't interact with the browser
  // to load images, the UI code needs to load the images for each path.  For
  // each path in default_icon_path() and icon_paths(), load the image there
  // using an ImageLoadingTracker and call CacheIcon(path, image) with the
  // result.
  //
  // If an image is cached redundantly, the first load will be used.
  void CacheIcon(const std::string& path, const gfx::Image& icon);

  // Set this action's icon bitmap on a specific tab.
  void SetIcon(int tab_id, const gfx::Image& image);

  // Get the icon for a tab, or the default if no icon was set for this tab,
  // retrieving icons that have been specified by path from the previous
  // arguments to CacheIcon().  If the default icon isn't found in the cache,
  // returns the puzzle piece icon.
  gfx::Image GetIcon(int tab_id) const;

  // Gets the icon that has been set using |SetIcon| for the tab.
  gfx::ImageSkia GetExplicitlySetIcon(int tab_id) const;

  // Set this action's icon index for a specific tab.  For use with
  // icon_paths(), only used in page actions.
  void SetIconIndex(int tab_id, int index);

  // Get this action's icon index for a tab, or the default if no icon index
  // was set.
  int GetIconIndex(int tab_id) const {
    return GetValue(&icon_index_, tab_id);
  }

  // Non-tab-specific icon path. This is used to support the default_icon key of
  // page and browser actions.
  void set_default_icon_path(const std::string& path) {
    default_icon_path_ = path;
  }
  const std::string& default_icon_path() const {
    return default_icon_path_;
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

  // Set this action's badge visibility on a specific tab.  This takes
  // care of any appropriate transition animations.  Returns true if
  // the appearance has changed.
  bool SetAppearance(int tab_id, Appearance value);
  // Get the badge visibility for a tab, or the default badge visibility
  // if none was set.
  bool GetIsVisible(int tab_id) const {
    return GetValue(&appearance_, tab_id) != INVISIBLE;
  }

  // Remove all tab-specific state.
  void ClearAllValuesForTab(int tab_id);

  // If the specified tab has a badge, paint it into the provided bounds.
  void PaintBadge(gfx::Canvas* canvas, const gfx::Rect& bounds, int tab_id);

  // Returns icon image with badge for specified tab.
  gfx::ImageSkia GetIconWithBadge(const gfx::ImageSkia& icon,
                                  int tab_id,
                                  const gfx::Size& spacing) const;

  // Gets a weak reference to the icon animation for a tab, if any. The
  // reference will only have a value while the animation is running.
  base::WeakPtr<IconAnimation> GetIconAnimation(int tab_id) const;

 private:
  class IconWithBadgeImageSource;

  // Runs an animation on a tab.
  void RunIconAnimation(int tab_id);

  // If the icon animation is running on tab |tab_id|, applies it to
  // |orig| and returns the result. Otherwise, just returns |orig|.
  gfx::ImageSkia ApplyIconAnimation(int tab_id,
                                    const gfx::ImageSkia& orig) const;

  // Paints badge with specified parameters to |canvas|.
  static void DoPaintBadge(gfx::Canvas* canvas,
                           const gfx::Rect& bounds,
                           const std::string& text,
                           const SkColor& text_color_in,
                           const SkColor& background_color_in,
                           int icon_width);

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

  template<class T>
  T GetValue(const std::map<int, T>* map, int tab_id) const {
    typename std::map<int, T>::const_iterator iter = map->find(tab_id);
    if (iter != map->end()) {
      return iter->second;
    } else {
      iter = map->find(kDefaultTabId);
      return iter != map->end() ? iter->second : ValueTraits<T>::CreateEmpty();
    }
  }

  // The id for the extension this action belongs to (as defined in the
  // extension manifest).
  const std::string extension_id_;

  const Type action_type_;

  // Each of these data items can have both a global state (stored with the key
  // kDefaultTabId), or tab-specific state (stored with the tab_id as the key).
  std::map<int, GURL> popup_url_;
  std::map<int, std::string> title_;
  std::map<int, gfx::ImageSkia> icon_;
  std::map<int, int> icon_index_;  // index into icon_paths_
  std::map<int, std::string> badge_text_;
  std::map<int, SkColor> badge_background_color_;
  std::map<int, SkColor> badge_text_color_;
  std::map<int, Appearance> appearance_;

  // IconAnimations are destroyed by a delayed task on the UI message loop so
  // that even if the Extension and ExtensionAction are destroyed on a non-UI
  // thread, the animation will still only be touched from the UI thread.  This
  // causes the WeakPtr in this map to become NULL.  GetIconAnimation() removes
  // NULLs to prevent the map from growing without bound.
  mutable std::map<int, base::WeakPtr<IconAnimation> > icon_animation_;

  std::string default_icon_path_;

  // The id for the ExtensionAction, for example: "RssPageAction". This is
  // needed for compat with an older version of the page actions API.
  std::string id_;

  // A list of paths to icons this action might show. This is needed to support
  // the legacy setIcon({iconIndex:...} method of the page actions API.
  std::vector<std::string> icon_paths_;

  // Saves the arguments from CacheIcon() calls.
  std::map<std::string, gfx::ImageSkia> path_to_icon_cache_;

  // True if the ExtensionAction's settings have changed from what was
  // specified in the manifest.
  bool has_changed_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionAction);
};

template<>
struct ExtensionAction::ValueTraits<int> {
  static int CreateEmpty() {
    return -1;
  }
};

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_ACTION_H_
