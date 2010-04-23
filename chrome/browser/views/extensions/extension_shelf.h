// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_EXTENSIONS_EXTENSION_SHELF_H_
#define CHROME_BROWSER_VIEWS_EXTENSIONS_EXTENSION_SHELF_H_

#include "app/slide_animation.h"
#include "base/task.h"
#include "chrome/browser/extensions/extension_shelf_model.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/views/browser_bubble.h"
#include "chrome/browser/views/detachable_toolbar_view.h"
#include "gfx/canvas.h"
#include "views/view.h"

class Browser;
namespace views {
  class Label;
  class MouseEvent;
}

// A shelf that contains Extension toolstrips.
class ExtensionShelf : public DetachableToolbarView,
                       public ExtensionView::Container,
                       public ExtensionShelfModelObserver,
                       public AnimationDelegate,
                       public NotificationObserver {
 public:
  explicit ExtensionShelf(Browser* browser);
  virtual ~ExtensionShelf();

  // Get the current model.
  ExtensionShelfModel* model() { return model_; }

  // Toggles a preference for whether to always show the extension shelf.
  static void ToggleWhenExtensionShelfVisible(Profile* profile);

  int top_margin() { return top_margin_; }

  // DetachableToolbarView methods:
  virtual bool IsDetached() const;
  virtual double GetAnimationValue() const {
    return size_animation_->GetCurrentValue();
  }

  // View methods:
  virtual void PaintChildren(gfx::Canvas* canvas);
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void OnMouseExited(const views::MouseEvent& event);
  virtual void OnMouseEntered(const views::MouseEvent& event);
  virtual bool GetAccessibleRole(AccessibilityTypes::Role* role);
  virtual void ThemeChanged();

  // ExtensionContainer methods:
  virtual void OnExtensionMouseEvent(ExtensionView* view);
  virtual void OnExtensionMouseLeave(ExtensionView* view);

  // ExtensionShelfModelObserver methods:
  virtual void ToolstripInsertedAt(ExtensionHost* toolstrip, int index);
  virtual void ToolstripRemovingAt(ExtensionHost* toolstrip, int index);
  virtual void ToolstripDraggingFrom(ExtensionHost* toolstrip, int index);
  virtual void ToolstripMoved(ExtensionHost* toolstrip,
                              int from_index,
                              int to_index);
  virtual void ToolstripChanged(ExtensionShelfModel::iterator toolstrip);
  virtual void ExtensionShelfEmpty();
  virtual void ShelfModelReloaded();
  virtual void ShelfModelDeleting();

  // AnimationDelegate methods:
  virtual void AnimationProgressed(const Animation* animation);
  virtual void AnimationEnded(const Animation* animation);

  // NotificationObserver methods:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Toggle fullscreen mode.
  void OnFullscreenToggled(bool fullscreen);

 protected:
  // View methods:
  virtual void ChildPreferredSizeChanged(View* child);

 private:
  class Toolstrip;
  friend class Toolstrip;
  class PlaceholderView;

  // Dragging toolstrips
  void DropExtension(Toolstrip* handle, const gfx::Point& pt, bool cancel);

  // Expand the specified toolstrip, navigating to |url| if non-empty,
  // and setting the |height|.
  void ExpandToolstrip(ExtensionHost* host, const GURL& url, int height);

  // Collapse the specified toolstrip, navigating to |url| if non-empty.
  void CollapseToolstrip(ExtensionHost* host, const GURL& url);

  // Initializes the background bitmaps for all views.
  void InitBackground(gfx::Canvas* canvas);

  // Returns the Toolstrip at |x| coordinate.  If |x| is out of bounds, returns
  // NULL.
  Toolstrip* ToolstripAtX(int x);

  // Returns the Toolstrip at |index|.
  Toolstrip* ToolstripAtIndex(int index);

  // Returns the toolstrip associated with |view|.
  Toolstrip* ToolstripForView(ExtensionView* view);

  // Loads initial state from |model_|.
  void LoadFromModel();

  // This method computes the bounds for the extension shelf items. If
  // |compute_bounds_only| = TRUE, the bounds for the items are just computed,
  // but are not set. This mode is used by GetPreferredSize() to obtain the
  // desired bounds. If |compute_bounds_only| = FALSE, the bounds are set.
  gfx::Size LayoutItems(bool compute_bounds_only);

  // Returns whether the extension shelf always shown (checks pref value).
  bool IsAlwaysShown() const;

  // Returns whether the extension shelf is being displayed over the new tab
  // page.
  bool OnNewTabPage() const;

  int top_margin_;

  NotificationRegistrar registrar_;

  // Background bitmap to draw under extension views.
  bool background_needs_repaint_;

  // The browser this extension shelf belongs to.
  Browser* browser_;

  // The model representing the toolstrips on the shelf.
  ExtensionShelfModel* model_;

  // Animation controlling showing and hiding of the shelf.
  scoped_ptr<SlideAnimation> size_animation_;

  // Are we in fullscreen mode or not.
  bool fullscreen_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionShelf);
};

#endif  // CHROME_BROWSER_VIEWS_EXTENSIONS_EXTENSION_SHELF_H_
