// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/infobars/infobar_container_gtk.h"

#include <gtk/gtk.h>

#include <utility>

#include "base/message_loop.h"
#include "chrome/browser/infobars/infobar_delegate.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/infobars/infobar_gtk.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/gtk/gtk_compat.h"
#include "ui/gfx/canvas_skia_paint.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/skia_utils_gtk.h"

InfoBarContainerGtk::InfoBarContainerGtk(InfoBarContainer::Delegate* delegate,
                                         SearchModel* search_model,
                                         Profile* profile)
    : InfoBarContainer(delegate, search_model),
      profile_(profile),
      container_(gtk_vbox_new(FALSE, 0)) {
  gtk_widget_show(widget());
}

InfoBarContainerGtk::~InfoBarContainerGtk() {
  RemoveAllInfoBarsForDestruction();
  container_.Destroy();
}

int InfoBarContainerGtk::TotalHeightOfAnimatingBars() const {
  int sum = 0;

  for (std::vector<InfoBarGtk*>::const_iterator it = infobars_gtk_.begin();
       it != infobars_gtk_.end(); ++it) {
    sum += (*it)->AnimatingHeight();
  }

  return sum;
}

bool InfoBarContainerGtk::ContainsInfobars() const {
  return !infobars_gtk_.empty();
}

void InfoBarContainerGtk::PlatformSpecificAddInfoBar(InfoBar* infobar,
                                                     size_t position) {
  InfoBarGtk* infobar_gtk = static_cast<InfoBarGtk*>(infobar);
  infobars_gtk_.insert(infobars_gtk_.begin() + position, infobar_gtk);

  if (infobars_gtk_.back() == infobar_gtk) {
    gtk_box_pack_start(GTK_BOX(widget()), infobar_gtk->widget(),
                       FALSE, FALSE, 0);
  } else {
    // Clear out our container and then repack it to make sure everything is in
    // the right order.
    gtk_util::RemoveAllChildren(widget());

    // Repack our container.
    for (std::vector<InfoBarGtk*>::const_iterator it = infobars_gtk_.begin();
         it != infobars_gtk_.end(); ++it) {
      gtk_box_pack_start(GTK_BOX(widget()), (*it)->widget(),
                         FALSE, FALSE, 0);
    }
  }
}

void InfoBarContainerGtk::PlatformSpecificRemoveInfoBar(InfoBar* infobar) {
  InfoBarGtk* infobar_gtk = static_cast<InfoBarGtk*>(infobar);
  gtk_container_remove(GTK_CONTAINER(widget()), infobar_gtk->widget());

  std::vector<InfoBarGtk*>::iterator it =
      std::find(infobars_gtk_.begin(), infobars_gtk_.end(), infobar);
  if (it != infobars_gtk_.end())
    infobars_gtk_.erase(it);

  MessageLoop::current()->DeleteSoon(FROM_HERE, infobar);
}

void InfoBarContainerGtk::PlatformSpecificInfoBarStateChanged(
    bool is_animating) {
  // Force a redraw of all infobars since something has a new height and we
  // need to make sure we animate our arrows.
  for (std::vector<InfoBarGtk*>::iterator it = infobars_gtk_.begin();
       it != infobars_gtk_.end(); ++it) {
    gtk_widget_queue_draw((*it)->widget());
  }
}

void InfoBarContainerGtk::PaintInfobarBitsOn(GtkWidget* sender,
                                             GdkEventExpose* expose,
                                             InfoBarGtk* infobar) {
  if (infobars_gtk_.empty())
    return;

  // For each infobar after |infobar| (or starting from the beginning if NULL),
  // we draw each every arrow and rely on clipping rects to ignore overdraw.
  std::vector<InfoBarGtk*>::iterator it;
  if (infobar) {
    it = std::find(infobars_gtk_.begin(), infobars_gtk_.end(), infobar);
    if (it == infobars_gtk_.end()) {
      NOTREACHED();
      return;
    }

    it++;
    if (it == infobars_gtk_.end()) {
      // |infobar| is the last infobar in the list and thus doesn't need to
      // paint the next infobar's arrow.
      return;
    }
  } else {
    it = infobars_gtk_.begin();
  }

  // Figure out the x location so that that arrow is over the location item.
  GtkWindow* parent = platform_util::GetTopLevel(sender);
  BrowserWindowGtk* browser_window =
      BrowserWindowGtk::GetBrowserWindowForNativeWindow(parent);
  int x = browser_window ?
      browser_window->GetXPositionOfLocationIcon(sender) : 0;

  for (; it != infobars_gtk_.end(); ++it) {
    // Find the location of the arrow in |sender|'s coordinate space relative
    // to the infobar.
    int y = 0;
    gtk_widget_translate_coordinates((*it)->widget(), sender,
                                     0, 0,
                                     NULL, &y);
    if (!gtk_widget_get_has_window(sender)) {
      GtkAllocation allocation;
      gtk_widget_get_allocation(sender, &allocation);
      y += allocation.y;
    }

    // We rely on the +1 in the y calculation so we hide the bottom of the drawn
    // triangle just right outside the view bounds.
    gfx::Rect bounds(x - (*it)->arrow_half_width(),
                     y - (*it)->arrow_height() + 1,
                     2 * (*it)->arrow_half_width(),
                     (*it)->arrow_target_height());

    PaintArrowOn(sender, expose, bounds, *it);
  }
}

void InfoBarContainerGtk::PaintArrowOn(GtkWidget* widget,
                                       GdkEventExpose* expose,
                                       const gfx::Rect& bounds,
                                       InfoBarGtk* source) {
  // TODO(erg): All of this could be rewritten in cairo.
  SkPath path;
  path.moveTo(bounds.x() + 0.5, bounds.bottom() + 0.5);
  path.rLineTo(bounds.width() / 2.0, -bounds.height());
  path.lineTo(bounds.right() + 0.5, bounds.bottom() + 0.5);
  path.close();

  SkPaint paint;
  paint.setStrokeWidth(1);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setAntiAlias(true);

  SkPoint grad_points[2];
  grad_points[0].set(SkIntToScalar(0), SkIntToScalar(bounds.bottom()));
  grad_points[1].set(SkIntToScalar(0),
                     SkIntToScalar(bounds.bottom() +
                                   source->arrow_target_height()));

  SkColor grad_colors[2];
  grad_colors[0] = source->ConvertGetColor(&InfoBarGtk::GetTopColor);
  grad_colors[1] = source->ConvertGetColor(&InfoBarGtk::GetBottomColor);

  skia::RefPtr<SkShader> gradient_shader = skia::AdoptRef(
      SkGradientShader::CreateLinear(
          grad_points, grad_colors, NULL, 2, SkShader::kMirror_TileMode));
  paint.setShader(gradient_shader.get());

  gfx::CanvasSkiaPaint canvas_paint(expose, false);
  SkCanvas& canvas = *canvas_paint.sk_canvas();

  canvas.drawPath(path, paint);

  paint.setShader(NULL);
  paint.setColor(SkColorSetA(gfx::GdkColorToSkColor(source->GetBorderColor()),
                             SkColorGetA(grad_colors[0])));
  paint.setStyle(SkPaint::kStroke_Style);
  canvas.drawPath(path, paint);
}
