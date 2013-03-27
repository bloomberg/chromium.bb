// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_INFOBARS_INFOBAR_CONTAINER_GTK_H_
#define CHROME_BROWSER_UI_GTK_INFOBARS_INFOBAR_CONTAINER_GTK_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/infobars/infobar_container.h"
#include "ui/base/gtk/owned_widget_gtk.h"

class InfoBar;
class InfoBarGtk;
class InfoBarDelegate;
class Profile;

namespace gfx {
class Rect;
}

typedef struct _GdkColor GdkColor;
typedef struct _GdkEventExpose GdkEventExpose;
typedef struct _GtkWidget GtkWidget;

// Container that both contains the currently displaying infobars, and does
// drawing of infobar arrows on other widgets.
//
// Due to how X11/GTK+ works, this class owns the methods to draw arrows on top
// of other widgets. Since most bars in the top of the window have their own
// event boxes, we can't just draw over the coordinates in the toplevel window
// as event boxes get their own canvases (and they need to have their own event
// boxes for a mixture of handling mouse events and themeing). And because they
// have their own event boxes and event boxes can't be partially transparent,
// we can't just overlap the widgets.
class InfoBarContainerGtk : public InfoBarContainer {
 public:
  InfoBarContainerGtk(InfoBarContainer::Delegate* delegate,
                      SearchModel* search_model,
                      Profile* profile);
  virtual ~InfoBarContainerGtk();

  // Get the native widget.
  GtkWidget* widget() const { return container_.get(); }

  // Remove the specified InfoBarDelegate from the selected WebContents. This
  // will notify us back and cause us to close the View. This is called from
  // the InfoBar's close button handler.
  void RemoveDelegate(InfoBarDelegate* delegate);

  // Returns the total pixel height of all infobars in this container that
  // are currently animating.
  int TotalHeightOfAnimatingBars() const;

  // True if we are displaying any infobars.
  bool ContainsInfobars() const;

  // Paints parts of infobars that aren't inside the infobar's widget. This
  // method is called with |widget|/|expose| pairs for both infobars and
  // toolbars. All infobars starting from |infobar| (NULL for the first) to the
  // end of the list will be rendered.
  void PaintInfobarBitsOn(GtkWidget* widget,
                          GdkEventExpose* expose,
                          InfoBarGtk* infobar);

 protected:
  // InfoBarContainer:
  virtual void PlatformSpecificAddInfoBar(InfoBar* infobar,
                                          size_t position) OVERRIDE;
  virtual void PlatformSpecificRemoveInfoBar(InfoBar* infobar) OVERRIDE;
  virtual void PlatformSpecificInfoBarStateChanged(bool is_animating) OVERRIDE;

 private:
  // Performs the actual painting of the arrow in an expose event.
  void PaintArrowOn(GtkWidget* widget,
                    GdkEventExpose* expose,
                    const gfx::Rect& bounds,
                    InfoBarGtk* source);

  // The profile for the browser that hosts this InfoBarContainer.
  Profile* profile_;

  // A list of the InfoBarGtk* instances. Used during drawing to determine
  // which InfoBarGtk supplies information about drawing the arrows.
  std::vector<InfoBarGtk*> infobars_gtk_;

  // VBox that holds the info bars.
  ui::OwnedWidgetGtk container_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarContainerGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_INFOBARS_INFOBAR_CONTAINER_GTK_H_
