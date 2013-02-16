// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_PROMPT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_PROMPT_VIEW_H_

#include "base/basictypes.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/link_listener.h"

class PrefService;

// BookmarkPromptView is a view intended to be used as the content of an
// Bubble. BookmarkPromptView provides views for prompting user for action box
// or star icon. Don't create a BookmarkPromptView directly, instead use
// the static ShowPrompt() method.
class BookmarkPromptView : public views::BubbleDelegateView,
                           public views::LinkListener {
 public:
  // Show the prompt bubble.
  static void ShowPrompt(views::View* anchor_view, PrefService* prefs);

 private:
  // Creates a BookmarkPromptView.
  BookmarkPromptView(views::View* anchor_view, PrefService* prefs);

  virtual ~BookmarkPromptView();

  // views::BubbleDelegateView method.
  virtual void Init() OVERRIDE;

  // views::LinkListener method.
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // views::WidgetDelegate method.
  virtual void WindowClosing() OVERRIDE;

  // The bookmark bubble, if we're showing one.
  static BookmarkPromptView* bookmark_bubble_;

  // Link for dismissing the prompt.
  views::Link* dismiss_link_;

  // The pref service to use disabling bookmark prompt feature.
  PrefService* prefs_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkPromptView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_PROMPT_VIEW_H_
