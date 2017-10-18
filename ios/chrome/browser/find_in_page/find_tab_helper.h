// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIND_IN_PAGE_FIND_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_FIND_IN_PAGE_FIND_TAB_HELPER_H_

#include <Foundation/Foundation.h>

#include "base/ios/block_types.h"
#import "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "ios/web/public/web_state/web_state_observer.h"
#import "ios/web/public/web_state/web_state_user_data.h"

@class FindInPageController;
@protocol FindInPageControllerDelegate;
@class FindInPageModel;

typedef void (^FindInPageCompletionBlock)(FindInPageModel*);

// Adds support for the "Find in page" feature.
class FindTabHelper : public web::WebStateObserver,
                      public web::WebStateUserData<FindTabHelper> {
 public:
  ~FindTabHelper() override;

  enum FindDirection {
    FORWARD,
    REVERSE,
  };

  // Creates a FindTabHelper and attaches it to the given |web_state|.
  // |controller_delegate| can be nil.
  static void CreateForWebState(
      web::WebState* web_state,
      id<FindInPageControllerDelegate> controller_delegate);

  // Starts an asynchronous Find operation that will call the given completion
  // handler with results.  Highlights matches on the current page.  Always
  // searches in the FORWARD direction.
  void StartFinding(NSString* search_string,
                    FindInPageCompletionBlock completion);

  // Runs an asynchronous Find operation that will call the given completion
  // handler with results.  Highlights matches on the current page.  Uses the
  // previously remembered search string and searches in the given |direction|.
  void ContinueFinding(FindDirection direction,
                       FindInPageCompletionBlock completion);

  // Stops any running find operations and runs the given completion block.
  // Removes any highlighting from the current page.
  void StopFinding(ProceduralBlock completion);

  // Returns the FindInPageModel that contains the latest find results.
  FindInPageModel* GetFindResult() const;

  // Returns true if the currently loaded page supports Find in Page.
  bool CurrentPageSupportsFindInPage() const;

  // Returns true if the Find in Page UI is currently visible.
  bool IsFindUIActive() const;

  // Marks the Find in Page UI as visible or not.  This method does not directly
  // show or hide the UI.  It simply acts as a marker for whether or not the UI
  // is visible.
  void SetFindUIActive(bool active);

  // Saves the current find text to persistent storage.
  void PersistSearchTerm();

  // Restores the current find text from persistent storage.
  void RestoreSearchTerm();

 private:
  friend class FindTabHelperTest;

  FindTabHelper(web::WebState* web_state,
                id<FindInPageControllerDelegate> controller_delegate);

  // web::WebStateObserver.
  void NavigationItemCommitted(
      web::WebState* web_state,
      const web::LoadCommittedDetails& load_details) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // The ObjC find in page controller.
  base::scoped_nsobject<FindInPageController> controller_;

  DISALLOW_COPY_AND_ASSIGN(FindTabHelper);
};

#endif  // IOS_CHROME_BROWSER_FIND_IN_PAGE_FIND_TAB_HELPER_H_
