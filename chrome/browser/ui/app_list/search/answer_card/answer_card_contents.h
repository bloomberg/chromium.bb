// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_ANSWER_CARD_ANSWER_CARD_CONTENTS_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_ANSWER_CARD_ANSWER_CARD_CONTENTS_H_

#include "base/observer_list.h"

class GURL;

namespace app_list {
class AnswerCardResult;
}

namespace content {
class NavigationHandle;
struct OpenURLParams;
class WebContents;
}

namespace gfx {
class Size;
}

namespace views {
class View;
}

namespace app_list {

// Abstract source of contents for AnswerCardSearchProvider.
class AnswerCardContents {
 public:
  // Delegate handling contents-related events.
  class Delegate {
   public:
    Delegate() {}

    // Events that the delegate processes. They have same meaning as same-named
    // events in WebContentsDelegate and WebContentsObserver, however,
    // unnecessary parameters are omitted.
    virtual void UpdatePreferredSize(const gfx::Size& pref_size) = 0;
    virtual content::WebContents* OpenURLFromTab(
        const content::OpenURLParams& params) = 0;
    virtual void DidFinishNavigation(
        content::NavigationHandle* navigation_handle) = 0;
    virtual void DidStopLoading() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  AnswerCardContents();
  virtual ~AnswerCardContents();

  // Loads contents from |url|.
  virtual void LoadURL(const GURL& url) = 0;
  // Returns whether loading is in progress.
  virtual bool IsLoading() const = 0;
  // Returns the view associated with the contents.
  virtual views::View* GetView() = 0;

  // Sets the delegate to process contents-related events.
  void SetDelegate(Delegate* delegate);
  // Registers a result that will be notified of input events for the view.
  void RegisterResult(AnswerCardResult* result);
  // Unregisters a result.
  void UnregisterResult(AnswerCardResult* result);

 protected:
  Delegate* delegate() const { return delegate_; }
  // Notifies registered results about a mouse event.
  void SetIsMouseInView(bool mouse_is_inside);

 private:
  // Results receiving input events.
  base::ObserverList<AnswerCardResult> results_;
  // Unowned delegate that handles content-related events.
  Delegate* delegate_ = nullptr;
  // Whether the mouse is in |view()|.
  bool mouse_is_in_view_ = false;

  DISALLOW_COPY_AND_ASSIGN(AnswerCardContents);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_ANSWER_CARD_ANSWER_CARD_CONTENTS_H_
