// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/infobars/infobar_container_gtk.h"

#include <gtk/gtk.h>

#include <utility>

#include "base/command_line.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/gtk_theme_provider.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/infobars/infobar_gtk.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/infobar_delegate.h"
#include "third_party/skia/include/core/SkPaint.h"

namespace {

static const char* kInfoBar = "info-bar";

// If |infobar_widget| matches |info_bar_delegate|, then close the infobar.
void AnimateClosingForDelegate(GtkWidget* infobar_widget,
                               gpointer info_bar_delegate) {
  InfoBarDelegate* delegate =
      static_cast<InfoBarDelegate*>(info_bar_delegate);
  InfoBar* infobar = reinterpret_cast<InfoBar*>(
      g_object_get_data(G_OBJECT(infobar_widget), kInfoBar));

  if (!infobar) {
    NOTREACHED();
    return;
  }

  if (delegate == infobar->delegate())
    infobar->AnimateClose();
}

// If |infobar_widget| matches |info_bar_delegate|, then close the infobar w/o
// an animation.
void ClosingForDelegate(GtkWidget* infobar_widget, gpointer info_bar_delegate) {
  InfoBarDelegate* delegate =
      static_cast<InfoBarDelegate*>(info_bar_delegate);
  InfoBar* infobar = reinterpret_cast<InfoBar*>(
      g_object_get_data(G_OBJECT(infobar_widget), kInfoBar));

  if (!infobar) {
    NOTREACHED();
    return;
  }

  if (delegate == infobar->delegate())
    infobar->Close();
}

// Get the height of the widget and add it to |userdata|, but only if it is in
// the process of animating.
void SumAnimatingBarHeight(GtkWidget* widget, gpointer userdata) {
  int* height_sum = static_cast<int*>(userdata);
  InfoBar* infobar = reinterpret_cast<InfoBar*>(
      g_object_get_data(G_OBJECT(widget), kInfoBar));
  if (infobar->IsAnimating())
    *height_sum += widget->allocation.height;
}

}  // namespace

// InfoBarContainerGtk, public: ------------------------------------------------

InfoBarContainerGtk::InfoBarContainerGtk(Profile* profile)
    : profile_(profile),
      tab_contents_(NULL),
      container_(gtk_vbox_new(FALSE, 0)) {
  gtk_widget_show(widget());
}

InfoBarContainerGtk::~InfoBarContainerGtk() {
  ChangeTabContents(NULL);

  container_.Destroy();
}

void InfoBarContainerGtk::ChangeTabContents(TabContents* contents) {
  if (tab_contents_)
    registrar_.RemoveAll();

  gtk_util::RemoveAllChildren(widget());
  UpdateToolbarInfoBarState(NULL, false);

  tab_contents_ = contents;
  if (tab_contents_) {
    UpdateInfoBars();
    Source<TabContents> source(tab_contents_);
    registrar_.Add(this, NotificationType::TAB_CONTENTS_INFOBAR_ADDED, source);
    registrar_.Add(this, NotificationType::TAB_CONTENTS_INFOBAR_REMOVED,
                   source);
    registrar_.Add(this, NotificationType::TAB_CONTENTS_INFOBAR_REPLACED,
                   source);
  }
}

void InfoBarContainerGtk::RemoveDelegate(InfoBarDelegate* delegate) {
  tab_contents_->RemoveInfoBar(delegate);
}

int InfoBarContainerGtk::TotalHeightOfAnimatingBars() const {
  int sum = 0;
  gtk_container_foreach(GTK_CONTAINER(widget()), SumAnimatingBarHeight, &sum);
  return sum;
}

// InfoBarContainerGtk, NotificationObserver implementation: -------------------

void InfoBarContainerGtk::Observe(NotificationType type,
                                  const NotificationSource& source,
                                  const NotificationDetails& details) {
  if (type == NotificationType::TAB_CONTENTS_INFOBAR_ADDED) {
    AddInfoBar(Details<InfoBarDelegate>(details).ptr(), true);
  } else if (type == NotificationType::TAB_CONTENTS_INFOBAR_REMOVED) {
    RemoveInfoBar(Details<InfoBarDelegate>(details).ptr(), true);
  } else if (type == NotificationType::TAB_CONTENTS_INFOBAR_REPLACED) {
    std::pair<InfoBarDelegate*, InfoBarDelegate*>* delegates =
        Details<std::pair<InfoBarDelegate*, InfoBarDelegate*> >(details).ptr();

    // By not animating the removal/addition, this appears to be a replace.
    RemoveInfoBar(delegates->first, false);
    AddInfoBar(delegates->second, false);
  } else {
    NOTREACHED();
  }
}

// InfoBarContainerGtk, private: -----------------------------------------------

void InfoBarContainerGtk::UpdateInfoBars() {
  for (size_t i = 0; i < tab_contents_->infobar_count(); ++i) {
    InfoBarDelegate* delegate = tab_contents_->GetInfoBarDelegateAt(i);
    AddInfoBar(delegate, false);
  }
}

void InfoBarContainerGtk::ShowArrowForDelegate(InfoBarDelegate* delegate,
                                               bool animate) {
  GList* children = gtk_container_get_children(GTK_CONTAINER(widget()));
  if (!children)
    return;

  // Iterate through the infobars and find the last one that isn't closing.
  InfoBar* last_bar = NULL;
  InfoBar* this_bar = NULL;
  for (GList* iter = children; iter != NULL; iter = iter->next) {
    this_bar = reinterpret_cast<InfoBar*>(
        g_object_get_data(G_OBJECT(iter->data), kInfoBar));

    if (this_bar->delegate() == delegate)
      break;

    if (!this_bar->IsClosing())
      last_bar = this_bar;

    this_bar = NULL;
  }

  if (last_bar)
    last_bar->ShowArrowFor(this_bar, animate);
  else
    UpdateToolbarInfoBarState(this_bar, animate);

  g_list_free(children);
}

void InfoBarContainerGtk::AddInfoBar(InfoBarDelegate* delegate, bool animate) {
  InfoBar* infobar = delegate->CreateInfoBar();
  infobar->set_container(this);
  infobar->SetThemeProvider(GtkThemeProvider::GetFrom(profile_));
  gtk_box_pack_start(GTK_BOX(widget()), infobar->widget(),
                     FALSE, FALSE, 0);

  if (animate)
    infobar->AnimateOpen();
  else
    infobar->Open();

  ShowArrowForDelegate(delegate, animate);
}

void InfoBarContainerGtk::RemoveInfoBar(InfoBarDelegate* delegate,
                                        bool animate) {
  if (animate) {
    gtk_container_foreach(GTK_CONTAINER(widget()),
                          AnimateClosingForDelegate, delegate);
  } else {
    gtk_container_foreach(GTK_CONTAINER(widget()), ClosingForDelegate,
                          delegate);
  }

  InfoBarDelegate* next_delegate = NULL;
  for (size_t i = 1; i < tab_contents_->infobar_count(); ++i) {
    if (tab_contents_->GetInfoBarDelegateAt(i - 1) == delegate) {
      next_delegate = tab_contents_->GetInfoBarDelegateAt(i);
      break;
    }
  }

  ShowArrowForDelegate(next_delegate, animate);
}

void InfoBarContainerGtk::UpdateToolbarInfoBarState(InfoBar* infobar,
                                                    bool animate) {
  GtkWindow* parent = platform_util::GetTopLevel(widget());
  BrowserWindowGtk* browser_window =
      BrowserWindowGtk::GetBrowserWindowForNativeWindow(parent);
  if (browser_window)
    browser_window->SetInfoBarShowing(infobar, animate);
}
