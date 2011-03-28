// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_FIND_BAR_GTK_H_
#define CHROME_BROWSER_UI_GTK_FIND_BAR_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/gtk/focus_store_gtk.h"
#include "chrome/browser/ui/gtk/owned_widget_gtk.h"
#include "chrome/browser/ui/gtk/slide_animator_gtk.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "ui/gfx/point.h"

class Browser;
class BrowserWindowGtk;
class CustomDrawButton;
class FindBarController;
class GtkThemeService;
class NineBox;
class SlideAnimatorGtk;
class TabContentsContainerGtk;

typedef struct _GtkFloatingContainer GtkFloatingContainer;

// Currently this class contains both a model and a view.  We may want to
// eventually pull out the model specific bits and share with Windows.
class FindBarGtk : public FindBar,
                   public FindBarTesting,
                   public NotificationObserver {
 public:
  explicit FindBarGtk(Browser* browser);
  virtual ~FindBarGtk();

  GtkWidget* widget() const { return slide_widget_->widget(); }

  // Methods from FindBar.
  virtual FindBarController* GetFindBarController() const;
  virtual void SetFindBarController(FindBarController* find_bar_controller);
  virtual void Show(bool animate);
  virtual void Hide(bool animate);
  virtual void SetFocusAndSelection();
  virtual void ClearResults(const FindNotificationDetails& results);
  virtual void StopAnimation();
  virtual void MoveWindowIfNecessary(const gfx::Rect& selection_rect,
                                     bool no_redraw);
  virtual void SetFindText(const string16& find_text);
  virtual void UpdateUIForFindResult(const FindNotificationDetails& result,
                                     const string16& find_text);
  virtual void AudibleAlert();
  virtual bool IsFindBarVisible();
  virtual void RestoreSavedFocus();
  virtual FindBarTesting* GetFindBarTesting();

  // Methods from FindBarTesting.
  virtual bool GetFindBarWindowInfo(gfx::Point* position,
                                    bool* fully_visible);
  virtual string16 GetFindText();
  virtual string16 GetFindSelectedText();
  virtual string16 GetMatchCountText();

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  void InitWidgets();

  // Store the currently focused widget if it is not in the find bar.
  // This should always be called before we claim focus.
  void StoreOutsideFocus();

  // For certain keystrokes, such as up or down, we want to forward the event
  // to the renderer rather than handling it ourselves. Returns true if the
  // key event was forwarded.
  // See similar function in FindBarWin.
  bool MaybeForwardKeyEventToRenderer(GdkEventKey* event);

  // Searches for another occurrence of the entry text, moving forward if
  // |forward_search| is true.
  void FindEntryTextInContents(bool forward_search);

  void UpdateMatchLabelAppearance(bool failure);

  // Asynchronously repositions the dialog.
  void Reposition();

  // Returns the rectangle representing where to position the find bar. If
  // |avoid_overlapping_rect| is specified, the return value will be a rectangle
  // located immediately to the left of |avoid_overlapping_rect|, as long as
  // there is enough room for the dialog to draw within the bounds. If not, the
  // dialog position returned will overlap |avoid_overlapping_rect|.
  // Note: |avoid_overlapping_rect| is expected to use coordinates relative to
  // the top of the page area, (it will be converted to coordinates relative to
  // the top of the browser window, when comparing against the dialog
  // coordinates). The returned value is relative to the browser window.
  gfx::Rect GetDialogPosition(gfx::Rect avoid_overlapping_rect);

  // Adjust the text alignment according to the text direction of the widget
  // and |text_entry_|'s content, to make sure the real text alignment is
  // always in sync with the UI language direction.
  void AdjustTextAlignment();

  // Get the position of the findbar within the floating container.
  gfx::Point GetPosition();

  static void OnParentSet(GtkWidget* widget, GtkObject* old_parent,
                          FindBarGtk* find_bar);

  static void OnSetFloatingPosition(GtkFloatingContainer* floating_container,
                                    GtkAllocation* allocation,
                                    FindBarGtk* find_bar);

  // Callback when the entry text changes.
  static gboolean OnChanged(GtkWindow* window, FindBarGtk* find_bar);

  static gboolean OnKeyPressEvent(GtkWidget* widget, GdkEventKey* event,
                                  FindBarGtk* find_bar);
  static gboolean OnKeyReleaseEvent(GtkWidget* widget, GdkEventKey* event,
                                    FindBarGtk* find_bar);

  // Callback for previous, next, and close button.
  static void OnClicked(GtkWidget* button, FindBarGtk* find_bar);

  // Handles shapping and drawing the find bar background.
  static gboolean OnExpose(GtkWidget* widget, GdkEventExpose* event,
                           FindBarGtk* bar);

  // Expose that draws the text entry background in GTK mode.
  static gboolean OnContentEventBoxExpose(GtkWidget* widget,
                                          GdkEventExpose* event,
                                          FindBarGtk* bar);

  // These are both used for focus management.
  static gboolean OnFocus(GtkWidget* text_entry, GtkDirectionType focus,
                          FindBarGtk* find_bar);
  static gboolean OnButtonPress(GtkWidget* text_entry, GdkEventButton* e,
                                FindBarGtk* find_bar);

  // Forwards ctrl-Home/End key bindings to the renderer.
  static void OnMoveCursor(GtkEntry* entry, GtkMovementStep step, gint count,
                           gboolean selection, FindBarGtk* bar);

  // Handles Enter key.
  static void OnActivate(GtkEntry* entry, FindBarGtk* bar);

  static void OnWidgetDirectionChanged(GtkWidget* widget,
                                       GtkTextDirection previous_direction,
                                       FindBarGtk* find_bar) {
    find_bar->AdjustTextAlignment();
  }

  static void OnKeymapDirectionChanged(GdkKeymap* keymap,
                                       FindBarGtk* find_bar) {
    find_bar->AdjustTextAlignment();
  }

  static gboolean OnFocusIn(GtkWidget* entry, GdkEventFocus* event,
                            FindBarGtk* find_bar);

  static gboolean OnFocusOut(GtkWidget* entry, GdkEventFocus* event,
                             FindBarGtk* find_bar);

  Browser* browser_;
  BrowserWindowGtk* window_;

  // Provides colors and information about GTK.
  GtkThemeService* theme_service_;

  // The widget that animates the slide-in and -out of the findbar.
  scoped_ptr<SlideAnimatorGtk> slide_widget_;

  // A GtkAlignment that is the child of |slide_widget_|.
  GtkWidget* container_;

  // Cached allocation of |container_|. We keep this on hand so that we can
  // reset the widget's shape when the width/height change.
  int container_width_;
  int container_height_;

  // The widget where text is entered.
  GtkWidget* text_entry_;

  // An event box and alignment that wrap the entry area and the count label.
  GtkWidget* content_event_box_;
  GtkWidget* content_alignment_;

  // The border around the text entry area.
  GtkWidget* border_bin_;
  GtkWidget* border_bin_alignment_;

  // The next and previous match buttons.
  scoped_ptr<CustomDrawButton> find_previous_button_;
  scoped_ptr<CustomDrawButton> find_next_button_;

  // The GtkLabel listing how many results were found.
  GtkWidget* match_count_label_;
  GtkWidget* match_count_event_box_;
  // Cache whether the match count label is showing failure or not so that
  // we can update its appearance without changing its semantics.
  bool match_label_failure_;

  // The X to close the find bar.
  scoped_ptr<CustomDrawButton> close_button_;

  // The last matchcount number we reported to the user.
  int last_reported_matchcount_;

  // Pointer back to the owning controller.
  FindBarController* find_bar_controller_;

  // Saves where the focus used to be whenever we get it.
  FocusStoreGtk focus_store_;

  // If true, the change signal for the text entry is ignored.
  bool ignore_changed_signal_;

  // This is the width of widget(). We cache it so we can recognize whether
  // allocate signals have changed it, and if so take appropriate actions.
  int current_fixed_width_;

  scoped_ptr<NineBox> dialog_background_;

  // The selection rect we are currently showing. We cache it to avoid covering
  // it up.
  gfx::Rect selection_rect_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(FindBarGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_FIND_BAR_GTK_H_
