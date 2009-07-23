// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_EXTENSIONS_EXTENSION_SHELF_H_
#define CHROME_BROWSER_VIEWS_EXTENSIONS_EXTENSION_SHELF_H_

#include "app/gfx/canvas.h"
#include "base/task.h"
#include "chrome/browser/extensions/extension_shelf_model.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/views/browser_bubble.h"
#include "views/view.h"

class Browser;
namespace views {
  class Label;
  class MouseEvent;
}

// A shelf that contains Extension toolstrips.
class ExtensionShelf : public views::View,
                       public ExtensionContainer,
                       public ExtensionShelfModelObserver {
 public:
  explicit ExtensionShelf(Browser* browser);
  virtual ~ExtensionShelf();

  // Get the current model.
  ExtensionShelfModel* model() { return model_.get(); }

  // View
  virtual void Paint(gfx::Canvas* canvas);
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void OnMouseExited(const views::MouseEvent& event);
  virtual void OnMouseEntered(const views::MouseEvent& event);
  virtual bool GetAccessibleName(std::wstring* name);
  virtual bool GetAccessibleRole(AccessibilityTypes::Role* role);
  virtual void SetAccessibleName(const std::wstring& name);

  // ExtensionContainer
  virtual void OnExtensionMouseEvent(ExtensionView* view);
  virtual void OnExtensionMouseLeave(ExtensionView* view);

  // ExtensionShelfModelObserver
  virtual void ToolstripInsertedAt(ExtensionHost* toolstrip, int index);
  virtual void ToolstripRemovingAt(ExtensionHost* toolstrip, int index);
  virtual void ToolstripDraggingFrom(ExtensionHost* toolstrip, int index);
  virtual void ToolstripMoved(ExtensionHost* toolstrip,
                              int from_index,
                              int to_index);
  virtual void ToolstripChangedAt(ExtensionHost* toolstrip, int index);
  virtual void ExtensionShelfEmpty();
  virtual void ShelfModelReloaded();

 protected:
  // View
  virtual void ChildPreferredSizeChanged(View* child);

 private:
  class Toolstrip;
  friend class Toolstrip;
  class PlaceholderView;

  // Dragging toolstrips
  void DropExtension(Toolstrip* handle, const gfx::Point& pt, bool cancel);

  // Inits the background bitmap.
  void InitBackground(gfx::Canvas* canvas, const SkRect& subset);

  // Returns the Toolstrip at |x| coordinate.  If |x| is out of bounds, returns
  // NULL.
  Toolstrip* ToolstripAtX(int x);

  // Returns the Toolstrip at |index|.
  Toolstrip* ToolstripAtIndex(int index);

  // Returns the toolstrip associated with |view|.
  Toolstrip* ToolstripForView(ExtensionView* view);

  // Loads initial state from |model_|.
  void LoadFromModel();

  // Background bitmap to draw under extension views.
  SkBitmap background_;

  // The model representing the toolstrips on the shelf.
  scoped_ptr<ExtensionShelfModel> model_;

  // Storage of strings needed for accessibility.
  std::wstring accessible_name_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionShelf);
};

#endif  // CHROME_BROWSER_VIEWS_EXTENSIONS_EXTENSION_SHELF_H_
