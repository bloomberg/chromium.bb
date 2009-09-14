// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_INFO_BUBBLE_H_
#define CHROME_BROWSER_VIEWS_INFO_BUBBLE_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include "views/widget/widget_win.h"
#elif defined(OS_LINUX)
#include "views/widget/widget_gtk.h"
#endif

// InfoBubble is used to display an arbitrary view above all other windows.
// Think of InfoBubble as a tooltip that allows you to embed an arbitrary view
// in the tooltip. Additionally the InfoBubble renders an arrow pointing at
// the region the info bubble is providing the information about.
//
// To use an InfoBubble, invoke Show() and it'll take care of the rest.  The
// InfoBubble insets the contents for you, so the contents typically shouldn't
// have any additional margins.

class InfoBubble;

namespace views {
class Window;
}

namespace gfx {
class Path;
}

class InfoBubbleDelegate {
 public:
  // Called when the InfoBubble is closing and is about to be deleted.
  // |closed_by_escape| is true if the close is the result of the user pressing
  // escape.
  virtual void InfoBubbleClosing(InfoBubble* info_bubble,
                                 bool closed_by_escape) = 0;

  // Whether the InfoBubble should be closed when the Esc key is pressed.
  virtual bool CloseOnEscape() = 0;
};

// TODO: this code is ifdef-tastic. It might be cleaner to refactor the
// WidgetFoo subclass into a separate class that calls into InfoBubble.
// That way InfoBubble has no (or very few) ifdefs.
#if defined(OS_WIN)
class InfoBubble : public views::WidgetWin {
#else
class InfoBubble : public views::WidgetGtk {
#endif
 public:
  // Shows the InfoBubble.  |parent| is set as the parent window, |contents| are
  // the contents shown in the bubble, and |position_relative_to| is a rect in
  // screen coordinates at which the InfoBubble will point.  Show() takes
  // ownership of |contents| and deletes the created InfoBubble when another
  // window is activated.  You can explicitly close the bubble by invoking
  // Close().  You may provide an optional |delegate| to be notified when the
  // InfoBubble is closed and/or to prevent the InfoBubble from being closed
  // when the Escape key is pressed (the default behavior).
  static InfoBubble* Show(views::Window* parent,
                          const gfx::Rect& position_relative_to,
                          views::View* content,
                          InfoBubbleDelegate* delegate);

  // Overridden from WidgetWin:
  virtual void Close();

 protected:
 protected:
  // InfoBubble::CreateContentView() creates one of these. ContentView houses
  // the supplied content as its only child view, renders the arrow/border of
  // the bubble and sizes the content.
  class ContentView : public views::View {
   public:
    // Possible edges the arrow is aligned along.
    enum ArrowEdge {
      TOP_LEFT     = 0,
      TOP_RIGHT    = 1,
      BOTTOM_LEFT  = 2,
      BOTTOM_RIGHT = 3
    };

    // Creates the ContentView. The supplied view is added as the only child of
    // the ContentView.
    ContentView(views::View* content, InfoBubble* host);

    virtual ~ContentView() {}

    // Returns the bounds for the window to contain this view.
    //
    // This invokes CalculateWindowBounds, if the returned bounds don't fit on
    // the monitor containing position_relative_to, the arrow edge is adjusted.
    virtual gfx::Rect CalculateWindowBoundsAndAjust(
        const gfx::Rect& position_relative_to);

    // Sets the edge the arrow is rendered at.
    void SetArrowEdge(ArrowEdge arrow_edge) { arrow_edge_ = arrow_edge; }

    // Returns the preferred size, which is the sum of the preferred size of
    // the content and the border/arrow.
    virtual gfx::Size GetPreferredSize();

    // Positions the content relative to the border.
    virtual void Layout();

    // Return the mask for the content view.
    void GetMask(const gfx::Size& size, gfx::Path* mask);

    // Paints the background and arrow appropriately.
    virtual void Paint(gfx::Canvas* canvas);

    // Returns true if the arrow is positioned along the top edge of the
    // view. If this returns false the arrow is positioned along the bottom
    // edge.
    bool IsTop() { return (arrow_edge_ & 2) == 0; }

    // Returns true if the arrow is positioned along the left edge of the
    // view. If this returns false the arrow is positioned along the right edge.
    bool IsLeft() { return (arrow_edge_ & 1) == 0; }

    virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child);

   private:
    // Returns the bounds for the window containing us based on the current
    // arrow edge.
    gfx::Rect CalculateWindowBounds(const gfx::Rect& position_relative_to);

    views::View* content_;

    // Edge to draw the arrow at.
    ArrowEdge arrow_edge_;

    // The bubble we're in.
    InfoBubble* host_;

    DISALLOW_COPY_AND_ASSIGN(ContentView);
  };

  InfoBubble();
  virtual ~InfoBubble() {}

  // Creates the InfoBubble.
  void Init(views::Window* parent,
            const gfx::Rect& position_relative_to,
            views::View* contents);

  // Sets the delegate for that InfoBubble.
  void SetDelegate(InfoBubbleDelegate* delegate) { delegate_ = delegate; }

  // Creates and return a new ContentView containing content.
  virtual ContentView* CreateContentView(views::View* content);

  // Closes the window notifying the delegate. |closed_by_escape| is true if
  // the close is the result of pressing escape.
  void Close(bool closed_by_escape);

#if defined(OS_WIN)
  // Overridden from WidgetWin:
  virtual void OnActivate(UINT action, BOOL minimized, HWND window);
  virtual void OnSize(UINT param, const CSize& size);
#elif defined(OS_LINUX)
  // Overridden from WidgetGtk:
  virtual void OnSizeAllocate(GtkWidget* widget, GtkAllocation* allocation);
#endif

  // Overridden from WidgetWin/WidgetGtk:
  virtual bool AcceleratorPressed(const views::Accelerator& accelerator);

  // The delegate, if any.
  InfoBubbleDelegate* delegate_;

  // The window that this InfoBubble is parented to.
  views::Window* parent_;

  // The content view contained by the infobubble.
  ContentView* content_view_;

  // Have we been closed?
  bool closed_;

  DISALLOW_COPY_AND_ASSIGN(InfoBubble);
};

#endif  // CHROME_BROWSER_VIEWS_INFO_BUBBLE_H_
