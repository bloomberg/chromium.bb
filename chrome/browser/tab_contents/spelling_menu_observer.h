// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_SPELLING_MENU_OBSERVER_H_
#define CHROME_BROWSER_TAB_CONTENTS_SPELLING_MENU_OBSERVER_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/public/pref_member.h"
#include "base/string16.h"
#include "base/timer.h"
#include "chrome/browser/spellchecker/spelling_service_client.h"
#include "chrome/browser/tab_contents/render_view_context_menu_observer.h"

class RenderViewContextMenuProxy;
struct SpellCheckResult;

// An observer that listens to events from the RenderViewContextMenu class and
// shows suggestions from the Spelling ("do you mean") service to a context menu
// while we show it. This class implements two interfaces:
// * RenderViewContextMenuObserver
//   This interface is used for adding a menu item and update it while showing.
// * net::URLFetcherDelegate
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
class SpellingMenuObserver : public RenderViewContextMenuObserver {
 public:
  explicit SpellingMenuObserver(RenderViewContextMenuProxy* proxy);
  virtual ~SpellingMenuObserver();

  // RenderViewContextMenuObserver implementation.
  virtual void InitMenu(const content::ContextMenuParams& params) OVERRIDE;
  virtual bool IsCommandIdSupported(int command_id) OVERRIDE;
  virtual bool IsCommandIdChecked(int command_id) OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) OVERRIDE;
  virtual void ExecuteCommand(int command_id) OVERRIDE;

  // A callback function called when the Spelling service finishes checking a
  // misspelled word.
  void OnTextCheckComplete(
      SpellingServiceClient::ServiceType type,
      bool success,
      const string16& text,
      const std::vector<SpellCheckResult>& results);

 private:
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
  size_t loading_frame_;

  // A flag represending whether a JSON-RPC call to the Spelling service
  // finished successfully and its response had a suggestion not included in the
  // ones provided by the local spellchecker. When this flag is true, we enable
  // the context-menu item so users can choose it.
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
  scoped_ptr<SpellingServiceClient> client_;

  // A timer used for loading animation.
  base::RepeatingTimer<SpellingMenuObserver> animation_timer_;

  // Flag indicating whether online spelling correction service is enabled. When
  // this variable is true and we right-click a misspelled word, we send a
  // JSON-RPC request to the service and retrieve suggestions.
  BooleanPrefMember integrate_spelling_service_;

  // Flag indicating whether automatic spelling correction is enabled.
  BooleanPrefMember autocorrect_spelling_;

  DISALLOW_COPY_AND_ASSIGN(SpellingMenuObserver);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_SPELLING_MENU_OBSERVER_H_
