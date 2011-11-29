// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_SPELLING_MENU_OBSERVER_H_
#define CHROME_BROWSER_TAB_CONTENTS_SPELLING_MENU_OBSERVER_H_
#pragma once

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/timer.h"
#include "chrome/browser/tab_contents/render_view_context_menu_observer.h"
#include "content/public/common/url_fetcher_delegate.h"

class RenderViewContextMenuProxy;

namespace net {
class URLRequestContextGetter;
}

// An observer that listens to events from the RenderViewContextMenu class and
// shows suggestions from the Spelling ("do you mean") service to a context menu
// while we show it. This class implements two interfaces:
// * RenderViewContextMenuObserver
//   This interface is used for adding a menu item and update it while showing.
// * content::URLFetcherDelegate
//   This interface is used for sending a JSON_RPC request to the Spelling
//   service and retrieving its response.
// These interfaces allow this class to make a JSON-RPC call to the Spelling
// service in the background and update the context menu while showing. The
// following snippet describes how to add this class to the observer list of the
// RenderViewContextMenu class.
//
//   void RenderViewContextMenu::InitMenu() {
//     spelling_menu_observer_.reset(new SpellingMenuObserver(this));
//     if (spelling_menu_observer_.get())
//       observers_.AddObserver(spelling_menu_observer.get());
//   }
//
class SpellingMenuObserver : public RenderViewContextMenuObserver,
                             public content::URLFetcherDelegate {
 public:
  explicit SpellingMenuObserver(RenderViewContextMenuProxy* proxy);
  virtual ~SpellingMenuObserver();

  // RenderViewContextMenuObserver implementation.
  virtual void InitMenu(const ContextMenuParams& params) OVERRIDE;
  virtual bool IsCommandIdSupported(int command_id) OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) OVERRIDE;
  virtual void ExecuteCommand(int command_id) OVERRIDE;

  // content::URLFetcherDelegate implementation.
  virtual void OnURLFetchComplete(const content::URLFetcher* source) OVERRIDE;

 private:
  // Invokes a JSON-RPC call in the background. This function sends a JSON-RPC
  // request to the Spelling service. Chrome will call OnURLFetchComplete() when
  // it receives a response from the service.
  bool Invoke(const string16& text,
              const std::string& locale,
              net::URLRequestContextGetter* context);

  // Parses the specified response from the Spelling service.
  bool ParseResponse(int code, const std::string& data);

  // The callback function for base::RepeatingTimer<SpellingMenuClient>. This
  // function updates the "loading..." animation in the context-menu item.
  void OnAnimationTimerExpired();

  // The interface to add a context-menu item and update it. This class uses
  // this interface to avoid accesing context-menu items directly.
  RenderViewContextMenuProxy* proxy_;

  // Suggested words from the local spellchecker. If the spelling service
  // returns a word in this list, we hide the context-menu item to prevent
  // showing the same word twice.
  std::vector<string16> suggestions_;

  // The string used for animation until we receive a response from the Spelling
  // service. The current animation just adds periods at the end of this string:
  //   'Loading' -> 'Loading.' -> 'Loading..' -> 'Loading...' (-> 'Loading')
  string16 loading_message_;
  int loading_frame_;

  // A flag represending whether this call finished successfully. This means we
  // receive an empty JSON string or a JSON string that consists of misspelled
  // words. ('spelling_menu_observer.cc' describes its format.)
  bool succeeded_;

  // The misspelled word. When we choose the "Add to dictionary" item, we add
  // this word to the custom-word dictionary.
  string16 misspelled_word_;

  // The string representing the result of this call. This string is a
  // suggestion when this call finished successfully. Otherwise it is error
  // text. Until we receive a response from the Spelling service, this string
  // stores the input string. (Since the Spelling service sends only misspelled
  // words, we replace these misspelled words in the input text with the
  // suggested words to create suggestion text.
  string16 result_;

  // The URLFetcher object used for sending a JSON-RPC request.
  scoped_ptr<content::URLFetcher> fetcher_;

  // A timer used for loading animation.
  base::RepeatingTimer<SpellingMenuObserver> animation_timer_;

  DISALLOW_COPY_AND_ASSIGN(SpellingMenuObserver);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_SPELLING_MENU_OBSERVER_H_
