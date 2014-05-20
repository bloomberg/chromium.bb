// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FIND_BAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FIND_BAR_VIEW_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/views/dropdown_bar_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"

class FindBarHost;
class FindNotificationDetails;

namespace views {
class ImageButton;
class Label;
class MouseEvent;
class Painter;
}

////////////////////////////////////////////////////////////////////////////////
//
// The FindBarView is responsible for drawing the UI controls of the
// FindBar, the find text box, the 'Find' button and the 'Close'
// button. It communicates the user search words to the FindBarHost.
//
////////////////////////////////////////////////////////////////////////////////
class FindBarView : public DropdownBarView,
                    public views::ButtonListener,
                    public views::TextfieldController {
 public:
  // A tag denoting which button the user pressed.
  enum ButtonTag {
    FIND_PREVIOUS_TAG = 0,  // The Find Previous button.
    FIND_NEXT_TAG,          // The Find Next button.
    CLOSE_TAG,              // The Close button (the 'X').
  };

  explicit FindBarView(FindBarHost* host);
  virtual ~FindBarView();

  // Accessors for the text and selection displayed in the text box.
  void SetFindTextAndSelectedRange(const base::string16& find_text,
                                   const gfx::Range& selected_range);
  base::string16 GetFindText() const;
  gfx::Range GetSelectedRange() const;

  // Gets the selected text in the text box.
  base::string16 GetFindSelectedText() const;

  // Gets the match count text displayed in the text box.
  base::string16 GetMatchCountText() const;

  // Updates the label inside the Find text box that shows the ordinal of the
  // active item and how many matches were found.
  void UpdateForResult(const FindNotificationDetails& result,
                       const base::string16& find_text);

  // Clears the current Match Count value in the Find text box.
  void ClearMatchCount();

  // Claims focus for the text field and selects its contents.
  virtual void SetFocusAndSelection(bool select_all) OVERRIDE;

  // views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual gfx::Size GetPreferredSize() const OVERRIDE;

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // views::TextfieldController:
  virtual bool HandleKeyEvent(views::Textfield* sender,
                              const ui::KeyEvent& key_event) OVERRIDE;
  virtual void OnAfterUserAction(views::Textfield* sender) OVERRIDE;
  virtual void OnAfterPaste() OVERRIDE;

 private:
  // Starts finding |search_text|.  If the text is empty, stops finding.
  void Find(const base::string16& search_text);

  // Updates the appearance for the match count label.
  void UpdateMatchCountAppearance(bool no_match);

  // views::View:
  virtual void OnThemeChanged() OVERRIDE;

  // We use a hidden view to grab mouse clicks and bring focus to the find
  // text box. This is because although the find text box may look like it
  // extends all the way to the find button, it only goes as far as to the
  // match_count label. The user, however, expects being able to click anywhere
  // inside what looks like the find text box (including on or around the
  // match_count label) and have focus brought to the find box.
  class FocusForwarderView : public views::View {
   public:
    explicit FocusForwarderView(
        views::Textfield* view_to_focus_on_mousedown)
      : view_to_focus_on_mousedown_(view_to_focus_on_mousedown) {}

   private:
    virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;

    views::Textfield* view_to_focus_on_mousedown_;

    DISALLOW_COPY_AND_ASSIGN(FocusForwarderView);
  };

  // Returns the OS-specific view for the find bar that acts as an intermediary
  // between us and the WebContentsView.
  FindBarHost* find_bar_host() const;

  // Used to detect if the input text, not including the IME composition text,
  // has changed or not.
  base::string16 last_searched_text_;

  // The controls in the window.
  views::Textfield* find_text_;
  scoped_ptr<views::Painter> find_text_border_;
  views::Label* match_count_text_;
  FocusForwarderView* focus_forwarder_view_;
  views::ImageButton* find_previous_button_;
  views::ImageButton* find_next_button_;
  views::ImageButton* close_button_;

  // The preferred height of the find bar.
  int preferred_height_;

  DISALLOW_COPY_AND_ASSIGN(FindBarView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FIND_BAR_VIEW_H_
