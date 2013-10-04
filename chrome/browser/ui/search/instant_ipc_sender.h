// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_INSTANT_IPC_SENDER_H_
#define CHROME_BROWSER_UI_SEARCH_INSTANT_IPC_SENDER_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/common/omnibox_focus_state.h"
#include "content/public/browser/web_contents_observer.h"

namespace gfx {
class Rect;
}

namespace IPC {
class Sender;
}

class InstantIPCSender : public content::WebContentsObserver {
 public:
  // Creates a new instance of InstantIPCSender. If |is_incognito| is true,
  // the instance will only send appropriate IPCs for incognito profiles.
  static scoped_ptr<InstantIPCSender> Create(bool is_incognito);

  virtual ~InstantIPCSender() {}

  // Sets |web_contents| as the receiver of IPCs.
  void SetContents(content::WebContents* web_contents);


  // Tells the page the left and right margins of the omnibox. This is used by
  // the page to align text or assets properly with the omnibox.
  virtual void SetOmniboxBounds(const gfx::Rect& bounds) {}

  // Tells the page that the omnibox focus has changed.
  virtual void FocusChanged(OmniboxFocusState state,
                            OmniboxFocusChangeReason reason) {}

  // Tells the page that user input started or stopped.
  virtual void SetInputInProgress(bool input_in_progress) {}

  // Tells the page to toggle voice search.
  virtual void ToggleVoiceSearch() {}

 protected:
  InstantIPCSender() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(InstantIPCSender);
};

#endif  // CHROME_BROWSER_UI_SEARCH_INSTANT_IPC_SENDER_H_
